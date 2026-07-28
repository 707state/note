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
    const qjs_core = b.addLibrary(.{ .name = "qjs", .root_module = qjs_core_mod, .linkage = .static });
    b.installArtifact(qjs_core);

    // ── quickjs-libc ───────────────────────────────────────────────────
    const qjs_libc_mod = b.createModule(.{ .target = target, .optimize = optimize });
    qjs_libc_mod.addIncludePath(b.path("quickjs"));
    qjs_libc_mod.linkLibrary(qjs_core);
    qjs_libc_mod.addCSourceFile(.{ .file = b.path("quickjs/quickjs-libc.c"), .flags = qjs_flags });
    const qjs_libc = b.addLibrary(.{ .name = "quickjs-libc", .root_module = qjs_libc_mod, .linkage = .static });
    b.installArtifact(qjs_libc);

    // ── zigzag bridge (C glue + Zig TUI exports) ──────────────────────
    const bridge_mod = b.createModule(.{
        .root_source_file = b.path("src/bridge/module.zig"),
        .target = target, .optimize = optimize, .link_libc = true,
        .imports = &.{.{ .name = "zigzag", .module = zigzag_mod }},
    });
    bridge_mod.addIncludePath(b.path("quickjs"));
    bridge_mod.linkLibrary(qjs_core);
    bridge_mod.linkLibrary(qjs_libc);
    bridge_mod.linkSystemLibrary("m", .{});
    bridge_mod.linkSystemLibrary("pthread", .{});
    bridge_mod.linkSystemLibrary("dl", .{});
    bridge_mod.addCSourceFile(.{ .file = b.path("src/bridge/bridge.c"), .flags = qjs_flags });
    const bridge_lib = b.addLibrary(.{ .name = "zigzag-bridge", .root_module = bridge_mod, .linkage = .static });
    b.installArtifact(bridge_lib);

    // ── qjsc (bridge linked) ───────────────────────────────────────────
    const qjsc_mod = b.createModule(.{ .target = target, .optimize = optimize });
    qjsc_mod.addIncludePath(b.path("quickjs"));
    qjsc_mod.linkLibrary(qjs_core);
    qjsc_mod.linkLibrary(qjs_libc);
    qjsc_mod.linkLibrary(bridge_lib);
    qjsc_mod.addCSourceFile(.{ .file = b.path("quickjs/qjsc.c"), .flags = qjs_flags });
    const qjsc_exe = b.addExecutable(.{ .name = "qjsc", .root_module = qjsc_mod });
    b.installArtifact(qjsc_exe);

    // ── qjs (bridge linked) ────────────────────────────────────────────
    const qjs_exe_mod = b.createModule(.{ .target = target, .optimize = optimize });
    qjs_exe_mod.addIncludePath(b.path("quickjs"));
    qjs_exe_mod.linkLibrary(qjs_core);
    qjs_exe_mod.linkLibrary(qjs_libc);
    qjs_exe_mod.linkLibrary(bridge_lib);
    qjs_exe_mod.addCSourceFiles(.{
        .root = b.path("."),
        .files = &.{ "quickjs/qjs.c", "quickjs/gen/repl.c", "quickjs/gen/standalone.c" },
        .flags = qjs_flags,
    });
    const qjs_exe = b.addExecutable(.{ .name = "qjs", .root_module = qjs_exe_mod });
    b.installArtifact(qjs_exe);

    // ── main executable ────────────────────────────────────────────────
    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"), .target = target, .optimize = optimize,
        .imports = &.{.{ .name = "zigzag", .module = zigzag_mod }},
    });
    exe_mod.linkLibrary(qjs_core);
    exe_mod.linkLibrary(qjs_libc);
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

    jsCompilerStep(b, target, optimize, qjs_core, qjs_libc, bridge_lib, qjsc_exe);
}

fn jsCompilerStep(
    b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode,
    qjs_core: *std.Build.Step.Compile, qjs_libc: *std.Build.Step.Compile,
    bridge_lib: *std.Build.Step.Compile, qjsc_exe: *std.Build.Step.Compile,
) void {
    const js_file = b.option([]const u8, "js", "JS file to compile") orelse return;
    const js_out_name = b.option([]const u8, "js-out", "output executable name") orelse "js_app";

    const qjsc_run = b.addRunArtifact(qjsc_exe);
    qjsc_run.addArgs(&.{ "-e", "-o" });
    const c_out = qjsc_run.addOutputFileArg("qjsc_out.c");
    qjsc_run.addArgs(&.{ "-M", "zigzag,zigzag", "-m", js_file });

    const js_exe_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    js_exe_mod.addIncludePath(b.path("quickjs"));
    js_exe_mod.linkLibrary(qjs_core);
    js_exe_mod.linkLibrary(qjs_libc);
    js_exe_mod.linkLibrary(bridge_lib);
    js_exe_mod.linkSystemLibrary("m", .{});
    js_exe_mod.linkSystemLibrary("pthread", .{});
    js_exe_mod.linkSystemLibrary("dl", .{});
    js_exe_mod.addCSourceFile(.{ .file = c_out, .flags = &.{ "-D_GNU_SOURCE", "-DQUICKJS_NG_BUILD" } });

    const js_exe = b.addExecutable(.{ .name = js_out_name, .root_module = js_exe_mod });
    js_exe.step.dependOn(&qjsc_run.step);
    b.installArtifact(js_exe);

    const js_run_step = b.step("js-run", "Compile and run a JS file (-Djs=file)");
    const js_run_cmd = b.addRunArtifact(js_exe);
    js_run_step.dependOn(&js_run_cmd.step);
    js_run_cmd.step.dependOn(b.getInstallStep());
    js_run_cmd.addPassthruArgs();
}
