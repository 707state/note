//! Minimal UEFI application, cross-compiled for the `*-uefi` targets (see
//! build.zig). There is no OS underneath: the firmware loads this PE/COFF
//! image, calls `EfiMain`, and `std.start` forwards that call to `main` below,
//! publishing the image handle and system table in `std.os.uefi`.

const std = @import("std");
const builtin = @import("builtin");
const uefi = std.os.uefi;

/// Largest message handed to the firmware console in a single call. Messages
/// are transcoded on the stack, so this bounds the stack usage of `print`.
const max_message_len = 1024;

/// Writes a UTF-8 formatted message to the firmware console, i.e. the
/// `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL` in the system table.
///
/// The protocol takes UCS-2 strings terminated by a null character, so the
/// message is transcoded before it reaches the firmware. Buffers are static,
/// which makes `print` non-reentrant; do not call it from interrupt context or
/// from multiple cores at once.
pub fn print(comptime fmt: []const u8, args: anytype) void {
    const con_out = uefi.system_table.con_out orelse return;

    var utf8_buf: [max_message_len]u8 = undefined;
    // One extra element for the null terminator the protocol requires.
    var utf16_buf: [max_message_len + 1]u16 = undefined;

    const utf8 = std.fmt.bufPrint(&utf8_buf, fmt, args) catch {
        _ = con_out.outputString(wide("<print: message dropped>")) catch {};
        return;
    };
    const len = std.unicode.utf8ToUtf16Le(&utf16_buf, utf8) catch {
        _ = con_out.outputString(wide("<print: invalid UTF-8>")) catch {};
        return;
    };

    utf16_buf[len] = 0;
    _ = con_out.outputString(utf16_buf[0..len :0]) catch {};
}

/// Marshals an ASCII comptime literal into the null-terminated UTF-16 form the
/// firmware console expects. Only for the few messages that cannot be produced
/// by `print`; every other string goes through the formatter.
fn wide(comptime s: []const u8) [*:0]const u16 {
    const S = struct {
        const data: [s.len:0]u16 = blk: {
            var buf: [s.len:0]u16 = undefined;
            for (s, 0..) |byte, i| {
                if (byte > 0x7f) @compileError("wide: only ASCII literals are supported");
                buf[i] = byte;
            }
            buf[s.len] = 0;
            break :blk buf;
        };
    };
    return &S.data;
}

pub fn main() uefi.Status {
    // UEFI consoles need CRLF: a bare LF moves the cursor without returning it.
    print("Hello from Zig UEFI!\r\n", .{});
    print("  built for:     {s}-{s}\r\n", .{
        @tagName(builtin.target.cpu.arch),
        @tagName(builtin.target.os.tag),
    });
    print("  zig version:   {s}\r\n", .{builtin.zig_version_string});
    print("  UEFI revision: {d}.{d}\r\n", .{
        uefi.system_table.hdr.revision >> 16,
        uefi.system_table.hdr.revision & 0xffff,
    });
    print("  config tables: {d}\r\n", .{uefi.system_table.number_of_table_entries});

    // Returned to the firmware as EFI_STATUS; see std.start.EfiMain.
    return .success;
}
