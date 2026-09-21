#!/usr/bin/env python3

"""Create a qrest_data release package from the current Xmake output."""

from __future__ import annotations

import argparse
import platform as host_platform
import re
import shutil
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_ROOT = PROJECT_ROOT / "build"


def parse_args() -> argparse.Namespace:
    default_platform = "windows" if host_platform.system() == "Windows" else "linux"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", nargs="?", choices=("linux", "windows"), default=default_platform)
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


def copy_artifacts(build_dir: Path, package_dir: Path, target_platform: str) -> None:
    if not build_dir.is_dir():
        raise FileNotFoundError(
            f"Xmake output directory not found: {build_dir}. Build the requested platform/mode first."
        )

    copied_library = False
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
                copied_library = copied_library or name == "qrest_data_lib.lib"
        else:
            if name in {"qrest_data_tools_cli", "qrest_data_tools_gui"}:
                copy_file(artifact, package_dir / "bin" / name)
            elif name.startswith("libqrest_data") and (
                ".so" in name or name.endswith(".a") or name.endswith(".dylib")
            ):
                copy_file(artifact, package_dir / "lib" / name)
                copied_library = copied_library or name.startswith("libqrest_data_lib.")

    if not copied_library:
        raise FileNotFoundError(
            f"qrest_data_lib artifact not found in {build_dir}. Build qrest_data_lib first."
        )


def create_package(args: argparse.Namespace) -> Path:
    target_platform = args.platform
    arch = args.arch or ("x64" if target_platform == "windows" else "x86_64")
    version = module_version()
    package_arch = "x64" if arch in {"x64", "x86_64", "amd64"} else arch
    package_dir = args.output_dir.resolve() / f"qrest_data-{version}-{target_platform}-{package_arch}"
    build_dir = BUILD_ROOT / target_platform / arch / args.mode

    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)

    copy_artifacts(build_dir, package_dir, target_platform)
    copy_tree(PROJECT_ROOT / "include" / "qrest_data", package_dir / "include" / "qrest_data")
    copy_file(
        BUILD_ROOT / "generated" / "qrest_data" / "version.h",
        package_dir / "include" / "qrest_data" / "version.h",
    )
    copy_tree(PROJECT_ROOT / "doc", package_dir / "doc")
    copy_tree(PROJECT_ROOT / "resource" / "qrest_data", package_dir / "examples")
    for filename in ("LICENSE.txt", "readme.md", "release.md"):
        copy_file(PROJECT_ROOT / filename, package_dir / filename)

    print(f"Created release package: {package_dir}")
    return package_dir


if __name__ == "__main__":
    create_package(parse_args())
