# OBS target handoff

## Objective and state
Add OBS Studio projector selection while preserving WoW and the shared neural pipeline.
Branch: `obs-target`; fork: `nbosse1998/dlss5-wow-sidecar`. No merges into main,
history rewrites, workflow edits, or GitHub automation operations are authorized.
Baseline: see `git log` parent of the first handoff commit. Initial tree clean on main.
No repository AGENTS.md or CONTRIBUTING.md was found.

## Original capture flow (traced before implementation)
- `WindowTracker.cpp::FindWowWindow`: FindWindowExW over top-level classes
  waApplication Window, GxWindowClass, GxWindowClassD3d, GxWindowClassD3d9Ex,
  GxWindowClassOpenGl, w. First visible positive-client-area window wins.
  Only generic class w requires exact title World of Warcraft; other classes
  accept localized titles. No executable filename, metadata, process, PID or
  install-folder check in window discovery. Class selects modern vs Wrath.
- Runtime `ResolveTarget` rejects non-borderless WoW. Optional command-line
  class name supports testpattern. Manager separately probes WoW folders and
  injector filenames; these are not capture discovery.
- ResolveTarget's HWND becomes PipelineConfig.target. Pipeline::Create sizes
  resources from GetClientRect, creates DeviceBridge, WgcSource::CreateForWindow,
  and DirectComposition overlay. WGC uses IGraphicsCaptureItemInterop::CreateForWindow.
- Shared pipeline: D3D11 capture -> shared D3D12 textures -> luminance/NVOFA
  motion -> normalization -> existing neural pass -> UI mask -> DComp overlay.
  WindowTracker follows location events out of context. WGC Closed/IsWindow
  detects target loss; Pipeline signals owner thread to exit. All generic.
- Neural runtime selection, presets, settings, hotkeys and processing are generic.
  Existing project provides neural filtering, NOT frame generation or FPS gains.
- Resize currently lacks frame-pool/ring recreation. Windowed WGC can include
  non-client borders while Pipeline sizes from the client; needs explicit handling.

## Chosen design
Shared OBS enumeration helper returns explicit HWND/PID/title candidates. Require
OBS executable basename via PROCESS_QUERY_LIMITED_INFORMATION only, plus recognized
English projector titles; no install directory or WoW executable required.
Manager mode + explicit projector picker passes HWND/PID to runtime, which revalidates
before capture; never auto-select a different window. Mode persists, HWND does not.
WoW discovery and rendering algorithms remain intact. Prefer clean stop with an
instruction on OBS resize/minimize over unsafe mismatched GPU copies.

## Work completed / validation
Initial source/build/package inspection complete. See implementation and final validation checkpoints below for completed changes.
CMake initially unavailable; later installed for a configure attempt.
MSVC, Windows SDK, WGC/OBS/GPU unavailable.
No compile or live Windows test claimed. No executable produced.

## Build and test commands (Windows developer command prompt)
Prerequisites: VS 2022 Desktop C++, CMake >=3.28, Python + pefile, Windows SDK
with dxc (release uses 10.0.26100.0 dxc), NVIDIA DLSS SDK v310.7.0 and Optical
Flow SDK 5.0.7 for actual neural execution. Dependencies fetched at pinned tags.
```
python -m pip install pefile pytest
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSIDECAR_REQUIRE_SDKS=ON -DDLSS_SDK_DIR="C:/SDK/DLSS" -DNVOF_SDK_DIR="C:/SDK/OpticalFlow" -DDXC_EXECUTABLE="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
cmake --build build --config Release
build\tests\Release\sidecar_tests.exe "[unit]"
build\tests\Release\sidecar_tests.exe "[device]"
python -m pytest ci -q
python ci/check_translations.py
```
Paths C:/SDK/... are placeholders for your downloaded SDKs. Without SDKs builds
can silently be passthrough, so REQUIRE_SDKS=ON is deliberate.
Outputs: build/Release/wowsidecar.exe and wowsidecar-manager.exe, raw executables.
Runtime folder must also contain upstream dxgi.dll (ReShade), renodx-dlss5.addon64,
nvngx_dlss.dll, runtimes/latest/nvngx_dlssnr.dll and
runtimes/compatible/nvngx_dlssnr.dll (Ada-patched for RTX 4070 Super).
MSVC x64 redistributable required by default dynamic CRT. DLLs are not built here.
`powershell -File tools/package_release.ps1 -BuildDir build -RuntimeDirectory out/sidecar`
creates portable ZIP under out/release, not installer; requires clean tree,
SDK-enabled build and approved runtime hashes. See docs/VERIFYING.md.

