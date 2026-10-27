#!/usr/bin/env python3
"""Extract the first second of the project-local equivalent SIT recordings."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
source = root / "resource/example_data"
out = root / "resource/test/format_readers"
out.mkdir(parents=True, exist_ok=True)
files = {
    "sit_1s.dat": ("HBSY0012026102222.dat", 512 + 788),
    "standard_1s.bin": ("HBSY0012026102222_dat.mseed", 512 * 3),
}
manifest = {}
for name, (original, size) in files.items():
    raw = (source / original).read_bytes()
    (out / name).write_bytes(raw[:size])
    manifest[original] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
text = next(source.glob("*_Gal_dat.txt"))
raw = text.read_bytes()
(out / "sit_1s.txt").write_bytes(b"".join(raw.splitlines(keepends=True)[:101]))
manifest[text.name] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
(out / "sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
