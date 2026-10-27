#!/usr/bin/env python3
"""Exercise the installed or build-tree CLI against the small reader fixtures."""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("cli", type=Path)
args = parser.parse_args()
cli = args.cli.resolve()
fixtures = root / "resource/test/format_readers"
checks = 0


def run(*arguments, ok=True):
    global checks
    result = subprocess.run([str(cli), *map(str, arguments)], capture_output=True, text=True)
    checks += 1
    assert (result.returncode == 0) == ok, result.stdout + result.stderr
    return result.stdout + result.stderr


with tempfile.TemporaryDirectory(prefix="qrest_reader_cli_") as directory:
    temp = Path(directory)
    # The explicit mseed selector accepts file content regardless of the extension.
    seed = temp / "standard.mseed"
    seed.write_bytes((fixtures / "standard_1s.bin").read_bytes())
    dat, txt = fixtures / "sit_1s.dat", fixtures / "sit_1s.txt"
    for fmt, path in [("mseed", seed), ("sit-dat", dat), ("sit-txt", txt)]:
        report = run("validate", fmt, path)
        summary = json.loads(report[report.index("{"):])
        assert summary["samples"] == 100 and summary["channels"] == 3
        assert summary["non_finite_samples"] == 0
        assert summary["representation"] == ("RawCounts" if fmt == "mseed" else "Physical")
    # Use fixture-owned minimal metadata; no site geometry is inferred from the waves.
    metadata = json.loads((fixtures / "metadata.json").read_text())
    meta = temp / "metadata.json"
    meta.write_text(json.dumps(metadata))
    for fmt, path, options in [("sit-dat", dat, []), ("sit-txt", txt, ["--channel-map", "3,1,2"]),
                               ("mseed", seed, ["--sensitivity", "100000", "--quantity", "Acceleration", "--unit", "cm/s2"])]:
        output = temp / f"{fmt}.qrest"
        report = run("import", fmt, path, meta, output, *options, "--encoding", "1")
        assert "mapping:" in report
        run("validate", "qrest", output)
        run("extract", output, temp / f"{fmt}.json", temp / f"{fmt}.txt", "--precision", "17")
    columns = [[float(v) for v in (temp / f"{fmt}.txt").read_text().split()] for fmt in ("sit-dat", "sit-txt", "mseed")]
    assert all(abs(a-b) <= 1e-12 for data in columns[1:] for a, b in zip(columns[0], data))
    bad = temp / "bad.qrest"
    run("import", "mseed", seed, meta, bad, ok=False)
    run("import", "sit-txt", txt, meta, bad, ok=False)
    run("import", "sit-dat", dat, meta, bad, "--channel-map", "1,1,3", ok=False)
    run("validate", "mseed", seed, "--unit", "m/s2", ok=False)
    run("validate", "mseed", seed, "--counts", "--unit", "m/s2", ok=False)
    run("validate", "mseed", seed, "--sensitivity", "0", ok=False)
    run("validate", "sit-txt", txt, "--timezone", "+15:00", ok=False)
    assert not bad.exists()
    # Raw data can be written only with explicitly raw metadata.
    metadata["Units"][0] = "count"
    for channel in metadata["InstrumentInfo"]["Channels"]:
        channel["Measurand"] = "RawCounts"
    meta.write_text(json.dumps(metadata))
    run("import", "mseed", seed, meta, temp / "counts.qrest", "--counts", "--encoding", "1")
    # Generic numeric matrix packing keeps its original command semantics.
    run("pack", meta, temp / "sit-dat.txt", temp / "matrix.qrest")
print(f"CLI reader checks passed: {checks}")