## Exact next step
On Windows, check out `obs-target`, configure/build x64 Release with both NVIDIA
SDKs using the commands above, run `[unit]`, then execute steps 1-8 of
`docs/OBS_PROJECTOR.md` at 100% and 125% DPI. First verify the OBS-only client crop
aligns without black borders. This is the highest-risk unverified code path.
Fix any compiler errors on this branch; record actual outputs here. No merge.


## Implementation checkpoint
Implemented source changes (not Windows-compiled): shared ObsProjector helper,
manager target mode/picker, persisted obs_projector boolean, explicit runtime
HWND/PID revalidation, OBS-only WGC client-area crop, stop/restart on geometry
changes/minimize/hidden/lost target, and five-second no-frame timeout. WoW's WGC
path defaults unchanged. No renderer/model algorithms changed.
OBS process is opened only with PROCESS_QUERY_LIMITED_INFORMATION after title
filtering; this meets spec I2. New source requires dwmapi for client crop.
New Catch2 tests cover identity matching, default behavior, config persistence
and malformed mode type. They require Windows to execute.
Python unittest discovery was attempted incorrectly: files use pytest, which
was initially missing. Translation checker passed all nine tables. Installing
pytest/CMake locally for correct test invocation/configure attempt is in progress.

## Final source/test checkpoint
Implemented, NOT Windows-compiled, NOT verified working with OBS or WoW here.
Known-working checks only: pure executable/title identity predicate and existing
Python tooling. Existing WoW path is logically preserved by code inspection;
Windows compilation/regression testing remains outstanding. No binary produced.

### Files added
- `src/common/present/ObsProjector.h/.cpp`: candidate enumeration and revalidation,
  using EnumWindows, GetWindowThreadProcessId and query-limited executable lookup.
- `src/common/present/ObsProjectorRules.h`: portable executable/title predicate.
- `tests/test_obs_projector_identity.cpp`: nine identity assertions.
- `tests/test_obs_projector.cpp`: invalid HWND, defaults and config round-trip/type tests.
- `docs/OBS_PROJECTOR.md`: exact Windows build/runtime setup and 15-step manual checklist.
- `CODEX_HANDOFF.md`: persistent state, traced architecture, results and next step.

### Files modified
- `CMakeLists.txt`: compile discovery helper; link dwmapi for OBS-only client crop.
- `tests/CMakeLists.txt`: register both new Catch2 test sources.
- `src/common/core/Config.h/.cpp`: default-off persisted obs_projector boolean;
  existing files default to WoW, rendering reset leaves target mode intact.
- `src/manager/main.cpp`: mode and explicit picker, selection labels with PID/HWND/
  size/location, general-only OBS probes, skip WoW discovery/setup in OBS mode,
  launch revalidation/arguments; WoW client selector disabled while in OBS mode.
- `src/runtime/main.cpp`: strict HWND/PID argument parsing and revalidation, direct
  launch guard against saved-OBS->WoW fallback, OBS timer and restart dialogs.
- `src/runtime/Pipeline.h/.cpp`: one default-false captureClientArea flag forwarded
  to WGC; same Create/Start/neural/presentation code in both modes.
- `src/common/capture/WgcSource.h/.cpp`: opt-in OBS client crop before the existing
  bridge, bounds checks, five-second no-frame timeout, diagnostic logs. No WoW
  crop or timeout enabled; existing callers retain default behavior.
- `README.md`: fork feature status and test/build document link.

### Results
- `python -m pytest ci -q`: **32 passed**. One unrelated installed-plugin
  PytestRemovedIn10Warning; no test failure.
- `python ci/check_translations.py`: **passed**, all nine translation tables.
  New OBS controls remain English and deliberately do not invent translations.
- Portable identity tests compiled with GCC 14.2/C++20 against pinned Catch2 3.7.1
  amalgamated sources: **1 case / 9 assertions passed**. This verifies only the
  pure identity rule, not EnumWindows, Win32 code, config tests or WGC.
- `python -m cmake -S . -B /tmp/obs-target-cmake`: **blocked**, expected fatal
  `Windows-only project (spec: Global Constraints).` GNU compiler detected but
  no Windows target compilation occurred. No compiler diagnostic for modified
  Windows code can be claimed. MSVC/SDK/GPU and all C++ Windows/device tests unavailable.
