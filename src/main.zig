//! Zig-owned QuickJS host for Coding Agent applications.
//!
//! Modes of operation:
//!   1) `oh_my_poop <app.js>`           – interpret a JS source file.
//!   2) `oh_my_poop --compile <app.js> -o <out>`  – compile JS to a self-contained binary.
//!   3) (embedded bytecode)             – run an appended bytecode payload directly.
//!
//! Zig owns terminal lifecycle, input parsing, event scheduling, and rendering.
//! The C bridge keeps QuickJS ABI types opaque; JavaScript only registers the
//! declarative `defineApp({ init, onKey, onResize, onTick, view })` contract.

const std = @import("std");
const zz = @import("zigzag");
const keyboard = zz.input.keyboard;

// ── C bridge extern declarations ──────────────────────────────────────
const BridgeHost = opaque {};
const KeyData = struct { name: [*:0]const u8, codepoint: c_int };

extern fn bridge_host_create() ?*BridgeHost;
extern fn bridge_host_destroy(host: *BridgeHost) void;
extern fn bridge_host_eval_module(host: *BridgeHost, source: [*]const u8, source_len: usize, filename: [*:0]const u8) c_int;
extern fn bridge_host_compile_module(host: *BridgeHost, source: [*]const u8, source_len: usize, filename: [*:0]const u8, out: *?[*]u8, out_len: *usize) c_int;
extern fn bridge_host_free_bytecode(host: *BridgeHost, bytecode: [*]u8) void;
extern fn bridge_host_eval_bytecode(host: *BridgeHost, bytecode: [*]const u8, bytecode_len: usize) c_int;
extern fn bridge_host_app_init(host: *BridgeHost, width: c_int, height: c_int) c_int;
extern fn bridge_host_app_tick(host: *BridgeHost, width: c_int, height: c_int) c_int;
extern fn bridge_host_app_resize(host: *BridgeHost, width: c_int, height: c_int) c_int;
extern fn bridge_host_app_key(host: *BridgeHost, key: [*:0]const u8, codepoint: c_int, shift: c_int, alt: c_int, ctrl: c_int) c_int;
extern fn bridge_host_app_view(host: *BridgeHost, width: c_int, height: c_int, out: *?[*:0]const u8, out_len: *usize) c_int;
extern fn bridge_host_free_view(host: *BridgeHost, view: [*:0]const u8) void;
extern fn bridge_host_should_quit(host: *BridgeHost) c_int;
extern fn bridge_host_dump_exception(host: *BridgeHost) void;

// ── Self-contained binary trailer format ──────────────────────────────
// The last 16 bytes of a compiled binary contain:
//   [0..8]  bytecode_size  (little-endian u64)
//   [8..16] magic          "OHMPOOP\x00"
const trailer_magic = "OHMPOOP\x00";
const trailer_len: usize = 16;

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    const exe_name = args.next() orelse "oh_my_poop";

    // ── Check for embedded bytecode first ─────────────────────────────
    if (tryRunEmbedded(init, exe_name)) |_| {
        return;
    } else |_| {
        // No embedded bytecode, continue to CLI parsing.
    }

    // ── Parse CLI arguments ───────────────────────────────────────────
    var compile_mode = false;
    var input_path: ?[]const u8 = null;
    var output_path: ?[]const u8 = null;

    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--compile") or std.mem.eql(u8, arg, "-c")) {
            compile_mode = true;
        } else if (std.mem.eql(u8, arg, "-o") or std.mem.eql(u8, arg, "--output")) {
            output_path = args.next() orelse {
                std.debug.print("-o requires an output path\n", .{});
                return error.InvalidArguments;
            };
        } else if (std.mem.eql(u8, arg, "--help") or std.mem.eql(u8, arg, "-h")) {
            printUsage(exe_name);
            return;
        } else if (arg.len > 0 and arg[0] == '-') {
            std.debug.print("unknown option: {s}\n", .{arg});
            return error.InvalidArguments;
        } else {
            if (input_path != null) {
                std.debug.print("unexpected argument: {s}\n", .{arg});
                return error.InvalidArguments;
            }
            input_path = arg;
        }
    }

    const app_path = input_path orelse {
        printUsage(exe_name);
        return error.InvalidArguments;
    };

    if (compile_mode) {
        try compileApplication(init, app_path, output_path orelse "app", exe_name);
    } else {
        try interpretApplication(init, app_path);
    }
}

