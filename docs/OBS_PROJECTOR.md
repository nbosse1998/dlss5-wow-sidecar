# OBS projector target (obs-target branch)

**Status:** implemented; isolated identity rules compiled/tested on Linux.
The Windows application has NOT been compiled or verified with OBS yet.
No modified executable is included. Do not treat an upstream release as this build.

The manager's **Capture target** control appears above every page. Choose
**World of Warcraft** for the original behavior or **OBS Studio Projector**.
In OBS mode choose **Refresh OBS projectors**, then explicitly choose a window.
The list includes title, PID, HWND, client resolution and screen position, so two
identical projector titles are distinguishable. Refresh clears selection. A saved
OBS mode always needs a new live selection after restarting the manager.

OBS discovery enumerates visible, non-minimized top-level windows and checks the
owning executable using query-limited Windows process information. It accepts
`obs64.exe`/`obs32.exe` at any installation path and English titles starting with
`Projector - `, `Windowed Projector (` or `Fullscreen Projector (`. The first is
the current OBS title form; older versions use the latter two. The process and
window are revalidated at launch. It never falls back to another window or WoW.
WoW folder checks and WoW window discovery do not run in OBS mode.

The selected HWND enters the existing WGC -> DeviceBridge -> NVOFA/neural ->
DirectComposition path. An OBS-only crop removes the window frame/title bar.
No neural runtime, presets, hotkeys, masks or rendering algorithms were replaced.
This project applies neural filtering, not frame generation or an FPS boost.

## Build on Windows

Use an **x64 Native Tools Command Prompt for VS 2022**. Install Desktop development
with C++, CMake >=3.28, Python, the Windows SDK including `dxc`, NVIDIA DLSS SDK
v310.7.0 and NVIDIA Optical Flow SDK 5.0.7. SDKs are manual downloads; they are not
vendored. Adjust the two SDK paths below to their actual directories.

```bat
git clone https://github.com/nbosse1998/dlss5-wow-sidecar.git
cd dlss5-wow-sidecar
git checkout obs-target
python -m pip install pefile pytest
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSIDECAR_REQUIRE_SDKS=ON -DDLSS_SDK_DIR="C:/SDK/DLSS" -DNVOF_SDK_DIR="C:/SDK/OpticalFlow" -DDXC_EXECUTABLE="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
cmake --build build --config Release
build\tests\Release\sidecar_tests.exe "[unit]"
python -m pytest ci -q
python ci/check_translations.py
```

`SIDECAR_REQUIRE_SDKS=ON` prevents a seemingly complete build with stub neural
support. For upstream-release toolchain reproduction, see `docs/VERIFYING.md`.
Build configuration: x64 Release; C++20/MSVC; shaders compiled into headers by dxc.
The project fetches Catch2 3.7.1, toml++ 3.4.0 and ImGui 1.91.5 at configure time.

Outputs are raw executables:

- `build\Release\wowsidecar-manager.exe`
- `build\Release\wowsidecar.exe`
- `build\tests\Release\sidecar_tests.exe`

Use a **separate test copy** of a complete upstream portable runtime folder,
outside the WoW/OBS directories. Replace both executables in that copy with the
new build; do not mix one new executable with one old executable. It must contain
`dxgi.dll` (ReShade), `renodx-dlss5.addon64`, `nvngx_dlss.dll`, and both
`runtimes\latest\nvngx_dlssnr.dll` and
`runtimes\compatible\nvngx_dlssnr.dll`. The compatible runtime is the Ada-patched
build needed for RTX 4070 Super. Existing runtime hashes/checks still apply.
Install the MSVC x64 redistributable if needed. These third-party files are not
produced by compiling the repository. The manager will create sidecar.toml and
ReShade.ini; runtime selection stages the appropriate neural DLL at launch.

Optional packaging after committing, building and testing a clean tree:

```powershell
powershell -File tools/package_release.ps1 -BuildDir build -RuntimeDirectory out\sidecar
```

Put the complete runtime files in `out\sidecar` first. The existing packager checks
SDK-enabled configuration and runtime hashes, then writes a portable ZIP and
manifests beneath `out\release`; it does not create an installer. No Actions,
release workflow or PR is needed. Neither binaries nor a ZIP were produced here.

## Manual Windows checklist

Record Windows/OBS versions, GPU/driver, monitor scaling, projector type, resolution
and the source commit. Save sidecar.log immediately after failures (it truncates
on each launch).

1. Launch OBS with **English interface language**. Leave WoW closed and do not
   configure a WoW folder for this test.
