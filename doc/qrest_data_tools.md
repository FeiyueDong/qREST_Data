# qrest_data_tools

`qrest_data_tools` provides command-line import, export, validation, pack, and
extract utilities for qREST data files.

## External Imports

TDMS and modified MiniSEED imports accept either one instrument file or a
directory of instrument files:

```sh
qrest_data_tools import tdms <tdms-file-or-dir> <metadata.json> <output.qrest>
qrest_data_tools import mseed <mseed-file-or-dir> <metadata.json> <output.qrest>
```

When the input is a directory, files are sorted by filename. The default merge
strategy treats sorted file number as the measurement point number and maps
channel directions into qREST channel IDs:

- `E` and `EIE` map to `Xn`
- `N` and `EIN` map to `Yn`
- `Z` and `EIZ` map to `Zn`

For example, the first sorted file maps its east/north/vertical channels to
`X1`, `Y1`, and `Z1`; the second file maps to `X2`, `Y2`, and `Z2`.

Before writing a `.qrest` file, the merged external data is checked against the
provided metadata. `InstrumentInfo.ChannelNum`, `DataInfo.NPTS`, and the sample
rate implied by `DataInfo.DT` must match. The tool reports mismatches and does
not modify metadata automatically.

TDMS physical conversion requires a positive sensitivity. Use `--counts` to
import raw integer counts when a file stores no usable `Parmt/Sen` value.
