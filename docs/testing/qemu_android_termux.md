# Run SyntraOS ARM64 in QEMU on Android (Termux)

This guide runs the existing SyntraOS ARM64 kernel in QEMU's **system emulator**. It does not install SyntraOS on the phone or modify Android.

## 1. Download a CI-tested kernel

1. Open the SyntraOS repository on GitHub.
2. Open **Actions** and select the latest successful **SyntraOS CI** run on the `main` branch.
3. In the run's **Artifacts** section, download `syntraos-arm64-qemu-<commit-sha>`.
4. The ZIP contains `syntraos_arm64.elf`. Keep the file; it is a kernel ELF for QEMU's `-kernel` option, not a bootable disk image.

The artifact is uploaded only after the ARM64 ELF has built and passed the QEMU boot test. Artifacts are retained for 14 days; download a fresh one from a later successful run if it expires.

## 2. Install QEMU in Termux

Use the official Termux app from its maintained distribution source. Open Termux and run:

```sh
pkg update
pkg install qemu-system-aarch64-headless unzip
termux-setup-storage
```

Allow the storage permission when Android asks. The headless package provides the AArch64 system emulator and a text console; no desktop environment is required.

## 3. Put the kernel in your Termux home directory

Download the artifact ZIP using your browser. Then in Termux:

```sh
mkdir -p ~/syntraos-test
unzip ~/storage/downloads/syntraos-arm64-qemu-*.zip -d ~/syntraos-test
cd ~/syntraos-test
ls -lh syntraos_arm64.elf
```

If the ZIP is in a different folder, change the path in the `unzip` command. The final `ls` command should show `syntraos_arm64.elf`.

## 4. Start SyntraOS

Run:

```sh
qemu-system-aarch64 \
  -machine virt,gic-version=3,virtualization=off,secure=off \
  -cpu cortex-a57 \
  -m 512M \
  -nic none \
  -kernel syntraos_arm64.elf \
  -display none \
  -serial stdio \
  -monitor none \
  -no-reboot \
  -no-shutdown
```

The terminal is the virtual machine's serial console. You should see the same family of boot and test messages that the CI checks, including `SyntraOS boot OK` when the full boot path succeeds. Press **Ctrl+C** in Termux to stop QEMU.

## Notes and limitations

- This is a console-only development test, not a graphical desktop or touch UI.
- QEMU uses software emulation on many Android devices, so execution may be slow and battery use may increase.
- The tested machine profile is `virt` + GICv3 + Cortex-A57. Keep it aligned with `tools/arm64/run_qemu.sh` and the CI workflow.
- Do not flash this ELF to the phone or replace Android boot files. It is intended to run inside QEMU only.
- If QEMU reports an unsupported option or missing binary, check `qemu-system-aarch64 --version` and the installed Termux package before changing kernel code.


## 5. Optional graphical desktop preview (QEMU with ramfb)

The repository also provides a separate graphical test script. It enables QEMU's RAM framebuffer device and leaves the original headless UART test unchanged.

On a desktop Linux host with a QEMU display backend installed:

```sh
bash tools/arm64/run_qemu_gui.sh build/arm64/syntraos_arm64.elf
```

The script defaults to GTK. To select another QEMU display backend, set `QEMU_DISPLAY`, for example `QEMU_DISPLAY=sdl`. This preview currently draws a static SyntraOS desktop; pointer input, keyboard input, windows, and applications are not implemented yet.

On Android/Termux, the headless QEMU package may not include GTK, SDL, or VNC support. Check available backends with:

```sh
qemu-system-aarch64 -display help
```

If the installed QEMU supports VNC, try:

```sh
QEMU_DISPLAY=vnc=:1 bash tools/arm64/run_qemu_gui.sh ~/syntraos-test/syntraos_arm64.elf
```

Then connect a VNC viewer on the same device to `127.0.0.1:5901`. If VNC is not listed by `-display help`, this QEMU package cannot show the graphical preview; the UART test remains available. Do not change the kernel boot profile to work around a missing display backend.
