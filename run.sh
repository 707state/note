#!/bin/sh
#
# Boot the UEFI image in QEMU.
#
#   ./run.sh                              boot the architecture build.zig defaults to
#   ./run.sh aarch64                      boot a specific architecture
#   ./run.sh x86_64 -Doptimize=Debug      extra arguments go to `zig build`
#   FIRMWARE=/path/to/firmware.fd ./run.sh
#
# The build graph owns everything else: the `run` step in build.zig builds the
# image, stages the ESP layout, and invokes QEMU. This script only resolves what
# a build script cannot: the QEMU binary and the firmware image on this host.
set -eu

arch=${1:-}
if [ $# -gt 0 ]; then
    shift
fi

if [ -n "$arch" ]; then
    case "$arch" in
        aarch64 | x86_64) ;;
        *)
            echo "$0: unknown architecture '$arch' (aarch64, x86_64)" >&2
            exit 2
            ;;
    esac
    command -v "qemu-system-$arch" >/dev/null 2>&1 ||
        {
            echo "$0: qemu-system-$arch not found in PATH" >&2
            exit 1
        }
fi

# EDK2 images QEMU ships: Homebrew's path first, then the usual Linux ones.
firmware=${FIRMWARE:-}
if [ -z "$firmware" ] && [ -n "$arch" ]; then
    for candidate in \
        "/opt/homebrew/share/qemu/edk2-$arch-code.fd" \
        "/usr/local/share/qemu/edk2-$arch-code.fd" \
        "/usr/share/qemu/edk2-$arch-code.fd"
    do
        if [ -f "$candidate" ]; then
            firmware=$candidate
            break
        fi
    done
    if [ -z "$firmware" ]; then
        echo "$0: no EDK2 image found for $arch" >&2
        echo "$0: set FIRMWARE=/path/to/firmware.fd" >&2
        exit 1
    fi
fi

# Unset architecture and firmware mean "use build.zig's defaults"; a quoted
# `${var:+…}` would pass an empty argument instead of none.
if [ -n "$firmware" ]; then
    set -- "-Dfirmware=$firmware" "$@"
fi
if [ -n "$arch" ]; then
    set -- "-Darch=$arch" "$@"
fi
exec zig build "$@" run