fn printUsage(exe_name: []const u8) void {
    std.debug.print(
        \\usage:
        \\  {s} <app.js>                        run a JS application
        \\  {s} --compile <app.js> -o <binary>   compile JS into a standalone binary
        \\  {s} --help                           show this help
        \\
    , .{ exe_name, exe_name, exe_name });
}

// ── Mode 1: interpret JS source ───────────────────────────────────────

fn interpretApplication(init: std.process.Init, app_path: []const u8) !void {
    const source = try std.Io.Dir.cwd().readFileAlloc(init.io, app_path, init.gpa, .limited(16 * 1024 * 1024));
    defer init.gpa.free(source);

    // The C bridge needs a NUL-terminated filename. Args from the iterator
    // are heap-allocated slices without a sentinel, so we add one.
    const filename = try init.gpa.dupeSentinel(u8, app_path, 0);
    defer init.gpa.free(filename);

    const host = bridge_host_create() orelse return error.QuickJsRuntimeInitFailed;
    defer bridge_host_destroy(host);
    try checkHost(host, bridge_host_eval_module(host, source.ptr, source.len, filename.ptr));
    try runApplication(init, host);
}

// ── Mode 2: compile JS to standalone binary ───────────────────────────

fn compileApplication(init: std.process.Init, app_path: []const u8, output_path: []const u8, exe_name: []const u8) !void {
    // 1. Read source
    const source = try std.Io.Dir.cwd().readFileAlloc(init.io, app_path, init.gpa, .limited(16 * 1024 * 1024));
    defer init.gpa.free(source);

    // 2. Compile to bytecode via QuickJS API
    const host = bridge_host_create() orelse return error.QuickJsRuntimeInitFailed;
    defer bridge_host_destroy(host);

    var bytecode_ptr: ?[*]u8 = null;
    var bytecode_len: usize = 0;
    const app_path_z: [*:0]const u8 = @ptrCast(app_path.ptr);
    try checkHost(host, bridge_host_compile_module(host, source.ptr, source.len, app_path_z, &bytecode_ptr, &bytecode_len));
    const bytecode = (bytecode_ptr orelse return error.BytecodeCompilationFailed)[0..bytecode_len];
    defer bridge_host_free_bytecode(host, bytecode_ptr.?);

    // 3. Copy our own executable, then append bytecode + trailer
    const cwd = std.Io.Dir.cwd();
    std.Io.Dir.copyFile(cwd, exe_name, cwd, output_path, init.io, .{ .replace = true }) catch {
        // Fallback: if exe_name is relative and not found, try /proc/self/exe equivalent
        return error.SelfCopyFailed;
    };

    // Open the copied file and append bytecode payload using positional writes.
    const file = try cwd.openFile(init.io, output_path, .{ .mode = .read_write });
    defer file.close(init.io);

    const stat = try file.stat(init.io);
    const exe_size = stat.size;

    // Write bytecode at the end of the copied file
    try file.writePositionalAll(init.io, bytecode, exe_size);

    // Write trailer: bytecode_size (u64 LE) + magic
    var trailer: [trailer_len]u8 = undefined;
    std.mem.writeInt(u64, trailer[0..8], bytecode_len, .little);
    @memcpy(trailer[8..16], trailer_magic);
    try file.writePositionalAll(init.io, &trailer, exe_size + bytecode_len);

    std.debug.print("compiled: {s} -> {s} ({d} bytes bytecode)\n", .{ app_path, output_path, bytecode_len });
}

// ── Mode 3: run embedded bytecode ─────────────────────────────────────

fn tryRunEmbedded(init: std.process.Init, exe_name: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    var file = cwd.openFile(init.io, exe_name, .{}) catch return error.NoEmbeddedBytecode;
    defer file.close(init.io);

    const stat = try file.stat(init.io);
    const file_size = stat.size;

    if (file_size < trailer_len) return error.NoEmbeddedBytecode;

    // Read the last 16 bytes
    var trailer: [trailer_len]u8 = undefined;
    const trailer_read = try file.readPositionalAll(init.io, &trailer, file_size - trailer_len);
    if (trailer_read < trailer_len) return error.NoEmbeddedBytecode;

    // Check magic
    if (!std.mem.eql(u8, trailer[8..16], trailer_magic)) return error.NoEmbeddedBytecode;

    const bytecode_size = std.mem.readInt(u64, trailer[0..8], .little);
    if (bytecode_size == 0 or bytecode_size > file_size - trailer_len) return error.NoEmbeddedBytecode;

    // Read bytecode
    const bytecode = try init.gpa.alloc(u8, bytecode_size);
    defer init.gpa.free(bytecode);

    const bc_offset = file_size - trailer_len - bytecode_size;
    const bc_read = try file.readPositionalAll(init.io, bytecode, bc_offset);
    if (bc_read < bytecode_size) return error.NoEmbeddedBytecode;

    // Execute bytecode
    const host = bridge_host_create() orelse return error.QuickJsRuntimeInitFailed;
    defer bridge_host_destroy(host);
    try checkHost(host, bridge_host_eval_bytecode(host, bytecode.ptr, bytecode.len));
    try runApplication(init, host);
}