- `git diff --check`: passed before each implementation/test commit.
- Initial `cmake --version` failed (not installed); installing CMake resolved that
  tooling issue but cannot supply the Windows platform. Initial unittest command
  was wrong for pytest-based files and failed imports; corrected invocation above
  passed after installing pytest. No failed experiment was hidden.

### Portable test reproduction (Linux, from repository root)
The following uses a temporary adapter header solely because the pinned Catch2
amalgamation combines its normal headers. It compiles the actual repository test
and actual inline predicate, without simulating Windows APIs.
```
mkdir -p /tmp/obs-native-tests/catch2
curl -fLsS https://raw.githubusercontent.com/catchorg/Catch2/v3.7.1/extras/catch_amalgamated.hpp -o /tmp/obs-native-tests/catch2/catch_amalgamated.hpp
curl -fLsS https://raw.githubusercontent.com/catchorg/Catch2/v3.7.1/extras/catch_amalgamated.cpp -o /tmp/obs-native-tests/catch2/catch_amalgamated.cpp
printf '#include "catch_amalgamated.hpp"\n' > /tmp/obs-native-tests/catch2/catch_test_macros.hpp
g++ -std=c++20 -I src/common -I /tmp/obs-native-tests tests/test_obs_projector_identity.cpp /tmp/obs-native-tests/catch2/catch_amalgamated.cpp -o /tmp/obs-native-tests/obs_identity_tests
/tmp/obs-native-tests/obs_identity_tests
```

### Limitations / decisions / remaining work
- No Windows executable or portable artifact was possible in this Linux host.
- English OBS titles and standard obs64.exe/obs32.exe names only. Query failure
  excludes a window; no directory hardcoding or executable renaming workaround.
  Executable basename is identity filtering, not signature verification.
- Windowed/fullscreen discovery implemented, both unverified live. Windowed
  crop assumes WGC ContentSize equals DWM extended bounds; test Windows 11
  borders and 100%/125%/mixed DPI first. Invalid geometry stops rather than
  copying incompatible resources or capturing an unrelated window.
- Client resizing/minimizing/hiding/closing and no-frame timeout stop capture;
  automatic resource rebuild/reconnect is deferred to keep renderer untouched.
  Same-size monitor moves use the existing tracker; DPI/size change may stop.
- Five-second watchdog needs live static-scene/occlusion testing. OBS must
  deliver frames while covered by the overlay. Avoid a feedback loop from OBS
  Display Capture including the overlay's monitor.
- New controls remain English; some unrelated WoW artwork/taglines remain.
  Original SDR, synthetic depth, motion estimates and passthrough limitations
  remain. Existing masks may need clearing for arbitrary OBS content.
- Capture loss is reported by a generic restart dialog plus sidecar.log; the
  source treats invalid OBS geometry as closed so the existing target-loss path
  hides the overlay and wakes its owner. No downstream rendering fork.
- No neural/runtime/model code changed. There was no existing frame generation
  to preserve; do not promise FPS improvements.
- Still required: full Windows x64 Release compile; all unit tests; live WGC/
  OBS crop, neural activation, motion, scene switching, monitor/size/close/
  restart cases; original WoW regression; GPU tests and portable packaging.
- Recommended improvements after first live results: seamless resizing only
  if needed, localized projector recognition, cached low-frequency validation,
  and a target-specific status summary. Do not prioritize these over validation.

### Git/session rules and delivery
Commits so far: `b750d9c` trace/plan, `975c619` OBS implementation. Subsequent
validation and final documentation commits appear after these in `git log`.
All work is on `obs-target`, main is untouched. Existing workflows were read
only to check triggers: CI/Pages push triggers are main-only, release is tag-only.
Push only obs-target; do not open a PR (pull_request triggers CI), operate Actions,
create tags, publish releases or merge. No automation was created or triggered.
The exact next step remains the Windows build and crop test above. Full manual
expected behavior and runtime packaging details are in `docs/OBS_PROJECTOR.md`.

## Final session checkpoint
`5891e41` added portable identity test coverage and final failure-path review.
README and docs/OBS_PROJECTOR.md are completed and committed with this handoff
update. Review at session end found no unrelated source changes and no workflow
changes. No outstanding compiler errors were observed because Windows compilation
could not begin; the platform configure failure above is the build result.
Next action: Windows SDK-enabled Release build, unit tests, then windowed OBS
projector crop at 100%/125% DPI. Do not mark live capture verified until tested.
