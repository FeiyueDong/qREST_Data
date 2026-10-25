#!/usr/bin/env python3

"""Create a Windows/MSVC or Linux/GCC package from Xmake output."""

from __future__ import annotations

import argparse
import os
import platform as host_platform
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_ROOT = PROJECT_ROOT / "build"


def parse_args() -> argparse.Namespace:
    default_platform = "windows" if host_platform.system() == "Windows" else "linux"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "platform",
        nargs="?",
        choices=("linux", "windows"),
        default=default_platform,
    )
    parser.add_argument("--arch", default=None, help="Xmake architecture directory")
    parser.add_argument("--mode", choices=("debug", "release"), default="release")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=PROJECT_ROOT / "release",
        help="directory that will contain the versioned package",
    )
    return parser.parse_args()


def module_version() -> str:
    version_header = BUILD_ROOT / "generated" / "qrest_data" / "version.h"
    if not version_header.is_file():
        raise FileNotFoundError(
            f"Generated version header not found: {version_header}. Run xmake config/build first."
        )
    match = re.search(
        r'^#define\s+QREST_DATA_VERSION_STRING\s+"([^"]+)"',
        version_header.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if not match:
        raise RuntimeError(f"Cannot read module version from {version_header}")
    return match.group(1)


def copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Required directory not found: {source}")
    shutil.copytree(source, destination)


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(f"Required file not found: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def copy_artifacts(
    build_dir: Path, package_dir: Path, target_platform: str
) -> None:
    if not build_dir.is_dir():
        raise FileNotFoundError(
            f"Xmake output directory not found: {build_dir}. Build the requested platform/mode first."
        )

    required_artifacts = (
        (
            "qrest_data_lib.dll",
            "qrest_data_lib.lib",
            "qrest_data_tools_cli.exe",
            "qrest_data_tools_gui.exe",
        )
        if target_platform == "windows"
        else ("libqrest_data_lib.so", "qrest_data_tools_cli", "qrest_data_tools_gui")
    )
    missing = [name for name in required_artifacts if not (build_dir / name).is_file()]
    if missing:
        raise FileNotFoundError(
            f"Required Xmake artifacts are missing from {build_dir}: {', '.join(missing)}"
        )

    for artifact in sorted(build_dir.iterdir()):
        if not artifact.is_file():
            continue
        name = artifact.name
        if target_platform == "windows":
            if name.endswith(".dll") or name in {
                "qrest_data_tools_cli.exe",
                "qrest_data_tools_gui.exe",
            }:
                copy_file(artifact, package_dir / "bin" / name)
            elif name.startswith("qrest_data") and name.endswith(".lib"):
                copy_file(artifact, package_dir / "lib" / name)
        else:
            if name in {"qrest_data_tools_cli", "qrest_data_tools_gui"}:
                copy_file(artifact, package_dir / "bin" / name)
            elif name.startswith("libqrest_data") and (
                ".so" in name or name.endswith(".a") or name.endswith(".dylib")
            ):
                copy_file(artifact, package_dir / "lib" / name)


def deploy_windows_gui(package_dir: Path, mode: str) -> None:
    gui_executable = package_dir / "bin" / "qrest_data_tools_gui.exe"
    if not gui_executable.is_file():
        raise FileNotFoundError(
            "Windows release requires qrest_data_tools_gui.exe; build the GUI target first."
        )

    configured_tool = os.environ.get("WINDEPLOYQT")
    candidates = [configured_tool] if configured_tool else []
    candidates.extend(("windeployqt6", "windeployqt"))
    deploy_tool = next(
        (resolved for name in candidates if (resolved := shutil.which(name))),
        None,
    )
    if deploy_tool is None:
        raise FileNotFoundError(
            "windeployqt was not found. Add it to PATH or set WINDEPLOYQT before packaging Windows."
        )

    subprocess.run(
        [
            deploy_tool,
            f"--{mode}",
            "--no-compiler-runtime",
            "--qmldir",
            str(PROJECT_ROOT / "src" / "qrest_data_tools" / "gui" / "qml"),
            "--dir",
            str(package_dir / "bin"),
            str(gui_executable),
        ],
        check=True,
    )

    runtime_suffix = "d" if mode == "debug" else ""
    required_runtime = (
        package_dir / "bin" / f"Qt6Core{runtime_suffix}.dll",
        package_dir / "bin" / f"Qt6Gui{runtime_suffix}.dll",
        package_dir / "bin" / f"Qt6Qml{runtime_suffix}.dll",
        package_dir / "bin" / f"Qt6Quick{runtime_suffix}.dll",
        package_dir / "bin" / "platforms" / f"qwindows{runtime_suffix}.dll",
        package_dir / "bin" / "qml",
    )
    missing = [str(path) for path in required_runtime if not path.exists()]
    if missing:
        raise RuntimeError(
            "windeployqt completed but required Qt runtime files are missing: "
            + ", ".join(missing)
        )


def create_package(args: argparse.Namespace) -> Path:
    target_platform = args.platform
    arch = args.arch or ("x64" if target_platform == "windows" else "x86_64")
    version = module_version()
    package_arch = "x64" if arch in {"x64", "x86_64", "amd64"} else arch
    output_dir = args.output_dir.resolve()
    package_name = f"qrest_data-{version}-{target_platform}-{package_arch}"
    package_dir = output_dir / package_name
    build_dir = BUILD_ROOT / target_platform / arch / args.mode

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".qrest_data_release_", dir=output_dir
    ) as staging:
        staged_package = Path(staging) / package_name
        staged_package.mkdir()

        copy_artifacts(build_dir, staged_package, target_platform)
        if target_platform == "windows":
            deploy_windows_gui(staged_package, args.mode)
        copy_tree(
            PROJECT_ROOT / "include" / "qrest_data",
            staged_package / "include" / "qrest_data",
        )
        copy_file(
            BUILD_ROOT / "generated" / "qrest_data" / "version.h",
            staged_package / "include" / "qrest_data" / "version.h",
        )
        copy_tree(PROJECT_ROOT / "doc", staged_package / "doc")
        copy_tree(PROJECT_ROOT / "resource" / "qrest_data", staged_package / "examples")
        for filename in ("LICENSE.txt", "readme.md", "release.md"):
            copy_file(PROJECT_ROOT / filename, staged_package / filename)

        backup_package = Path(staging) / f"{package_name}.previous"
        if package_dir.exists():
            package_dir.rename(backup_package)
        try:
            staged_package.rename(package_dir)
        except OSError:
            if backup_package.exists():
                backup_package.rename(package_dir)
            raise

    print(f"Created release package: {package_dir}")
    return package_dir


if __name__ == "__main__":
    create_package(parse_args())
