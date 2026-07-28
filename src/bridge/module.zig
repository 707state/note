//! Zig side: terminal I/O + style rendering functions for the C bridge.
//! Exports C-callable functions used by bridge.c.

const std = @import("std");
const zz = @import("zigzag");

const fd_t = c_int;
const STDOUT_FD: fd_t = 1;
const STDIN_FD: fd_t = 0;
extern "c" fn write(fd: fd_t, buf: [*]const u8, count: usize) isize;
extern "c" fn read(fd: fd_t, buf: [*]u8, count: usize) isize;
extern "c" fn tcgetattr(fd: fd_t, termios_p: *Termios) c_int;
extern "c" fn tcsetattr(fd: fd_t, optional_actions: c_int, termios_p: *const Termios) c_int;
extern "c" fn ioctl(fd: fd_t, request: c_ulong, ...) c_int;
extern "c" fn poll(fds: [*]PollFd, nfds: c_ulong, timeout: c_int) c_int;
const TCSANOW: c_int = 0;
const TIOCGWINSZ: c_ulong = 0x5413;
const PollFd = extern struct { fd: fd_t, events: c_short, revents: c_short };
const POLLIN: c_short = 0x001;
const Termios = extern struct { c_iflag: u32, c_oflag: u32, c_cflag: u32, c_lflag: u32, c_line: u8, c_cc: [32]u8, c_ispeed: u32, c_ospeed: u32 };
const Winsize = extern struct { ws_row: u16, ws_col: u16, ws_xpixel: u16, ws_ypixel: u16 };
const IGNBRK: u32 = 1; const BRKINT: u32 = 2; const PARMRK: u32 = 8;
const ISTRIP: u32 = 32; const INLCR: u32 = 64; const IGNCR: u32 = 128;
const ICRNL: u32 = 256; const IXON: u32 = 512; const OPOST: u32 = 1;
const ECHO: u32 = 8; const ICANON: u32 = 4; const ISIG: u32 = 2;
const IEXTEN: u32 = 32768; const VMIN: usize = 6; const VTIME: usize = 5;

var raw_mode: bool = false;
var old_termios: Termios = undefined;

export fn bridge_term_init() c_int {
    if (raw_mode) return 0;
    _ = tcgetattr(STDIN_FD, &old_termios);
    var raw = old_termios;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_lflag |= ISIG;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    _ = tcsetattr(STDIN_FD, TCSANOW, &raw);
    _ = write(STDOUT_FD, "\x1b[?1049h\x1b[2J\x1b[?25l", 18);
    raw_mode = true;
    return 0;
}

export fn bridge_term_deinit() c_int {
    if (!raw_mode) return 0;
    _ = write(STDOUT_FD, "\x1b[?25h\x1b[?1049l", 15);
    _ = tcsetattr(STDIN_FD, TCSANOW, &old_termios);
    raw_mode = false;
    return 0;
}

export fn bridge_term_size(cols: *u16, rows: *u16) c_int {
    var ws = Winsize{ .ws_row = 24, .ws_col = 80, .ws_xpixel = 0, .ws_ypixel = 0 };
    _ = ioctl(STDOUT_FD, TIOCGWINSZ, &ws);
    cols.* = ws.ws_col;
    rows.* = ws.ws_row;
    return 0;
}

export fn bridge_term_write_at(row: u16, col: u16, text: [*c]const u8) c_int {
    var buf: [32]u8 = undefined;
    const cmd = std.fmt.bufPrint(&buf, "\x1b[{d};{d}H", .{ row + 1, col + 1 }) catch return -1;
    _ = write(STDOUT_FD, cmd.ptr, cmd.len);
    _ = write(STDOUT_FD, text, std.mem.len(text));
    return 0;
}

export fn bridge_term_clear() c_int { _ = write(STDOUT_FD, "\x1b[2J", 4); return 0; }
export fn bridge_term_hide_cursor() c_int { _ = write(STDOUT_FD, "\x1b[?25l", 6); return 0; }
export fn bridge_term_show_cursor() c_int { _ = write(STDOUT_FD, "\x1b[?25h", 6); return 0; }

export fn bridge_term_poll_event(buf: [*c]u8, buf_size: usize, timeout_ms: i32) isize {
    if (timeout_ms >= 0) {
        var fds: [1]PollFd = .{.{ .fd = STDIN_FD, .events = POLLIN, .revents = 0 }};
        if (poll(&fds, 1, timeout_ms) <= 0) return 0;
    }
    return read(STDIN_FD, buf, buf_size);
}

export fn bridge_style_render(text: [*c]const u8, out_ptr: *?[*]u8, out_len: *usize) c_int {
    const txt = std.mem.span(text);
    var st: zz.Style = .{};
    const rendered = st.render(std.heap.page_allocator, txt) catch return -1;
    out_ptr.* = @constCast(rendered.ptr);
    out_len.* = rendered.len;
    return 0;
}

export fn bridge_style_render_ex(
    text: [*c]const u8,
    fg_r: u8, fg_g: u8, fg_b: u8, has_fg: c_int,
    bg_r: u8, bg_g: u8, bg_b: u8, has_bg: c_int,
    bold: c_int, italic: c_int, underline: c_int, dim: c_int,
    border_name: [*c]const u8, has_border: c_int,
    bc_r: u8, bc_g: u8, bc_b: u8, has_bc: c_int,
    padding: u16, has_padding: c_int,
    width: u16, height: u16, max_w: u16, max_h: u16,
    alignment: c_int,
    out_ptr: *?[*]u8, out_len: *usize,
) c_int {
    const txt = std.mem.span(text);
    var st: zz.Style = .{};
    if (has_fg != 0) st = st.fg(zz.Color.fromRgb(fg_r, fg_g, fg_b));
    if (has_bg != 0) st = st.bg(zz.Color.fromRgb(bg_r, bg_g, bg_b));
    if (bold != 0) st = st.bold(true);
    if (italic != 0) st = st.italic(true);
    if (underline != 0) st = st.underline(true);
    if (dim != 0) st = st.dim(true);
    if (has_border != 0) {
        const b = std.mem.span(border_name);
        const border: zz.Border = if (std.mem.eql(u8, b, "rounded")) zz.Border.rounded
            else if (std.mem.eql(u8, b, "double")) zz.Border.double
            else if (std.mem.eql(u8, b, "thick")) zz.Border.thick
            else if (std.mem.eql(u8, b, "normal")) zz.Border.normal
            else zz.Border.none;
        st = st.borderAll(border);
    }
    if (has_bc != 0) st = st.borderForeground(zz.Color.fromRgb(bc_r, bc_g, bc_b));
    if (has_padding != 0) st = st.paddingAll(padding);
    if (width > 0) st = st.width(width);
    if (height > 0) st = st.height(height);
    if (max_w > 0) st = st.maxWidth(max_w);
    if (max_h > 0) st = st.maxHeight(max_h);
    st = st.alignH(switch (alignment) { 1 => .center, 2 => .right, else => .left });

    const rendered = st.render(std.heap.page_allocator, txt) catch return -1;
    out_ptr.* = @constCast(rendered.ptr);
    out_len.* = rendered.len;
    return 0;
}
