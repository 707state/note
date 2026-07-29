const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const zigzag_dep = b.dependency("zigzag", .{ .target = target, .optimize = optimize });
    const zigzag_mod = zigzag_dep.module("zigzag");

    const qjs_flags = &.{ "-D_GNU_SOURCE", "-DQUICKJS_NG_BUILD", "-funsigned-char" };

    // ── quickjs-ng core ────────────────────────────────────────────────
    const qjs_core_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    qjs_core_mod.addIncludePath(b.path("quickjs"));
    qjs_core_mod.linkSystemLibrary("m", .{});
    qjs_core_mod.linkSystemLibrary("pthread", .{});
    qjs_core_mod.linkSystemLibrary("dl", .{});
    qjs_core_mod.addCSourceFiles(.{
        .files = &.{ "quickjs/dtoa.c", "quickjs/libregexp.c", "quickjs/libunicode.c", "quickjs/quickjs.c" },
        .flags = qjs_flags,
    });
    // Static runtime library only; this does not build the upstream qjs CLI.
    const qjs_core = b.addLibrary(.{ .name = "quickjs-core", .root_module = qjs_core_mod, .linkage = .static });
    b.installArtifact(qjs_core);

    // ── zigzag bridge (C glue + Zig TUI exports) ──────────────────────
    const bridge_mod = b.createModule(.{
        .root_source_file = b.path("src/bridge/module.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "zigzag", .module = zigzag_mod }},
    });
    bridge_mod.addIncludePath(b.path("quickjs"));
    bridge_mod.linkLibrary(qjs_core);
    bridge_mod.linkSystemLibrary("m", .{});
    bridge_mod.linkSystemLibrary("pthread", .{});
    bridge_mod.linkSystemLibrary("dl", .{});
    bridge_mod.addCSourceFile(.{ .file = b.path("src/bridge/bridge.c"), .flags = qjs_flags });
    const bridge_lib = b.addLibrary(.{ .name = "zigzag-bridge", .root_module = bridge_mod, .linkage = .static });
    b.installArtifact(bridge_lib);

    // ── main executable ────────────────────────────────────────────────
    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "zigzag", .module = zigzag_mod }},
    });
    exe_mod.linkLibrary(qjs_core);
    exe_mod.linkLibrary(bridge_lib);
    exe_mod.addIncludePath(b.path("quickjs"));
    const exe = b.addExecutable(.{ .name = "oh_my_poop", .root_module = exe_mod });
    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    run_cmd.addPassthruArgs();

    const exe_tests = b.addTest(.{ .root_module = exe_mod });
    const run_exe_tests = b.addRunArtifact(exe_tests);
    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_exe_tests.step);
}
