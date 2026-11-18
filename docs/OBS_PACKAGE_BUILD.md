# Portable OBS build/package attempt

## Current result

Source: `nbosse1998/dlss5-wow-sidecar`, branch `obs-target`, baseline
`a101fd5034b077550fdcda657ae0db638022f58d`. No modified Windows executables or
ready-to-test ZIP have been produced. The OBS/WoW implementation was not changed.

The user wants the build and packaging handled in the cloud, not a local developer
setup guide. Do not ask them to install developer SDKs or compile the application.

## Exact blocker

The xwin 0.10.0 Microsoft SDK/CRT downloader was downloaded and its archive SHA-256
verified against the publisher's checksum:
`d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88`.

Running it without acceptance produced:

```
Do you accept the license at https://go.microsoft.com/fwlink/?LinkId=2086102 (yes | no)?
Error: unknown response to license request
```

Input was EOF; no acceptance was supplied, no `--accept-license` flag was used,
and Microsoft SDK/CRT download/build was not attempted past this gate. The user
specifically asked to stop at license acceptance that requires their action.
The link currently redirects to:
https://visualstudio.microsoft.com/license-terms/mlt031519/

**Smallest next user action:** review that linked license and explicitly authorize
acceptance for this cloud build. No tools need installing on the user's PC.
After acceptance, use xwin to obtain Microsoft headers/libs in the cloud, acquire
LLVM clang-cl/lld and DXC, then attempt the Windows x64 Release cross-build. This
route is plausible, not yet proven for this WinRT/MSVC project. Do not claim that
acceptance alone guarantees a finished build; report any subsequent concrete gate.

Exact gated command in this session:
```
cd /workspace/scratch/27301ec1376f/build-deps
./xwin-0.10.0-x86_64-unknown-linux-musl/xwin --cache-dir xwin-cache splat --output winsdk
```
Only after explicit acceptance, add `--accept-license` before `splat`.
Microsoft tools are build dependencies and must not be redistributed wholesale.

## Upstream download and integrity inspection

Downloaded the actual official archive, not the GitHub source ZIP:
https://github.com/xilla420/dlss5-wow-sidecar/releases/download/v0.7.0/dlss5-wow-sidecar-v0.7.0.zip

- Size: 355,778,868 bytes.
- SHA-256: `e788f7a199f0d51cf83b67c3caffd7e3fd0ff0b8ef33ec1d8f78877cb347ed6c`.
- Whole archive matches the upstream release checksum.
- All 49 members listed in upstream SHA256SUMS-v0.7.0.txt matched.
- All 49 file size/hash entries inside BUILD-MANIFEST.json also matched.
- Archive has 50 non-directory files, including BUILD-MANIFEST.json, and two empty
  directory entries. ZIP member separators are backslashes. Normalize them before
  extracting on Linux; Python zipfile does not recognize those directory entries
  automatically. An initial slash-sensitive audit reported zero matching names;
  it was corrected and all hashes were then verified, not assumed.
- Manifest source commit is `9dd37f04ed242c3b75ba0289d1b8b05be3e1d071` (release
  branch before merge), while fork baseline main is the resulting merge commit.
- Manifest toolchain: VS 2022 x64, MSVC 14.44.35207 / cl 19.44.35228.0, Windows SDK
  10.0.28000.0, dxc 1.8.2502.11 from SDK 10.0.26100.0, DLSS v310.7.0 at
  a291cc7d2cc642a51566f3dfd5376f635cd1b284, Optical Flow SDK 5.0.7.

## Actual portable layout and dependency classification

| Component | Role | Treatment for modified package |
| --- | --- | --- |
| wowsidecar.exe, wowsidecar-manager.exe | Product executables | Rebuild both from obs-target; original binaries do not contain OBS mode |
| dxgi.dll (5,592,064 bytes) | ReShade runtime | Reuse exact upstream bytes; BSD notice obligations apply |
| renodx-dlss5.addon64 (573,440 bytes) | RenoDX add-on runtime | Reuse exact upstream bytes with MIT attribution; upstream identifies a development build |
| nvngx_dlss.dll (58,956,400 bytes) | Optional upscaling runtime | Present upstream; preserve compatible version subject to NVIDIA terms |
| runtimes/latest/nvngx_dlssnr.dll (266,424,944 bytes) | Stock RTX 50 neural runtime | Present upstream; preserve subject to NVIDIA terms |
| runtimes/compatible/nvngx_dlssnr.dll (165,840,496 bytes) | Ada-patched RTX 40 neural runtime | Present upstream and needed for user's RTX 4070 Super; see licensing caveat below |
| LICENSE, THIRD-PARTY-NOTICES.md, font OFL notices, docs | Attribution and documentation | Preserve; add current fork docs/handoff and identify modified build |
| BUILD-MANIFEST.json | Source/toolchain/file provenance | Regenerate for actual modified executables; never reuse original manifest as build proof |
| sidecar.toml, ReShade.ini | User/generated settings | Not bundled upstream; generated at runtime, do not copy personal settings |
| .cso shader files | Shader output | Not bundled; CMake compiles shaders into executable headers/resources |
| Artwork and fonts | UI resources | Embedded in manager; upstream archive includes notices/provenance, not loose resource fonts |
| MSVC/Windows SDK, xwin, clang-cl/lld, dxc, CMake, Python/pefile, Catch2/toml++/ImGui | Build/test only | Obtain/use in cloud; omit development tools from final portable package |
| DLSS SDK headers and nvsdk_ngx_d.lib | Build only | Public NVIDIA/DLSS repo; pinned release has required headers/library |
| Optical Flow SDK NvOFInterface headers | Build only | Full official SDK download is developer-program/license gated; D3D12 header availability needs resolving before build |
| NVIDIA optical-flow driver DLL | Runtime driver component | Comes with compatible NVIDIA display driver; not SDK-installed on user's PC |
| MSVCP140.dll, VCRUNTIME140.dll, VCRUNTIME140_1.dll | MSVC runtime | Imported by both upstream EXEs but NOT bundled; inspect lawful app-local redistributable option to avoid asking user to install developer tools |
| D3D11/D3D12/DComp/WGC, D3DCOMPILER_47.dll and other Windows DLLs | OS runtime | Provided by supported Windows 11; don't copy system DLLs into package |

