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

When the input is a directory, files are sorted by filename and concatenated in
that order. The command-line default maps external channels sequentially to the
metadata channel list. Use the external channel mapping API or GUI import
workflow when source channel labels must be matched to site-specific channel
IDs.

Before writing a `.qrest` file, the merged external data is checked against the
provided metadata. `InstrumentInfo.ChannelNum`, `DataInfo.NPTS`, and the sample
rate implied by `DataInfo.DT` must match. The tool reports mismatches and does
not modify metadata automatically.

TDMS physical conversion requires a positive sensitivity. Use `--counts` to
import raw integer counts when a file stores no usable `Parmt/Sen` value.

Modified MiniSEED import checks the time axis by default. `--gap-policy
fill-nan` preserves the sampling grid and fills missing records with NaN,
`--gap-policy error` keeps the original strict behavior, and `--gap-policy
ignore` concatenates records without checking continuity. `.qrest` import still
requires finite sample values, so NaN-filled gaps are reported instead of being
written silently.
