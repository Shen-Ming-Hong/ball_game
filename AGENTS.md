# Repository Guidelines

## Project Structure & Module Organization
This PlatformIO firmware project keeps all runtime logic centralized so navigation stays predictable.
- `src/main.cpp` houses the UNO state machine covering countdown, TM1637 display driving, MP3 playback, and IR scoring integration.
- `include/` stores headers and shared declarations; keep pin maps and prototypes here to keep `main.cpp` lean.
- `lib/` is reserved for vendor or custom libraries that PlatformIO should bundle alongside the firmware.
- `test/` currently carries bench documentation; add Unity test harnesses or diagnostic sketches here as they mature.
- `platformio.ini` defines the single `uno` environment and the TM1637 dependency. Treat `.pio/` as generated output and leave it untracked.

## Build, Test, and Development Commands
Run commands from the repository root to ensure PlatformIO locates the configuration.
- `pio run` builds the firmware for the default `uno` target; run it after major changes.
- `pio run --target upload` flashes the Arduino; append `--upload-port COMx` when Windows assigns a different port.
- `pio device monitor -b 9600` opens the serial console and mirrors log output from `setup()` and the countdown loop.
- `pio test -e uno` executes automated tests once Unity suites are added under `test/`.

## Coding Style & Naming Conventions
Match the existing Arduino C++ style so diffs stay readable.
- Use two-space indentation, Allman braces, and descriptive `//` comments kept close to the logic they explain.
- Keep constants in `UPPER_SNAKE_CASE`, functions and globals in `lowerCamelCase`, and enums in `PascalCase`.
- Prefer non-blocking patterns with volatile flags for ISR communication, and avoid lengthy work inside interrupts.

## Testing Guidelines
Firmware changes must be verified on hardware and, when possible, through automated checks.
- After flashing, confirm button debounce, IR triggers, audio cues, and rotating lights via the serial timeline.
- Capture thresholds or calibration tweaks in the relevant `test/` subfolder README so future agents can repeat the setup.
- Build Unity suites named `test_<feature>.cpp` for new logic; mock timing helpers instead of relying on `delay()`.

## Commit & Pull Request Guidelines
Consistency in history keeps collaboration efficient across human and automated contributors.
- Write short, imperative commit subjects in Traditional Chinese that summarize the primary change.
- Group related modifications per commit and mention the affected subsystem (timer, sensors, audio, lighting).
- Pull requests should outline the feature or fix, document hardware tested (UNO, sensor count, audio shield), attach serial output or photos when behavior changes, and link supporting issues.

## Hardware & Deployment Tips
Reliable hardware feedback shortens the debug loop.
- Confirm `lib_deps` resolves before building so TM1637 support is ready.
- Re-run `pio device monitor` after every flash to verify the baud rate and capture startup logs.
- When tuning sensors, temporarily log `ir_analog_values[i]` and remove the instrumentation once thresholds are locked in.
