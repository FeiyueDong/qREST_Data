# qrest_data_tools_cli

The CLI and `qrest_data_readers` SDK share the external reading, conversion and
metadata validation implementation. Generic numeric TXT matrices still use
`pack` / `validate text`; SIT software text uses the explicit `sit-txt` selector.

## Supported inputs

| Selector | Supported structure | Default output |
| --- | --- | --- |
| `mseed` | Standard MiniSEED 2 or existing `wc/wt/ws` Modified MiniSEED; Steim-2 (11) | SI physical values when calibrated; otherwise `RawCounts` |
| `sit-dat` | Recognized 512-byte SIT-4S container header, length-prefixed one-second blocks, marked Modified MiniSEED records | `cm/s^2` (Gal) for acceleration |
| `sit-txt` | Explicit SIT Gal profile, Tab-separated UD/NS/EW plus millisecond time, GBK or UTF-8 header/BOM | `cm/s^2` (Gal) |
| `tdms` | Existing supported TDMS profile | Existing behavior (`cm/s2`) |

MiniSEED 2 follows the [FDSN SEED 2.4 manual](https://www.fdsn.org/seed_manual/SEEDManual_V2.4.pdf):
record length comes from Blockette 1000, the Blockette chain is checked, fixed
header and payload byte order are independent, Blockette 100 sample rates and
1001 microsecond offsets are honored, and unapplied fixed-header time correction
is applied. Supported record lengths are 128 bytes–1 MiB. Unsupported encodings
and MiniSEED 3 produce explicit errors. Leap-second BTime values are currently
rejected rather than silently mapped onto another UTC second. Supported BTime
calendar years are 1900–2200. Modified records retain their packet-length and
native dimension/sensitivity checks; standard records never read the proprietary
extension or infer calibration from channel names.

The DAT signature `!SIT-4S Zip File` does not denote ZIP. This profile uses a
512-byte outer header and two little-endian block control words (counter and
payload byte length). Record lengths and the number of channels come from the
marked inner records, checked against the block boundary and the preceding
block's channel identities. The block counter and record times must advance by
one second; all channels must share a rate and start time. Other container
layouts are rejected with the block index and byte offset. Unknown outer header
fields are not treated as verified device metadata.

SIT TXT preserves its actual source column order. The known labels are `UD向`,
`NS向`, `EW向`, and `时间`; unknown or duplicate labels are errors. Numeric fields
must be finite. Empty/interrupted lines, duplicate timestamps and variable
sampling intervals are errors with line numbers. CRLF and final newlines work;
blank rows are deliberately rejected. A filename `SRate` marker, if present,
is cross-checked against the timestamps. Selecting `sit-txt` explicitly selects
the **Gal profile**; files in other units need a separate supported profile.
No filename renaming to `.sit.txt` is required.

## Validate and import

```sh
qrest_data_tools_cli validate mseed input.mseed
qrest_data_tools_cli validate sit-dat input.dat
qrest_data_tools_cli validate sit-txt input.txt --timezone +08:00

qrest_data_tools_cli import sit-dat input.dat metadata.json out.qrest
qrest_data_tools_cli import sit-txt input.txt metadata.json out.qrest \
    --timezone +08:00 --channel-map 3,1,2
qrest_data_tools_cli import mseed input.mseed metadata.json out.qrest \
    --sensitivity 100000 --quantity Acceleration --unit cm/s2
```

`validate` decodes and reports without writing qREST. Summaries include source
format, source identities/order/direction hints, records, sample count/rate,
UTC start/last sample time in microseconds, output unit, calibration origin,
missing/non-finite samples and diagnostics. Overlaps and undecodable records
are fatal, so successful summaries report zero for those counters.

MiniSEED keeps its previous SI default. New SIT inputs default to Gal.
`--unit m/s2` or `--unit cm/s2` requests acceleration output; other quantities conflict with these explicit units. Missing calibration is
then an error. `--sensitivity` is the **actual count per SI-unit** sensitivity,
not the stored Modified MiniSEED sensitivity (which is 100 times that value).
It requires `--quantity Acceleration|Velocity|Displacement`. Verifiable native
calibration takes precedence; conflicting user calibration/quantity is rejected.
Uncalibrated standard MiniSEED remains readable as counts. `--counts` requests
raw values and excludes physical conversion options. To write counts, metadata
must explicitly use `Units[0]="count"`, `Measurand="RawCounts"` and `Scale=1`.
Counts can never be written under acceleration metadata.

TXT timestamps have no zone. The SIT Gal profile defaults explicitly to `+08:00`;
`--timezone` accepts numeric offsets within ±14:00. The summary records this
assumption and the resulting UTC time. This rule does not apply to arbitrary
text exports. The supplied TXT 22:00+08:00 corresponds to binary 14:00 UTC.

## Mapping and quality contracts

Readers preserve source order; direction hints only recognize explicit
`EIN/NS/N`, `EIE/EW/E`, `EIZ/UD/Z` labels. They do not infer building X/Y axes.
Sequential mapping is retained for compatibility and printed by the CLI.
`--channel-map` lists the **one-based target channel number for each source**:
TXT UD/NS/EW → metadata NS/EW/UD uses `3,1,2`. A known direction conflict, missing,
duplicate or out-of-range mapping is rejected. Building geometry remains
user-supplied; no coordinate rotation is performed.

Before writing, channel/sample counts, DT, finite values, UTC StartTime, output
units, quantity and Scale=1 must agree with metadata. A source start with
sub-millisecond precision that cannot be represented by the current qREST time
contract is rejected at import, but remains available when reading/validating.
Metadata is never edited automatically.

MiniSEED `--gap-policy fill-nan` preserves gaps as NaN (default); `error` rejects
gaps; `ignore` explicitly concatenates records and reports that the true time
axis may be lost. Overlaps and off-grid jumps are rejected except under explicit
`ignore`. NaN-filled data cannot be written to qREST. There is no zero filling,
resampling, interpolation or automatic deletion of bad records.

Directory inputs filter by the selected format (`.mseed/.miniseed`, `.dat`,
`.txt`, `.tdms`) and present files in filename order. Individual explicit-format
inputs are identified by structure regardless of extension. Per-record MiniSEED
time order is reconstructed before checking gaps. Directory files must describe
synchronous, distinct source channels; duplicate exports or mismatched start
points/rates/counts are rejected. Time-segment files should first be assembled
into a single stream; filenames are never used to infer missing timestamps.
TDMS retains its existing collection semantics. The new TDMS file under
`resource/example_data` is outside this change; old Wuhan TDMS regressions remain.

## MiniSEED time precision

The integer `start_time_us` retains the decoded timestamp; it does not imply
that the source encoded microseconds. The internal header tracks whether
Blockette 1001 is present: BTime alone has a 100 us encoding step, while 1001
provides a signed microsecond remainder and a 1 us step. Duplicate Blockette
100/1001 declarations are rejected. Blockette 100's valid actual sample rate
continues to take precedence over fixed-header rate factors.

For two timestamp references with encoding steps `q1` and `q2`, sample-grid
checks use a quantization budget `(q1 + q2) / 2`, with a small arithmetic guard
scaled to relative elapsed time. That budget is about 100 us for two BTime
records and 1 us for two microsecond records. The complete tolerance must stay
**strictly below half a sample interval**; otherwise the reader reports
`insufficient timing precision` instead of choosing an ambiguous sample offset.
The same rule is used for record continuity and channel start/end alignment.

A timestamp residual near zero means quantization of a continuous boundary;
a residual near a nonzero integer sample offset means a real gap or overlap.
All records must also admit one common quantization-phase window on the grid
anchored at the first record. Adjacent records are checked independently so a
coarse first timestamp cannot hide later microsecond errors. The phase windows
are intersected across channels too. Closed endpoints preserve legitimate
halfway/round-to-even quantization; the window is an integrity bound, never a
correction applied to stored timestamps or values.

Calculations subtract integer timestamps before floating-point division and
use total sample counts for theoretical elapsed times. This avoids accumulated
roundoff and subtracting large Unix-epoch floating-point values. Errors include
stream identity, previous/current timestamp, theoretical next timestamp,
residual, encoding steps, sample interval and tolerance where relevant.
75/120 Hz non-integer record durations work with ordinary time checking enabled.
Gaps still follow the selected policy; there is no resampling or clock repair.

## SDK boundary

Include `<qrest_data/external_reader.h>` and link **`qrest_data_readers`**, a
separate shared library. `qrest_data_lib` retains its existing serialization C
ABI and does not acquire these new entry points. Internal C++ parent projects
can continue linking `qrest_data_tools_core`; CLI and shared readers use it too.

```c
qrest_external_dataset *data = NULL;
if (qrest_external_read("input.mseed", "mseed",
    "{\"sensitivity\":100000,\"quantity\":\"Acceleration\",\"unit\":\"cm/s2\"}", &data) == 0) {
    const double *values = qrest_external_values(data); /* channel-major */
    const char *summary = qrest_external_summary(data); /* UTF-8 JSON */
    /* values and summary belong to data; inspect or write before freeing. */
    qrest_external_free(data);
}
```

Options are a JSON object: `unit`, `counts`, `sensitivity`, `quantity`,
`gap_policy`, `group_index`, `include_dimensionless`; TXT accepts only `unit`
and `timezone`. Unknown or inapplicable options are rejected. NULL options use
format defaults. Errors return -1, set the output handle to NULL, and are
available through thread-local `qrest_external_last_error()`.

`qrest_external_write_qrest()` accepts user metadata and an optional **zero-based**
target index for each source, validates the same contracts as the CLI, and writes
through the existing qREST serializer. Borrowed values/report remain valid until
`qrest_external_free()`. See the public header for ownership and null handling.
Linux readers use system HDF5; Windows packages must carry required HDF5 DLLs.

## Regression commands

```sh
xmake config -p linux -m release
xmake build test_qrest_data_format_readers
xmake run test_qrest_data_format_readers
# Optional full one-hour cross-format test (requires the local example_data):
xmake run test_qrest_data_format_readers --full
xmake run test_qrest_data_import_formats
python3 scripts/test_format_reader_cli.py /absolute/path/to/qrest_data_tools_cli
xmake build
```

Small 1-second fixtures and fixture-owned metadata live under
`resource/test/format_readers`. `scripts/make_reader_fixtures.py` reproduces the
waveform extracts and records the full source hashes. The full local example
files remain ignored. Independent products/installed SDK projects cover the new
target and C/C++ consumers; Windows/MSVC, MinGW and clean-machine runtime gates
require their respective environments.

本轮实际执行结果见 [重构验证记录](reader_refactor_validation.md)。

Linux developer memory/time baseline (creates a new scratch directory):

```sh
python3 scripts/benchmark_format_reader.py /absolute/path/to/qrest_data_tools_cli \
    --work-dir /tmp/qrest_reader_benchmark --channels 63 --rate 200 --seconds 3600
```

The synthetic load repeats the project-owned fixture's legal Steim-2 payload;
it measures whole-process wall time and Linux peak RSS for validate/import.
It is not a compression-quality or realistic wavefield benchmark. The output
includes dimensions, byte counts, hashes and per-command logs/measurements.
