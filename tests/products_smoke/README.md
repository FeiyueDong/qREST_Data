# Products entry smoke test

This independent Xmake project includes only `../../xmake/products.lua`. Its
project version deliberately differs from qrest_data's version, so the test
also catches a generated header that accidentally inherits the parent version.

From this directory:

```sh
xmake config -P . -p linux -m release -o build
xmake show -P . -l targets
xmake build -P .
xmake run -P . products_smoke
```

The target list must contain exactly `qrest_data_headers`, `qrest_data_lib`,
`qrest_data_hdf5`, `qrest_data_import_formats`, `qrest_data_tools_core`,
`qrest_data_tools_cli`, and `products_smoke`. The smoke project's load check
rejects any missing or additional target and verifies version isolation.
