const std = @import("std");

/// Everything about a UEFI architecture that the build has to know: which QEMU
/// machine runs the firmware, where an EDK2 image for it usually lives, and how
/// the firmware names the file it boots from removable media.
const Platform = struct {
    qemu: []const []const u8,
    firmware: []const u8,
    /// Whether the firmware is loaded as a pflash drive (`-drive if=pflash`)
    /// rather than a single `-bios` image.
    pflash: bool,
    esp_boot_file: []const u8,
};

fn platform(arch: std.Target.Cpu.Arch) Platform {
    // The firmware paths are the ones Homebrew installs on macOS; override with
    // `-Dfirmware=` anywhere else.
    return switch (arch) {
        .x86_64 => .{
            .qemu = &.{ "qemu-system-x86_64", "-machine", "q35" },
            .firmware = "/opt/homebrew/share/qemu/edk2-x86_64-code.fd",
            .pflash = true,
            .esp_boot_file = "BOOTX64.EFI",
        },
        .aarch64 => .{
            .qemu = &.{ "qemu-system-aarch64", "-machine", "virt", "-cpu", "cortex-a57" },
            .firmware = "/opt/homebrew/share/qemu/edk2-aarch64-code.fd",
            .pflash = false,
            .esp_boot_file = "BOOTAA64.EFI",
        },
        else => std.debug.panic("unsupported UEFI architecture: {s}", .{@tagName(arch)}),
    };
}

pub fn build(b: *std.Build) void {
    // Firmware is loaded from flash on every boot and has no debugger attached,
    // so size-optimized is the sane default; `-Doptimize=Debug` trades binary
    // size for safety checks and panic messages.
    const optimize = b.option(
        std.builtin.OptimizeMode,
        "optimize",
        "Prioritize performance, safety, or binary size (default: ReleaseSmall)",
    ) orelse .ReleaseSmall;

    // The host machine is irrelevant here: this always cross-compiles. The
    // default is aarch64, matching the firmware on Apple Silicon machines;
    // pass `-Darch=x86_64` for PC firmware.
    const arch = b.option(
        std.Target.Cpu.Arch,
        "arch",
        "CPU architecture of the UEFI firmware (default: aarch64)",
    ) orelse .aarch64;
    const plat = platform(arch);

    // `.uefi` fixes the OS ABI: PE/COFF output, Microsoft ABI, EFI_APPLICATION
    // subsystem, and `EfiMain` as the entry point. Nothing linkable against the
    // host remains, so no libc, no OS, and only `std.os.uefi` is usable.
    const target = b.resolveTargetQuery(.{
        .cpu_arch = arch,
        .os_tag = .uefi,
    });

    const exe = b.addExecutable(.{
        .name = "learn_uefi",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(exe);

    // Firmware with no configured boot entries falls back to the
    // removable-media path `<esp>/EFI/BOOT/<BOOT…>.EFI`, so stage that exact
    // layout next to the raw image. Copy the tree onto a FAT partition (or use
    // `zig build run`) to boot it.
    const esp_file = b.addInstallFileWithDir(
        exe.getEmittedBin(),
        .{ .custom = "esp/EFI/BOOT" },
        plat.esp_boot_file,
    );
    b.getInstallStep().dependOn(&esp_file.step);

    const firmware = b.option(
        []const u8,
        "firmware",
        b.fmt("EDK2 firmware image to boot in QEMU (default: {s})", .{plat.firmware}),
    ) orelse plat.firmware;

    const run_cmd = b.addSystemCommand(plat.qemu);

    // The PC firmware ships as a code-only image for the pflash device: it is
    // far larger than the legacy 1 MiB BIOS ROM window that `-bios` loads into,
    // and QEMU rejects it there. The variable-store half is left off, in which
    // case the firmware keeps its variables in `NvVars` on the ESP; the code
    // half is read-only, so a boot never writes to the installed firmware file.
    if (plat.pflash) {
        run_cmd.addArgs(&.{
            "-drive",
            b.fmt("if=pflash,format=raw,unit=0,readonly=on,file={s}", .{firmware}),
        });
    } else {
        run_cmd.addArgs(&.{ "-bios", firmware });
    }

    // QEMU builds the FAT filesystem from the staged tree on the fly, so there
    // is no disk image to create or mount. Writes are enabled because the
    // firmware stores `NvVars` there; the tree lives under the install prefix,
    // never in the source directory.
    const esp_dir = b.getInstallPath(.{ .custom = "esp" }, "");
    run_cmd.addArgs(&.{
        "-drive",
        // Absolute unless `--prefix` was relative, in which case QEMU inherits
        // the same working directory the install prefix was resolved against.
        b.fmt("format=raw,file=fat:rw:{s}", .{esp_dir}),
    });

    // The serial console is multiplexed onto stdio, monitor included (Ctrl-A X
    // quits), so the child needs the real terminal rather than a pipe.
    run_cmd.addArg("-nographic");
    run_cmd.stdio = .inherit;
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Boot the app in QEMU (Ctrl-A X to quit)");
    run_step.dependOn(&run_cmd.step);
}
