# Dataset editing Core

`qrest_data_tools_core` exports `dataset_draft.hpp` through its target include path. It has no Qt dependency. The `qrest_module` frontend consumes this product; this project also provides the CLI.

A `DatasetDraft` owns three independent objects: Metadata with stable channel UIDs, source ChannelData facts (sample count/rate/optional start time plus immutable channel-sequential samples), and one-to-one data-column-to-Metadata bindings. ChannelNo is display order, never identity. Metadata edits, deletion, duplication, reorder and binding operations share the original sample buffer. Adding/duplicating Metadata creates an unallocated channel. Removing Metadata only unbinds its waveform.

First coexistence binds min(Metadata channels, real columns) in order. Later additions/append remain unbound. Full Metadata JSON import creates new UIDs and clears bindings; explicit sequential binding is available. Full waveform replacement creates new real-column identities and initial bindings. Append checks known source facts against the existing effective time axis; source-unknown TXT uses the global Metadata axis. Metadata dimensions/rate/units may differ in an editable draft. Structurally corrupt source matrices are rejected at import.

`take_over_binding(unbound_column, channel, expected_owner)` atomically transfers an occupied Metadata identity to an unbound real column. Invalid UIDs, a bound recipient, identical columns, and a changed owner are rejected before any binding changes. Bound pairs use `swap_bindings()` instead. `unbind_all_columns()` releases every binding in one transaction. Both operations preserve Metadata, source facts, column order, and the immutable sample buffer; explicit sequential binding can restore the free pairs.

`project_formal_output()` requires each Metadata UID to have one real column and builds a temporary buffer in Metadata order. Extra unbound real columns are warnings and are counted in `FormalPayload.omitted_columns`; they remain in the draft. No UID/binding fields enter `.qrest`. The frontend must confirm omission before publication and keep the complete draft when it still contains omitted waves. There is no fabricated user-confirmation flag in Core.

`from_file` retains original stored values, Scale, source ID and encoding. `from_values` with `DraftSource.normalized_physical=true` stores physical samples with Scale=1 and records their provenance; it does not claim original instrument counts. Binding never changes Scale or samples. A physical source requires Scale=1; a declared stored source Scale must match the bound Metadata.

`ExternalDataset` optionally carries declared channel measurands and distance units. TDMS, modified MiniSEED and HDF5 loaders populate known fields. Final validation rejects bound unit/quantity/representation conflicts; Draft reports them while allowing organization. Missing declarations remain unknown.

`validate(Draft)` reports incomplete work; `validate(Final)` rejects missing dimensions, nonfinite samples/physical products and lossy encoding. `save_new` writes in a sibling staging directory, closes and rereads the file, verifies metadata and stored values, and optionally calls a consumer verifier before publication. Publication uses an exclusive filesystem hard link. Existing files, symlinks and concurrent destinations are never replaced. Unsupported filesystems fail clearly. Staging files are cleaned on failure; published files are retained.

Build `test_qrest_data_dataset_draft`, then run it explicitly. The optional real TDMS/MiniSEED regression requires the documented sample files, absent from some checkouts.

## Unified channel import

`channel_import.hpp` provides `read_channel_import(path, format, transpose_text)` and
`DatasetDraft::import_channels(input, action, scales)`. All formats produce reader
samples and per-channel default factors: reader value times Scale equals m/s².
TXT infers rectangular dimensions from newlines and supports whitespace/comma/semicolon;
transpose makes each row a channel. It supplies no fabricated rate or timestamp.
An unknown source rate is represented by zero; `data_sample_rate()` then uses valid
global Metadata DT, or returns zero until supplied. Final still requires complete
Metadata, while editors can display sample indices before a time axis exists.

MiniSEED/DAT default to native inverse sensitivity, or declared default 1 when absent;
SIT TXT uses its existing Gal/+08:00 profile and factor 0.01. TDMS retains its existing
default calibrated cm/s² reader and factor 0.01. HDF5 factors include stored Scale
and m/cm units. Known non-acceleration sources, incompatible multiple groups, gaps,
nonfinite values and submillisecond origins are rejected. No public C ABI changes.

The import transaction normalizes to physical acceleration in the existing m/cm
geometry unit, updates imported bound channels to Acceleration/Scale=1, and records
source/default/applied factors in Metadata.Extension.ChannelImports. Geometry and
source files remain unchanged. Replace adopts real NPTS and known native rate/time;
Append checks synchronization and leaves added columns unbound. The frontend confirms
overwriting existing waves or nonempty sampling settings before calling Replace.

Build and run `test_qrest_data_channel_import` explicitly; existing reader and draft
regressions remain separate. Linux core evidence does not establish desktop/Windows
or real TDMS parser acceptance.