// ── Application event loop ────────────────────────────────────────────

fn runApplication(init: std.process.Init, host: *BridgeHost) !void {
    var environment = zz.Environment.fromEnvMap(init.environ_map);
    var terminal = try zz.Terminal.init(init.io, &environment, .{
        .alt_screen = true,
        .hide_cursor = true,
        .mouse = true,
        .bracketed_paste = true,
    });
    defer terminal.deinit();

    var size = try terminal.getSize();
    try checkHost(host, bridge_host_app_init(host, size.cols, size.rows));
    try render(host, &terminal, size.cols, size.rows);

    var input_buffer: [8192]u8 = undefined;
    while (bridge_host_should_quit(host) == 0) {
        var needs_render = false;
        if (terminal.checkResize()) {
            size = try terminal.getSize();
            try checkHost(host, bridge_host_app_resize(host, size.cols, size.rows));
            needs_render = true;
        }

        const bytes_read = try terminal.readInput(&input_buffer, 16);
        var offset: usize = 0;
        while (offset < bytes_read) {
            const parsed = keyboard.parse(input_buffer[offset..bytes_read]);
            if (parsed.consumed == 0) break;
            offset += parsed.consumed;
            if (parsed.result == .key) {
                try dispatchKey(host, parsed.result.key);
                needs_render = true;
            }
        }

        try checkHost(host, bridge_host_app_tick(host, size.cols, size.rows));
        if (needs_render and bridge_host_should_quit(host) == 0) {
            try render(host, &terminal, size.cols, size.rows);
        }
    }
}

fn render(host: *BridgeHost, terminal: *zz.Terminal, width: u16, height: u16) !void {
    var text: ?[*:0]const u8 = null;
    var text_len: usize = 0;
    try checkHost(host, bridge_host_app_view(host, width, height, &text, &text_len));
    const view = text orelse return error.MissingJavaScriptView;
    defer bridge_host_free_view(host, view);

    try terminal.clear();
    try writeView(terminal.writer(), view[0..text_len]);
    try terminal.flush();
}

/// Normalize logical `\n` into terminal `\r\n` for raw mode.
fn writeView(writer: *std.Io.Writer, view: []const u8) !void {
    var lines = std.mem.splitScalar(u8, view, '\n');
    var first = true;
    while (lines.next()) |line| {
        if (!first) try writer.writeAll("\r\n");
        first = false;
        try writer.writeAll(line);
    }
}

fn dispatchKey(host: *BridgeHost, event: zz.KeyEvent) !void {
    const key_data: KeyData = switch (event.key) {
        .char => |codepoint| .{ .name = "char", .codepoint = @intCast(codepoint) },
        .up => .{ .name = "up", .codepoint = 0 },
        .down => .{ .name = "down", .codepoint = 0 },
        .left => .{ .name = "left", .codepoint = 0 },
        .right => .{ .name = "right", .codepoint = 0 },
        .escape => .{ .name = "escape", .codepoint = 0 },
        .enter => .{ .name = "enter", .codepoint = 0 },
        .tab => .{ .name = "tab", .codepoint = 0 },
        .backspace => .{ .name = "backspace", .codepoint = 0 },
        else => .{ .name = "other", .codepoint = 0 },
    };
    try checkHost(host, bridge_host_app_key(host, key_data.name, key_data.codepoint, @intFromBool(event.modifiers.shift), @intFromBool(event.modifiers.alt), @intFromBool(event.modifiers.ctrl)));
}

fn checkHost(host: *BridgeHost, status: c_int) !void {
    if (status == 0) return;
    bridge_host_dump_exception(host);
    return error.JavaScriptCallbackFailed;
}
