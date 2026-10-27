#!/usr/bin/env python3
"""Generate a synthetic MiniSEED load and measure Linux CLI wall time/peak RSS.

Uses the project-owned 100-sample Steim-2 fixture payload. This is a memory and
throughput baseline, not a realistic compression or waveform benchmark. Output
is kept in a new, explicit scratch directory; no repository data is modified.
"""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("cli", type=Path)
parser.add_argument("--work-dir", type=Path, required=True)
parser.add_argument("--channels", type=int, default=63)
parser.add_argument("--rate", type=int, default=200)
parser.add_argument("--seconds", type=int, default=3600)
args = parser.parse_args()
if sys.platform != "linux" or not hasattr(os, "wait4"):
    parser.error("peak RSS measurement requires Linux os.wait4")
if not (1 <= args.channels <= 99 and 1 <= args.rate <= 32767 and args.seconds > 0
        and args.rate * args.seconds % 100 == 0):
    parser.error("require 1..99 channels, 1..32767 Hz, and a positive multiple of 100 samples")
cli = args.cli.resolve(strict=True)
work = args.work_dir.resolve()
work.mkdir(parents=True, exist_ok=False)
fixture = root / "resource/test/format_readers"
base = bytearray((fixture / "standard_1s.bin").read_bytes()[:512])
base[8:13] = b"BMARK"
base[18:20] = b"XX"
epoch = datetime(2026, 10, 22, 14, tzinfo=timezone.utc)
source = work / "synthetic.mseed"
samples = args.rate * args.seconds
channels = [f"Q{i:02d}" for i in range(args.channels)]
with source.open("wb") as output:
    for index in range(samples // 100):
        record = bytearray(base)
        stamp = epoch + timedelta(seconds=index * 100 / args.rate)
        struct.pack_into(">HH", record, 20, stamp.year, stamp.timetuple().tm_yday)
        record[24:27] = bytes((stamp.hour, stamp.minute, stamp.second))
        struct.pack_into(">H", record, 28, stamp.microsecond // 100)
        struct.pack_into(">h", record, 32, args.rate)
        for channel in channels:
            record[15:18] = channel.encode("ascii")
            output.write(record)
metadata = json.loads((fixture / "metadata.json").read_text())
metadata["BuildingInfo"]["ProjectName"] = "SyntheticReaderBenchmark"
metadata["DataInfo"].update(NPTS=samples, DT=1 / args.rate)
metadata["Units"][0] = "count"
prototype = metadata["InstrumentInfo"]["Channels"][0]
metadata["InstrumentInfo"]["ChannelNum"] = args.channels
metadata["InstrumentInfo"]["Channels"] = [dict(prototype, ChannelNo=i + 1,
    ChannelID=channel, Measurand="RawCounts", Scale=1) for i, channel in enumerate(channels)]
meta = work / "metadata.json"
meta.write_text(json.dumps(metadata))


def measure(label, arguments):
    with (work / f"{label}.log").open("wb") as log:
        begin = time.perf_counter()
        process = subprocess.Popen([str(cli), *map(str, arguments)], stdout=log, stderr=log)
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
        result = dict(seconds=round(time.perf_counter() - begin, 6),
                      peak_rss_kib=usage.ru_maxrss, exit_code=process.returncode)
        if process.returncode:
            raise RuntimeError(f"{label} failed: see {work / (label + '.log')}")
        return result


result = dict(channels=args.channels, rate_hz=args.rate, samples_per_channel=samples,
              input_bytes=source.stat().st_size, output_array_bytes=args.channels * samples * 8,
              payload_sha256=hashlib.sha256(base[64:]).hexdigest())
with source.open("rb") as input_file:
    result["input_sha256"] = hashlib.file_digest(input_file, "sha256").hexdigest()
result["validate"] = measure("validate", ["validate", "mseed", source])
result["import"] = measure("import", ["import", "mseed", source, meta, work / "output.qrest",
                                      "--counts", "--encoding", "1"])
result["qrest_bytes"] = (work / "output.qrest").stat().st_size
(work / "measurements.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
