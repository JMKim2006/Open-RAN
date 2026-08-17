#!/usr/bin/env python3
"""Reject Windows-style paths in repository text files."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP = {".git", "build", "vendor", "evidence"}
TEXT_SUFFIXES = {".md", ".py", ".sh", ".yml", ".yaml", ".json", ".env", ".txt"}
WINDOWS_PATH = re.compile(r"(?:[A-Za-z]:\\|(?:^|[\s\"'`])(?:\.{1,2}\\)[\w.-])")
failures = []
for path in ROOT.rglob("*"):
    if not path.is_file() or any(part in SKIP for part in path.parts):
        continue
    if path.suffix not in TEXT_SUFFIXES and path.name != "CMakeLists.txt":
        continue
    text = path.read_text(encoding="utf-8")
    for number, line in enumerate(text.splitlines(), 1):
        if WINDOWS_PATH.search(line):
            failures.append(f"{path.relative_to(ROOT).as_posix()}:{number}:{line}")
if failures:
    print("\n".join(failures), file=sys.stderr)
    raise SystemExit(1)
print("POSIX repository paths: PASS")
