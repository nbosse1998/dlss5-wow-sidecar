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
Initial source/build/package inspection complete. No functional changes yet.
CMake command unavailable on Linux host; MSVC, Windows SDK, WGC/OBS/GPU unavailable.
No compile or live Windows test claimed. No executable produced.

## Build and test commands (Windows developer command prompt)
Prerequisites: VS 2022 Desktop C++, CMake >=3.28, Python + pefile, Windows SDK
with dxc (release uses 10.0.26100.0 dxc), NVIDIA DLSS SDK v310.7.0 and Optical
Flow SDK 5.0.7 for actual neural execution. Dependencies fetched at pinned tags.
```
python -m pip install pefile
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSIDECAR_REQUIRE_SDKS=ON -DDLSS_SDK_DIR="C:/SDK/DLSS" -DNVOF_SDK_DIR="C:/SDK/OpticalFlow" -DDXC_EXECUTABLE="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
cmake --build build --config Release
build\tests\Release\sidecar_tests.exe "[unit]"
build\tests\Release\sidecar_tests.exe "[device]"
python -m unittest discover -s ci -p "test_*.py"
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

## Next step
Implement OBS discovery, selection, runtime validation, defensive size handling,
and targeted unit tests; then update this handoff with results and manual procedure.

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