2. Add a known source: a video with a face, fine detail and camera movement is
   useful. Add a second scene with visibly different content. Avoid a Display
   Capture source containing the sidecar overlay/projector monitor: that would
   feed the output back into its input.
3. Open **Windowed Projector (Program)** from the Program view/context menu
   (Preview projector in non-Studio mode is also suitable). Keep it visible,
   stationary and unminimized. Start with a fixed 1280x720 or similar client area.
4. Launch the newly built `wowsidecar-manager.exe` from the complete test folder.
5. Choose **Capture target -> OBS Studio Projector** above the page content.
   Expected: WoW setup fields disappear and WoW folder/window/injector checks
   do not block launch. GPU, Windows and runtime checks still apply.
6. Click **Refresh OBS projectors**, then choose the projector by title, position
   and dimensions. Expected: Start becomes available if the general checks pass.
   Open a second projector and repeat: neither should be selected automatically.
7. Press **Start overlay**. Expected: the manager minimizes and the overlay covers
   only the selected projector's client area, with no title-bar pixels, black
   border, offset, stretching or capture feedback. Other projectors remain untouched.
8. Press **Ctrl+Alt+D** repeatedly to compare processed/original output. Check
   Status/sidecar.log for actual neural-pass activity and runtime warnings. A
   running overlay alone does NOT prove neural processing: missing runtime,
   missing optical flow or a failed pass can still produce passthrough. On the
   RTX 4070 Super, **Automatic** should select the compatible runtime. Compare
   1x and 2x strength if the visual change is subtle; do not expect increased FPS.
9. Switch scenes in OBS. Expected: a Program projector follows scene changes and
   processing continues. Source/Scene projectors intentionally keep their chosen
   source/scene; Preview follows Preview in Studio mode.
10. Play motion and pan footage. Expected: continuous updates; inspect temporal
    artifacts, latency and Status timings. Test show/hide, HUD and panic hotkeys.
11. Resize the projector and change monitor/DPI. Expected on a size change:
    capture stops with instructions; restore the desired size, refresh/reselect
    and start again. A same-size monitor move should track its position; a DPI
    or fullscreen transition may change size and require restart. Verify at 100%,
    125% and mixed-monitor scaling. Changing OBS canvas size without changing
    projector dimensions should continue with OBS's own scaling/letterboxing.
12. Minimize, hide, close and reopen the projector; then terminate/restart OBS.
    Expected: overlay stops, never switches to another window, and selection is
    unavailable until a valid window is explicitly selected. If frames simply
    stop, the five-second watchdog should stop capture. Confirm a still image
    source does not trigger spurious timeouts on your OBS version.
13. Repeat 3-12 with **Fullscreen Projector (Program)** on the desired monitor.
    Confirm the full client area is covered, then stop the overlay.
14. Select **World of Warcraft**, choose the original client family and restore
    its normal folder/checks. Launch WoW in borderless mode and start the overlay.
    Expected: original class discovery, checks, presets, runtime selection,
    show/hide/HUD/start-stop hotkeys and game-close shutdown still work.
15. With OBS closed or OBS open without projectors, refresh. Expected: no selectable
    target and Start disabled; no random window captured. Launch runtime directly
    with saved OBS mode: expected instruction to select via the manager, no WoW fallback.

Additional existing GPU tests on a suitable Windows desktop:

```bat
build\tests\Release\sidecar_tests.exe "[device]"
```

## Limitations and next work

- English OBS titles only; localized/custom titles and renamed/custom executable
  builds are intentionally not guessed. OBS-owned windows are identified by
  executable basename, not a cryptographic signature.
- Windowed client cropping assumes WGC ContentSize corresponds to DWM extended
  frame bounds. This is unverified here, particularly for Windows 11 borders and
  mixed DPI. Mismatches stop capture instead of copying incompatible textures.
- Resolution changes, fullscreen transitions and minimization stop capture;
  seamless resize/recovery is deferred. Reopening/restarting needs selection.
- Five-second lack-of-frame detection may need tuning for OBS occlusion/static
  scene behavior. No GPU/OBS performance claims have been verified.
- Existing SDR-only, synthetic-depth/estimated-motion and passthrough fallback
  limitations remain. Existing UI mask rectangles may need clearing for new content.
- New target controls/messages are English; existing translations remain intact.
  Some existing status artwork/taglines remain WoW-themed.

First next step: build x64 Release with both SDKs on Windows, run unit tests, then
perform the windowed-projector crop test at 100%/125% DPI before judging neural
quality. If crop fails, compare logged WGC dimensions with DWM/client rectangles
and correct the OBS-only crop without changing WoW capture or neural processing.
