"""Assemble a private OBS test bundle from a verified upstream runtime folder.

No downloads, personal config, SDKs or system DLLs are included. The existing
PowerShell release packager remains the normal MSVC release route.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ci"))
from check_imports import forbidden_imports, parse_imports, check_no_elevation_manifest


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    parser.add_argument("--msvc-dir", type=Path, required=True)
    parser.add_argument("--notices-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).strip():
        raise RuntimeError("Commit source changes before packaging")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    cache = (args.build_dir / "CMakeCache.txt").read_text()
    if "SIDECAR_REQUIRE_SDKS:BOOL=ON" not in cache or "CMAKE_BUILD_TYPE:STRING=Release" not in cache:
        raise RuntimeError("An SDK-enabled Release build is required")
    manifest = json.loads((args.runtime_dir / "BUILD-MANIFEST.json").read_text(encoding="utf-8-sig"))
    for entry in manifest["files"]:
        relative = Path(entry["name"].replace("\\", "/"))
        if relative.is_absolute() or ".." in relative.parts:
            raise RuntimeError("Unsafe upstream manifest path")
        path = args.runtime_dir / relative
        if path.stat().st_size != entry["bytes"] or digest(path) != entry["sha256"]:
            raise RuntimeError(f"Upstream integrity mismatch: {relative}")
    version = re.search(r"project\(.*VERSION\s+([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
    name = f"dlss5-wow-sidecar-v{version}-obs-target-win64-{commit[:7]}"
    bundle = args.output_dir / name / "dlss5-wow-sidecar"
    archive = args.output_dir / (name + ".zip")
    if bundle.exists() or archive.exists():
        raise RuntimeError("Use a fresh output directory; artifacts are never overwritten")
    shutil.copytree(args.runtime_dir, bundle)
    (bundle / "BUILD-MANIFEST.json").unlink()
    for exe in ("wowsidecar.exe", "wowsidecar-manager.exe"):
        source = args.build_dir / exe
        if forbidden_imports(parse_imports(source)) or not check_no_elevation_manifest(source):
            raise RuntimeError(f"Product invariant check failed: {exe}")
        if pefile.PE(str(source), fast_load=True).FILE_HEADER.Machine != 0x8664:
            raise RuntimeError(f"Not Windows x64: {exe}")
        shutil.copy2(source, bundle / exe)
    for name in ("README.md", "CODEX_HANDOFF.md"):
        shutil.copy2(ROOT / name, bundle / name)
    for path in (ROOT / "docs").glob("OBS*.md"):
        shutil.copy2(path, bundle / "docs" / path.name)
    runtimes = []
    for source in args.msvc_dir.glob("*.dll_amd64"):
        if pefile.PE(str(source), fast_load=True).FILE_HEADER.Machine != 0x8664:
            raise RuntimeError(f"Not an AMD64 redistributable: {source.name}")
        destination = bundle / source.name.removesuffix("_amd64")
        shutil.copy2(source, destination)
        runtimes.append(destination.name)
    if not {"msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"}.issubset(runtimes):
        raise RuntimeError("Missing required app-local MSVC runtimes")
    shutil.copytree(args.notices_dir, bundle / "notices", dirs_exist_ok=True)
    tests = bundle / "tests"
    tests.mkdir()
    for source in (args.build_dir / "tests").glob("*.exe"):
        shutil.copy2(source, tests / source.name)
    if not (tests / "sidecar_tests.exe").exists() or not (tests / "testpattern.exe").exists():
        raise RuntimeError("Build the Windows tests and testpattern before packaging")
    (tests / "Run unit tests.cmd").write_bytes(
        b'@echo off\r\ncd /d "%~dp0"\r\nset "PATH=%~dp0..;%PATH%"\r\n'
        b'sidecar_tests.exe "[unit]" > unit-test-results.txt 2>&1\r\n'
        b'type unit-test-results.txt\r\npause\r\n')
    (bundle / "START_HERE.txt").write_text(
        "OBS Projector test build - Windows 11 x64, RTX 40/50\n\n"
        "Release cross-compiled with both neural SDK interfaces enabled.\n"
        "Live Windows/OBS/neural rendering is NOT yet verified.\n\n"
        "1. Extract this entire folder outside the WoW and OBS directories.\n"
        "2. Launch OBS in English and add your source.\n"
        "3. Open a Windowed or Fullscreen Projector (Program).\n"
        "4. Launch wowsidecar-manager.exe.\n"
        "5. Choose Capture target: OBS Studio Projector.\n"
        "6. Refresh and explicitly select the projector, then Start overlay.\n"
        "7. Ctrl+Alt+D toggles processed/original. Ctrl+Alt+Backspace stops it.\n"
        "8. Check sidecar.log for optical flow available and DLSS/DLAA feature\n"
        "   creation, plus no passthrough/failure warnings. A running overlay\n"
        "   or selected neural pass name alone does not prove neural filtering.\n"
        "9. Stop, then select World of Warcraft to test the original mode.\n\n"
        "RTX 4070 Super: Automatic uses the compatible Ada runtime.\n"
        "Minimize/resize/close may stop capture; restore, reselect and restart.\n"
        "Avoid OBS Display Capture feeding this overlay back into its source.\n"
        "No Visual Studio, SDKs, Python or separate VC runtime install is needed.\n"
        "If it fails, keep sidecar.log before launching again (it is overwritten).\n"
        "Optional Windows unit tests: tests/Run unit tests.cmd.\n",
        encoding="utf-8")
    files = []
    for path in sorted(bundle.rglob("*")):
        if path.is_file():
            files.append({"name": path.relative_to(bundle).as_posix(), "bytes": path.stat().st_size,
                          "sha256": digest(path)})
    record = {"version": version, "branch": "obs-target", "sourceCommit": commit,
              "build": "Windows x64 Release, clang-cl 19.1.7 / lld, MSVC CRT 14.44, SDK 10.0.26100",
              "neuralSdkEnabled": True, "liveWindowsVerified": False,
              "upstreamRuntimeVersion": manifest["version"],
              "neuralRuntimeSha256": manifest["neuralRuntimeSha256"],
              "appLocalMsvcRuntime": runtimes, "files": files}
    (bundle / "BUILD-MANIFEST.json").write_text(json.dumps(record, indent=2) + "\n")
    sums = [f"{entry['sha256']}  {entry['name']}" for entry in files]
    (bundle / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for path in sorted(bundle.rglob("*")):
            if path.is_file():
                zipped.write(path, "dlss5-wow-sidecar/" + path.relative_to(bundle).as_posix())
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise RuntimeError("ZIP CRC verification failed")
        for entry in files:
            if hashlib.sha256(zipped.read("dlss5-wow-sidecar/" + entry["name"])).hexdigest() != entry["sha256"]:
                raise RuntimeError("ZIP member hash verification failed")
    (archive.with_suffix(".sha256")).write_text(f"{digest(archive)}  {archive.name}\n")
    print(json.dumps({"artifact": str(archive.resolve()), "bytes": archive.stat().st_size,
                      "sha256": digest(archive), "manifestFiles": len(files)}, indent=2))


if __name__ == "__main__":
    main()
