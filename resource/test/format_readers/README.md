# Reader regression fixtures

These are the first second (3 channels × 100 samples) of the three equivalent
recordings in `resource/example_data`. `standard_1s.bin` contains MiniSEED 2;
`.bin` avoids the repository-wide ignore rule for `.mseed` files. Regenerate with
`python3 scripts/make_reader_fixtures.py` when the full local sources are present.
`sources.json` records the sizes and SHA-256 of those full sources.

Binary source order: EIN/EIE/EIZ (NS/EW/UD), first counts 19/-8/8.
TXT source order: UD/NS/EW, first Gal values 0.008/0.019/-0.008.
UTC start: 2026-10-22T14:00:00Z; TXT local time: 22:00:00 at +08:00.
DAT native sensitivity: 100000 count/(m/s²). Standard MiniSEED has no calibration.
The TXT header is GBK; the explicit SIT Gal profile defines the input unit.
