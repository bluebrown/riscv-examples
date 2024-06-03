const std = @import("std");

pub fn build(b: *std.Build) !void {
    const k = b.addExecutable(.{
        .optimize = b.standardOptimizeOption(.{}),
        .target = b.resolveTargetQuery(.{
            .cpu_arch = .riscv64,
            .os_tag = .freestanding,
            .abi = .none,
            .ofmt = .elf,
        }),
        .name = "kernel.elf",
        .code_model = .medium,
        .root_source_file = b.path("main.zig"),
    });

    k.addAssemblyFile(b.path("entry.S"));
    k.setLinkerScript(b.path("linker.ld"));

    b.installArtifact(k);

    const qemu = b.addSystemCommand(&.{
        "qemu-system-riscv64",
        "-machine",
        "virt",
        "-display",
        "none",
        "-serial",
        "stdio",
        "-bios",
        "none",
        "-smp",
        "1",
        "-kernel",
        "zig-out/bin/kernel.elf",
    });

    qemu.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        qemu.addArgs(args);
    }

    const run = b.step("run", "Run the kernel with qemu virt machine");
    run.dependOn(&qemu.step);
}
