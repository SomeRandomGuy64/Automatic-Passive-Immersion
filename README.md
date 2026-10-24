# Automatic-Passive-Immersion
Plays your passive immersion audio (e.g. condensed audio) on a loop and pauses it automatically whenever something else starts playing, like a YouTube video, an anime episode or a podcast. When the other media stops, passive audio carries on where it left off.

It's a small GUI app for Windows, macOS and Linux that sits in the system tray. It's a rewrite of my old Linux-only daemon (Go + `playerctl` + a systemd service).

## Features
- Pick the audio folder in the GUI. Every file in it is played on a loop, shuffled by default.
- A global hotkey (default `Ctrl+Alt+P`) pauses and resumes passive audio from any app.
- Auto-pause while other media plays, resuming a configurable number of seconds after it goes quiet.
- Or turn passive audio down instead of pausing (ducking), so a notification sound doesn't stop everything. "Turn it down, then pause" ducks first and only pauses if the other media keeps playing, rewinding over the part that played quietly.
- An hour counter: today's and total passive hours in the window and the tray menu. Only time at full volume counts. You choose when a new day starts (midnight by default, or e.g. 04:00 if you listen past midnight).
- Export your hours to CSV, one row per day or per day and file.
- Optional logging to [fazon.nexus Track](https://fazon.nexus/developers/): paste an API key and passive hours go to Track by themselves, under no sub-category, one you name, the audio file's name, or the day of the week (Monday, Tuesday...).
- Checks GitHub for a new release once a day and offers to download it.
- "Never pause for" list (e.g. `discord, steam`) so voice chat or games don't count as media.
- Tray icon whose colour shows the state: green = playing, amber = other media playing (play symbol while turned down, pause symbol while paused), blue = paused by you, grey = stopped.
- Launch at login, starting hidden in the tray.
- Portable mode: an empty `PassiveImmersion.ini` next to the executable makes the app keep its settings there instead of in your user config folder.
- If mpv crashes it's restarted automatically, and mpv never outlives the app.

## How "something else is playing" is detected
| OS | Method |
|---|---|
| Linux | Asks every MPRIS media player on D-Bus for its status, like `playerctl -a status` (browsers, VLC, Spotify, mpv with [mpv-mpris](https://github.com/hoyon/mpv-mpris)...) |
| Windows | Windows audio sessions: any app with an active audio stream that's actually making sound |
| macOS | Core Audio's per-process "is running output" flag (**macOS 14.2 or newer**) |

On Windows and macOS this means *anything* making sound counts, including games and notification sounds. Use "Turn it down, then pause", the resume delay and the "Never pause for" list to tune it.

Run `AutomaticPassiveImmersion --list-playing` to print what the detector currently sees. It's handy for checking detection on a new machine.

## Requirements
[mpv](https://mpv.io) does the actual playback. Install it normally or drop `mpv`/`mpv.exe` next to the app. The app looks next to itself, then on `PATH`, then in common install folders (Homebrew, `/Applications/mpv.app`, Program Files, Scoop). You can also set the path under **Advanced**.

## Hours and fazon.nexus Track
Hours are kept in `passive-hours.json` in the app's data folder (next to the executable in portable mode), per day and per file. **Export...** on the Hours tab writes them out as CSV.

To log to Track, create a *Read & write* key on your fazon.nexus profile (Developers section), tick **Log passive hours to Track automatically** and paste the key. Hours go to the `passive` category of the Track week they happened in, every 5 minutes, when playback stops and when the app quits. Anything that can't be sent yet (offline, a revoked key) waits in `track-queue.json` and goes next time. Track allows 30 sub-categories per category: with "The audio file's name", files past that limit are logged as unassigned. The key is stored in the settings ini, which is made readable by your user only.

## Settings under Advanced
- **mpv executable**: override the auto-detected mpv.
- **Extra mpv arguments**: passed straight to mpv, e.g. `--volume=70`.
- **Keys to press on start**: sent to mpv once it starts, e.g. `ctrl+p` to start an mpv tracking script.

## Known limitations
- Global hotkeys don't work on Wayland (only X11, Windows and macOS). Use the tray menu instead.
- On Linux, `Ctrl+Alt+F1`-`F12` can't be used as the hotkey because they switch virtual terminal.
- The macOS build isn't signed, so the first time you open it right-click the app → Open (or run `xattr -dr com.apple.quarantine AutomaticPassiveImmersion.app`).

## Building
Needs CMake 3.21+, a C++23 compiler and Qt 6.5+ (built and tested with Qt 6.10.3). [QHotkey](https://github.com/Skycoder42/QHotkey) is fetched automatically.

On Linux you also need the X11 development headers (`libx11-dev` on Debian/Ubuntu).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.3/<compiler>
cmake --build build --config Release
```

To make a portable folder with Qt bundled next to the app (uses `windeployqt`/`macdeployqt`/Qt's Linux deploy tool). The prefix has to be an absolute path or Qt's deploy step fails:
```
cmake --install build --config Release --prefix "/absolute/path/to/dist"
```

Every push and pull request is built on Windows, macOS and Linux by GitHub Actions. The portable builds can be downloaded from each run's artifacts.

### Releasing
1. Bump `VERSION` in `CMakeLists.txt` and rewrite `release-notes.md` for the new version.
2. Merge that into `main`, then tag it: `git tag v0.3.0 && git push origin v0.3.0`.

The tag builds and smoke-tests all three platforms, then publishes a GitHub release with `release-notes.md`, the three archives and their SHA-256 checksums. The tag has to match `VERSION` or the build stops. Keep the archive names as they are: the app's update check looks for them.

### Code layout
```
src/main.cpp                                  entry point, command line, single instance
src/PassiveImmersion/ImmersionController.*    the "daemon": polls the detector, decides when mpv pauses
src/PassiveImmersion/MpvPlayer.*              launches mpv and talks to it over its JSON IPC
src/PassiveImmersion/MediaDetector.h          interface each OS implements
src/PassiveImmersion/Platform/<OS>/           Linux (MPRIS), Windows (WASAPI), macOS (Core Audio) detectors
src/PassiveImmersion/MainWindow.*             settings/status window and tray icon
src/PassiveImmersion/HourLog.*                daily/per-file hour log and CSV export
src/PassiveImmersion/TrackSync.*              sends hours to the fazon.nexus Track API, with an on-disk queue
src/PassiveImmersion/UpdateChecker.*          asks GitHub for the latest release
src/PassiveImmersion/Settings.*               ini-backed settings (portable mode lives here)
src/PassiveImmersion/Autostart.*              launch at login for each OS
```
