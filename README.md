# omalauncher-dex-tools

Samsung DeX multi-display window router and Qt6/Lazarus Wayland compatibility fixes for [omalauncher](https://github.com/SimonSchubert/omalauncher).

## Features

1. **Samsung DeX / Multi-Display Window Routing (`omarchy-dex-daemon`)**
   - Automatically routes Linux Wayland windows (`LinuxWindowActivity`, `AgyWindow`, `TerminalWindow`, `BtopWindow`, `FilesActivity`, `SettingsActivity`, `EditorActivity`, `OwlAppActivity`) spawned by `omalauncher`'s background service on `displayId=0` into the active Samsung DeX freeform desk (`displayId > 0`, `mWindowingMode=freeform`) via `wm shell desktopmode moveTaskToDesk` and `cmd activity stack move-task`.
   - Preserves focus on `moe.n4tsu.dextop` (`Dextop`) on `displayId=0` when using local DeX display mirroring.
   - Connects automatically via Android Wireless Debugging (`_adb-tls-connect._tcp` mDNS or local port scan) and keeps a persistent `adb shell` session open for `<10ms` window transitions.

2. **Qt6 / Lazarus LCL Wayland Fix (`libheidisql-fix.so` & `heidisql` wrapper)**
   - **Transient Probe Surface Filtering:** Blocks unbuffered 20µs dummy `xdg_toplevel` probe surfaces created at startup by Qt6/Lazarus (`libQt6Pas`) before `wl_surface.attach`, preventing `omalauncher` from spawning and destroying 6 empty `LinuxWindowActivity` tasks per launch.
   - **Double-DPI Scaling Fix:** Overrides `QPaintDevice_logicalDpiX/Y` and `QScreen_*DotsPerInch*` to return `96` DPI so Lazarus LCL does not multiply form dimensions on top of `omalauncher`'s `wl_output.scale = 3`.

3. **DeX Linux App Launcher (`omarchy-dex-menu`)**
   - Lightweight GTK3 Wayland launcher window for opening Linux apps into individual DeX freeform windows.

## Installation

```sh
make
sudo ./install.sh
```
