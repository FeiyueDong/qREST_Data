# Installed SDK smoke test

This project consumes only an installed qrest_data prefix. It must not add the
repository `include/`, `src/`, or `build/generated/` directories.

Run it from this directory after installing qrest_data:

```sh
xmake f -P . -p linux -m release \
    -o build \
    --qrest_data_sdk=/tmp/qrest_data_sdk
xmake -P .
xmake run -P . installed_sdk_smoke
```

The test includes every public header, links `qrest_data_lib`, and checks that
the compile-time and runtime Module Version strings agree.
