# Dataset editing Core

`qrest_data_tools_core` exports `dataset_draft.hpp` through its target include path. It has no Qt dependency. The Widgets frontend consumes this product; QML GUI and CLI remain available.

A `DatasetDraft` owns Metadata and shared immutable channel-sequential samples. Metadata editing does not copy samples. Column selection, duplication, deletion and import commit metadata and waveforms together. New channels require explicit waveforms when existing data is present. No implicit zero padding, resampling or time alignment occurs. Failed transactions leave the draft unchanged.

`from_file` retains original stored values, Scale, source ID and encoding. `from_values` with `DraftSource.normalized_physical=true` stores physical samples with Scale=1 and records their provenance; it does not claim original instrument counts. Changing Scale changes physical interpretation without rewriting stored samples.

`ExternalDataset` optionally carries declared channel measurands and distance units. TDMS, modified MiniSEED and HDF5 loaders populate known fields. Import rejects declared unit/quantity conflicts. Missing declarations remain unknown.

`validate(Draft)` reports incomplete work; `validate(Final)` rejects missing dimensions, nonfinite samples/physical products and lossy encoding. `save_new` writes in a sibling staging directory, closes and rereads the file, verifies metadata and stored values, and optionally calls a consumer verifier before publication. Publication uses an exclusive filesystem hard link. Existing files, symlinks and concurrent destinations are never replaced. Unsupported filesystems fail clearly. Staging files are cleaned on failure; published files are retained.

Build `test_qrest_data_dataset_draft`, then run it explicitly. The optional real TDMS/MiniSEED regression requires the documented sample files, absent from some checkouts.