All seven upstream executable/DLL/add-on PE files inspected are AMD64 (0x8664).
Both upstream product EXEs passed ci/check_imports.py. These checks describe the
original release only and do not verify the future modified build.

**Licensing caveat, not resolved by a checksum:** upstream THIRD-PARTY-NOTICES.md
explicitly says redistribution of its modified Ada runtime is not covered by
NVIDIA's own terms. Upstream including a file does not itself prove permission
to redistribute it. Retain provenance/terms, distinguish private user testing
from a public release, and do not publish a repackaged proprietary runtime without
resolving applicable rights. No modified runtime was made or distributed here.

## Build-route investigation

- This cloud host is Linux x86_64. No wine, clang-cl, lld-link, native Windows
  compiler, Windows SDK, QEMU/KVM device, Docker or Podman was installed/found.
  No Windows remote runner is exposed by available tools.
- A fresh clone was created on obs-target, tracking origin/obs-target. Use this
  checkout rather than the earlier scratch clone with duplicate commit metadata.
- The native project requires Windows, C++20, MSVC-style flags and C++/WinRT.
  Native Linux GCC alone cannot build its Windows WGC implementation.
- xwin's documented route provides Microsoft CRT/SDK, including C++/WinRT, for
  clang-cl cross-compilation. It was chosen for investigation rather than handing
  the build back to the user; currently stopped at its license gate.
- Microsoft's DirectXShaderCompiler publishes Linux x64 DXC builds; use one for
  shader headers if cross-building. A different DXC will not reproduce upstream
  executable bytes, which is acceptable for a separately identified modified build.
- NVIDIA's official Optical Flow 5.0.7 download link advertises developer-program
  membership and license acceptance. A public copy of nvOpticalFlowD3D12.h with
  NVIDIA MIT header permission was located in 2600th/dlss5-video-player's
  external/nvof. This is only a lead: header version/provenance/compatibility must
  be checked, and it has NOT been used or substituted into the build.
- No GitHub Actions, workflow edits, tags, releases, PRs or external build jobs
  were created, dispatched or operated. Workflow YAML was read as reference only.

## Tests this session

- Installed pytest, pefile and CMake in cloud user environment.
- `python -m pytest ci -q`: 32 passed; one external pytest-plugin deprecation warning.
- `python ci/check_translations.py`: all nine tables passed.
- Recompiled actual tests/test_obs_projector_identity.cpp against actual repository
  ObsProjectorRules.h with GCC/C++20 and pinned Catch2 3.7.1 amalgamation:
  1 test case, 9 assertions passed. Not a Windows build or Win32 integration test.
- `python ci/check_imports.py <upstream wowsidecar.exe> <upstream manager.exe>`:
  both passed; reference binaries only.
- Initial pytest invocation found pytest absent in this refreshed runtime;
  installation and re-run resolved it. No Windows tests, config C++ tests,
  GPU/device tests, OBS capture tests or neural processing were executed.
- No source modifications, compiler build, packaging of modified executables or
  artifact launch verification occurred past the consent gate.

## Resumable scratch files

These are reproducible downloads/intermediates, not the deliverable:

- Checkout: /workspace/scratch/27301ec1376f/obs-package
- Original ZIP: /workspace/scratch/27301ec1376f/build-deps/upstream-v0.7.0.zip
- Normalized runtime tree: /workspace/scratch/27301ec1376f/build-deps/upstream-normalized/dlss5-wow-sidecar
- xwin: /workspace/scratch/27301ec1376f/build-deps/xwin-0.10.0-x86_64-unknown-linux-musl/xwin
- Native predicate test: /tmp/obs-package-native/obs_identity_tests

If scratch is gone, re-download from the pinned links/checksums above. Keep final
source/build documentation in GitHub and save any finished user artifact durably.

## Continuation after acceptance

1. Fetch/check obs-target and read CODEX_HANDOFF.md plus this file.
2. Only if the user's acceptance is recorded in the session, pass xwin's license
   flag; never infer acceptance from elapsed time or from this document.
3. Install/extract LLVM clang-cl/lld/llvm-rc in cloud; obtain SDK libraries and
   Linux DXC; resolve compatible NVIDIA build headers/library with their terms.
4. Add a focused cross-toolchain configuration only if required; preserve the
   existing WoW/OBS code and SDK-enabled neural build. Attempt Release compilation,
   fix actual compiler issues, and run every feasible test. If Windows execution
   requires Wine/actual Windows, distinguish that from compilation and GPU tests.
5. Replace both product EXEs in a staging copy of the verified upstream layout;
   preserve compatible runtimes and notices where rights permit. Resolve app-local
   VC runtime dependencies. Regenerate manifest/checksums and audit every member.
6. Create a uniquely named OBS test ZIP, not the original upstream release name,
   and provide it. Never call a source ZIP or upstream unchanged ZIP the deliverable.
7. Update handoff with compiler/test/package outputs and any exact remaining gate.

Actual OBS/WoW capture, neural activity, motion and DPI/resize behavior will still
require a compatible Windows/NVIDIA/OBS environment; no cloud GPU claims were made.
