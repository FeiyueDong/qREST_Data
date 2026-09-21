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

The target list must contain qrest_data products and `products_smoke`, but no
qrest_data `test_*` targets or GUI by default. Set
`--qrest_data_enable_gui=y` at configuration time to opt in to the GUI.
