## What's new in 0.2.0
- **Turn it down instead of pausing.** *When other media plays* now has three choices: pause (as before, still the default), turn passive audio down, or turn it down and only pause if the other media keeps playing. In the last mode, passive audio rewinds over the part that played quietly, so you don't miss it. A notification sound no longer stops everything on Windows and macOS.
- **Hour counter.** Today's and total passive hours show on the new Hours tab and in the tray menu. Only time playing at full volume counts. You choose when a new day starts (midnight by default, or e.g. 04:00 if you listen past midnight).
- **Export your hours** to CSV, one row per day or per day and file.
- **fazon.nexus Track.** Paste a Read & write API key from your profile and passive hours are logged to Track by themselves, under no sub-category, one you name, the audio file's name, or the day of the week. Anything that can't be sent yet waits and goes later.
- **Update check.** Once a day the app checks for a new release and offers to download it (you can turn this off on the General tab).
- Settings are now split into tabs, so the window fits on small screens.

0.1.0 doesn't have the update check, so this one update has to be downloaded by hand. Settings carry over.

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
Fully tested on Linux. The Windows and macOS builds are built, launched and smoke-tested in CI. Real-world detection and hotkey behaviour on those two hasn't been tried on a physical machine yet, so please open an issue if something misbehaves.
