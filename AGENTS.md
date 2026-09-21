# Repository Guidelines

## Project Structure & Module Organization

This repository contains the qREST data management library and tools. Public C/C++ headers live in `include/qrest_data/`. The standalone `xmake.lua` includes products and local tests; `xmake/products.lua` is the products-only entry for parent projects and must not add tests. Core implementations live in `src/`: `qrest_data_lib` provides the shared C ABI library, `qrest_data_hdf5` provides the HDF5 bridge, and `qrest_data_tools` contains CLI, GUI, import, and validation code. Imported external-format parsers live under `src/qrest_data_tools/formats/`. Tests are kept beside their modules in `src/test_qrest_data_lib`, `src/qrest_data_hdf5/test_*.cpp`, and `src/qrest_data_tools/test/`; independent smoke projects are under `tests/installed_sdk_smoke/` and `tests/products_smoke/`. qREST examples are under `resource/qrest_data/`, while tracked MiniSEED and TDMS fixtures are under `resource/wuhan_mseed/` and `resource/wuhan_tdms/`. Protocol and interface documentation is under `doc/`. Xmake is the only maintained build entry point.

## Build, Test, and Development Commands

- `xmake config -p linux -m debug`: configure a Linux debug build. Platforms must be selected explicitly; Windows/MSVC and Linux/GCC are the formal release platforms.
- `xmake build`: build all libraries, tools, and test binaries into `build/<platform>/`.
- `xmake build qrest_data_lib`: build one target while iterating.
- `xmake run test_qrest_data_lib`: run the C++ public-header, version, serialization, and C ABI regression test with its default fixture.
- `xmake run test_qrest_data_c_api`: compile, link, and run a pure C consumer of the public C API.
- `xmake run test_qrest_data_hdf5`: run HDF5 read/write tests; requires system HDF5.
- `xmake run test_qrest_data_import_formats`: run TDMS, modified MiniSEED, and HDF5 bridge regressions; requires the tracked `resource/wuhan_mseed/` and `resource/wuhan_tdms/` fixtures.
- `tests/products_smoke/`: independent parent-project test of the products-only entry; embedded GUI is opt-in via `--qrest_data_enable_gui=y`.
- `xmake run qrest_data_tools_cli pack resource/qrest_data/kunming/metadata.json resource/qrest_data/kunming/data.txt /tmp/sample.qrest`: generate a sample data file.

## Coding Style & Naming Conventions

Use C++20 and follow the root `.clang-format`, `.clang-tidy`, and `.editorconfig` files. C++ files use 4-space indentation, UTF-8, final newlines, trimmed trailing whitespace, and attached braces. Keep target and directory names lowercase with underscores, matching existing names such as `qrest_data_lib` and `qrest_data_tools`. Public headers should be clear and stable; implementation helpers belong in the relevant module directory.

## Testing Guidelines

Add or update tests when changing serialization, metadata handling, byte layout, external-format import behavior, or HDF5 behavior. Name C++ test files `test_*.cpp` and wire new test binaries through the nearest `xmake.lua`. Run the focused test target first, then `xmake build` before handing off broader changes.

## Commit & Pull Request Guidelines

Recent history uses short, imperative summaries such as `update proj dir` and `add new data`. Keep commits focused and mention the touched area when helpful, for example `update hdf5 reader`. Pull requests should describe the data-format or API impact, list tests run, link related issues, and include screenshots only for `qrest_data_tools_gui` UI changes.

## Security & Configuration Tips

Do not commit generated build outputs from `build/`, `out/`, or `.xmake/`. Keep sample data small and non-sensitive. Declare dependency changes in the relevant Xmake files.
