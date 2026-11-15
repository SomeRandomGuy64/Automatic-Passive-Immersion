## What's new in 0.3.0
- **New files are picked up while it plays.** Drop audio into the folder (or a subfolder) and it joins the playlist within a couple of seconds, no restart needed. With shuffle on it goes in somewhere still to come this time round, so you hear it before the whole loop repeats; with shuffle off it slots in by name.
- **Deleted files drop out** of the playlist too. One that's playing is allowed to finish first.
- **Files still being copied wait** until the copy has finished, so a half-copied file never plays.
- **An empty folder starts playing by itself** as soon as the first file lands in it, instead of stopping with an error.
- Folders on network drives that don't report changes are rescanned once a minute.

Settings carry over. 0.2.0 offers this update on its own; 0.1.0 has to be updated by hand.

## Downloads
| OS | File | How to run |
|---|---|---|
| Windows 10/11 (x64) | `AutomaticPassiveImmersion-windows-x64.zip` | Extract anywhere and run `AutomaticPassiveImmersion.exe` |
| macOS 14.2+ (Apple Silicon + Intel) | `AutomaticPassiveImmersion-macos-universal.zip` | Extract and open `AutomaticPassiveImmersion.app` |
| Linux (x64, glibc 2.35+, X11) | `AutomaticPassiveImmersion-linux-x64.tar.gz` | Extract and run `bin/AutomaticPassiveImmersion` |

All three are portable: Qt is bundled, nothing needs installing. To update, quit the app and replace the old folder with the new one. To keep settings next to the app instead of in your user config folder, create an empty `PassiveImmersion.ini` beside the executable.

## You also need mpv
[mpv](https://mpv.io) does the playback. Install it normally (Homebrew, Scoop, your package manager), or put `mpv`/`mpv.exe` next to the app. You can also set its path under **Advanced** on the General tab.

## First launch notes
- **Windows:** the exe isn't signed, so SmartScreen may warn. Click *More info → Run anyway*.
- **macOS:** the app isn't signed or notarized. Right-click it → *Open*, or run `xattr -dr com.apple.quarantine AutomaticPassiveImmersion.app`.
- **Linux:** needs `libxcb-cursor0` (installed by default on most desktops). Global hotkeys work on X11 only; on Wayland use the tray menu.

## Status
Fully tested on Linux, including the new folder watching. The Windows and macOS builds are built, launched and smoke-tested in CI. Real-world detection and hotkey behaviour on those two hasn't been tried on a physical machine yet, so please open an issue if something misbehaves.
