//! ZigZag TUI Counter Demo
//! A simple interactive counter demonstrating the zigzag TUI framework.

const std = @import("std");
const zz = @import("zigzag");

const Model = struct {
    count: i32,

    pub const Msg = union(enum) {
        key: zz.KeyEvent,
    };

    pub fn init(self: *Model, _: *zz.Context) zz.Cmd(Msg) {
        self.* = .{ .count = 0 };
        return .none;
    }

    pub fn update(self: *Model, msg: Msg, _: *zz.Context) zz.Cmd(Msg) {
        switch (msg) {
            .key => |k| switch (k.key) {
                .char => |c| switch (c) {
                    'q' => return .quit,
                    '+', '=' => self.count += 1,
                    '-', '_' => self.count -= 1,
                    '0' => self.count = 0,
                    else => {},
                },
                .up => self.count += 1,
                .down => self.count -= 1,
                .escape => return .quit,
                else => {},
            },
        }
        return .none;
    }

    pub fn view(self: *const Model, ctx: *const zz.Context) []const u8 {
        const content = blk: {
            // Title
            var title_style = zz.Style{};
            title_style = title_style.bold(true);
            title_style = title_style.fg(zz.Color.magenta);
            title_style = title_style.inline_style(true);
            const title = title_style.render(ctx.allocator, "Counter Demo") catch "Counter Demo";

            // Counter value — color changes with sign
            var counter_style = zz.Style{};
            counter_style = counter_style.bold(true);
            counter_style = counter_style.inline_style(true);
            counter_style = counter_style.fg(switch (std.math.order(self.count, 0)) {
                .gt => zz.Color.green,
                .lt => zz.Color.red,
                .eq => zz.Color.white,
            });
            const counter_str = std.fmt.allocPrint(ctx.allocator, "{d}", .{self.count}) catch "?";
            const styled_counter = counter_style.render(ctx.allocator, counter_str) catch counter_str;

            // Box around the counter
            var box_style = zz.Style{};
            box_style = box_style.borderAll(zz.Border.rounded);
            box_style = box_style.borderForeground(zz.Color.cyan);
            box_style = box_style.paddingAll(1);
            box_style = box_style.alignH(.center);

            const inner = std.fmt.allocPrint(ctx.allocator, "{s}\n\nCount: {s}", .{ title, styled_counter }) catch "Error";
            const boxed = box_style.render(ctx.allocator, inner) catch inner;
            const box_width = zz.measure.maxLineWidth(boxed);
            const centered_box = zz.place.place(ctx.allocator, box_width, zz.measure.height(boxed), .center, .top, boxed) catch boxed;

            // Help text
            var help_style = zz.Style{};
            help_style = help_style.fg(zz.Color.gray(12));
            help_style = help_style.inline_style(true);
            const help = help_style.render(ctx.allocator, "+/- Count  0 Reset  q/ESC Quit") catch "";

            const max_width = @max(box_width, zz.measure.width(help));
            const centered_help = zz.place.place(ctx.allocator, max_width, 1, .center, .top, help) catch help;

            const final_content = std.fmt.allocPrint(ctx.allocator, "{s}\n\n{s}", .{ centered_box, centered_help }) catch "Error";

            break :blk zz.place.place(ctx.allocator, ctx.width, ctx.height, .center, .middle, final_content) catch final_content;
        };

        return content;
    }
};

pub fn main(init: std.process.Init) !void {
    var program = zz.Program(Model).init(init.gpa, init.io, init.environ_map);
    defer program.deinit();

    try program.run();
}
