const std = @import("std");
const testing = std.testing;
const zz = @import("zigzag");

const TestAction = enum { cut, copy, paste, delete };
const CM = zz.ContextMenu(TestAction);

test "ContextMenu init defaults" {
    const cm = CM.init();
    try testing.expect(!cm.visible);
    try testing.expectEqual(@as(usize, 0), cm.item_count);
    try testing.expect(cm.selected_action == null);
}

test "ContextMenu addItem" {
    var cm = CM.init();
    cm.addItem("Cut", "Ctrl+X", .cut);
    cm.addItem("Copy", "Ctrl+C", .copy);
    try testing.expectEqual(@as(usize, 2), cm.item_count);
}

test "ContextMenu show and hide" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);

    cm.show(10, 5);
    try testing.expect(cm.isVisible());
    try testing.expectEqual(@as(usize, 10), cm.x);
    try testing.expectEqual(@as(usize, 5), cm.y);

    cm.hide();
    try testing.expect(!cm.isVisible());
}

test "ContextMenu navigate and select" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);
    cm.addItem("Copy", "", .copy);
    cm.addItem("Paste", "", .paste);

    cm.show(0, 0);

    // Navigate down
    _ = cm.handleKey(.{ .key = .down, .modifiers = .{} });
    try testing.expectEqual(@as(usize, 1), cm.cursor);

    // Select
    _ = cm.handleKey(.{ .key = .enter, .modifiers = .{} });
    try testing.expect(!cm.isVisible());
    try testing.expectEqual(TestAction.copy, cm.getSelectedAction().?);
}

test "ContextMenu skips separators" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);
    cm.addSeparator();
    cm.addItem("Delete", "", .delete);

    cm.show(0, 0);
    try testing.expectEqual(@as(usize, 0), cm.cursor);

    _ = cm.handleKey(.{ .key = .down, .modifiers = .{} });
    try testing.expectEqual(@as(usize, 2), cm.cursor); // skipped separator
}

test "ContextMenu skips disabled items" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);
    cm.addDisabledItem("Copy", .copy);
    cm.addItem("Paste", "", .paste);

    cm.show(0, 0);
    _ = cm.handleKey(.{ .key = .down, .modifiers = .{} });
    try testing.expectEqual(@as(usize, 2), cm.cursor); // skipped disabled
}

test "ContextMenu Escape closes" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);
    cm.show(0, 0);

    const consumed = cm.handleKey(.{ .key = .escape, .modifiers = .{} });
    try testing.expect(consumed);
    try testing.expect(!cm.isVisible());
}

test "ContextMenu does not consume keys when hidden" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);

    const consumed = cm.handleKey(.{ .key = .down, .modifiers = .{} });
    try testing.expect(!consumed);
}

test "ContextMenu view renders when visible" {
    var cm = CM.init();
    cm.addItem("Cut", "Ctrl+X", .cut);
    cm.addItem("Copy", "Ctrl+C", .copy);
    cm.show(2, 1);

    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();

    const output = try cm.view(arena.allocator());
    try testing.expect(output.len > 0);
}

test "ContextMenu view empty when hidden" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);

    const output = try cm.view(testing.allocator);
    defer testing.allocator.free(output);
    try testing.expectEqual(@as(usize, 0), output.len);
}

test "ContextMenu showClamped adjusts position" {
    var cm = CM.init();
    cm.addItem("Cut", "", .cut);
    cm.addItem("Copy", "", .copy);

    // Show at far right - should clamp
    cm.showClamped(78, 0, 80, 24);
    try testing.expect(cm.x < 78);
    try testing.expect(cm.isVisible());
}
