const std = @import("std");
const testing = std.testing;
const zz = @import("zigzag");

test "HitBox contains point" {
    const box = zz.HitBox.init(10, 5, 20, 10);

    try testing.expect(box.containsPoint(10, 5));
    try testing.expect(box.containsPoint(15, 10));
    try testing.expect(box.containsPoint(29, 14));

    // Outside
    try testing.expect(!box.containsPoint(9, 5));
    try testing.expect(!box.containsPoint(30, 5));
    try testing.expect(!box.containsPoint(10, 4));
    try testing.expect(!box.containsPoint(10, 15));
}

test "HitBox contains mouse event" {
    const box = zz.HitBox.init(5, 3, 10, 5);

    const inside = zz.MouseEvent{
        .x = 8,
        .y = 5,
        .button = .left,
        .event_type = .press,
    };
    try testing.expect(box.contains(inside));

    const outside = zz.MouseEvent{
        .x = 20,
        .y = 5,
        .button = .left,
        .event_type = .press,
    };
    try testing.expect(!box.contains(outside));
}

test "HitBox clicked" {
    const box = zz.HitBox.init(0, 0, 10, 5);

    const click = zz.MouseEvent{
        .x = 5,
        .y = 2,
        .button = .left,
        .event_type = .press,
    };
    try testing.expect(box.clicked(click));

    // Right click is not a click
    const right = zz.MouseEvent{
        .x = 5,
        .y = 2,
        .button = .right,
        .event_type = .press,
    };
    try testing.expect(!box.clicked(right));

    // Release is not a click
    const release = zz.MouseEvent{
        .x = 5,
        .y = 2,
        .button = .left,
        .event_type = .release,
    };
    try testing.expect(!box.clicked(release));
}

test "HitBox rightClicked" {
    const box = zz.HitBox.init(0, 0, 10, 5);

    const right = zz.MouseEvent{
        .x = 3,
        .y = 1,
        .button = .right,
        .event_type = .press,
    };
    try testing.expect(box.rightClicked(right));
}

test "HitBox localCoords" {
    const box = zz.HitBox.init(10, 20, 30, 15);

    const event = zz.MouseEvent{
        .x = 15,
        .y = 25,
        .button = .none,
        .event_type = .move,
    };

    const coords = box.localCoords(event).?;
    try testing.expectEqual(@as(u16, 5), coords.x);
    try testing.expectEqual(@as(u16, 5), coords.y);

    // Outside returns null
    const outside = zz.MouseEvent{
        .x = 0,
        .y = 0,
        .button = .none,
        .event_type = .move,
    };
    try testing.expect(box.localCoords(outside) == null);
}

test "HitBox expand" {
    const box = zz.HitBox.init(10, 10, 20, 15);
    const expanded = box.expand(2);

    try testing.expectEqual(@as(u16, 8), expanded.x);
    try testing.expectEqual(@as(u16, 8), expanded.y);
    try testing.expectEqual(@as(u16, 24), expanded.width);
    try testing.expectEqual(@as(u16, 19), expanded.height);
}

test "HitBox overlaps" {
    const a = zz.HitBox.init(0, 0, 10, 10);
    const b = zz.HitBox.init(5, 5, 10, 10);
    const c = zz.HitBox.init(20, 20, 5, 5);

    try testing.expect(a.overlaps(b));
    try testing.expect(b.overlaps(a));
    try testing.expect(!a.overlaps(c));
}

test "MouseState tracks click" {
    var state = zz.MouseState{};
    const box = zz.HitBox.init(0, 0, 10, 5);

    // Press inside
    const press = zz.MouseEvent{
        .x = 3,
        .y = 2,
        .button = .left,
        .event_type = .press,
    };
    const press_result = state.update(box, press);
    try testing.expectEqual(zz.MouseInteraction.press, press_result);
    try testing.expect(state.pressed);

    // Release inside = click
    const release = zz.MouseEvent{
        .x = 3,
        .y = 2,
        .button = .left,
        .event_type = .release,
    };
    const click_result = state.update(box, release);
    try testing.expectEqual(zz.MouseInteraction.click, click_result);
    try testing.expect(!state.pressed);
}

test "MouseState hover enter/leave" {
    var state = zz.MouseState{};
    const box = zz.HitBox.init(5, 5, 10, 10);

    // Move into box
    const enter = zz.MouseEvent{
        .x = 8,
        .y = 8,
        .button = .none,
        .event_type = .move,
    };
    const enter_result = state.update(box, enter);
    try testing.expectEqual(zz.MouseInteraction.enter, enter_result);
    try testing.expect(state.hover);

    // Move out of box
    const leave = zz.MouseEvent{
        .x = 0,
        .y = 0,
        .button = .none,
        .event_type = .move,
    };
    const leave_result = state.update(box, leave);
    try testing.expectEqual(zz.MouseInteraction.leave, leave_result);
    try testing.expect(!state.hover);
}
