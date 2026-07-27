# ORQEST public-stack validation

This repository contains one unified C++17 ORQEST implementation. It validates
the ORQEST identity as a standards-facing, multi-process wire contract over an
actual SCTP association. It does not split the system into ORQEST-T and
ORQEST-P variants.

ORQEST fields are **experimental extensions**. They are not standardized O-RAN
measurements or actions.

## Executables

- `orqest-ric`: accepts compatibility-qualified cumulative DU reports,
  requires source-complete provenance, and produces versioned snapshots.
- `orqest-du`: owns local observations and queue state, installs snapshots
  atomically, replays the exact post-cutoff suffix, acknowledges ActiveVersion,
  and performs queue-sensitive per-TTI matching locally.

The RIC never receives queue state and never selects per-TTI RBG assignments.

## Canonical binary contract

All integers and IEEE-754 binary64 bit patterns use network byte order. Strings
are UTF-8 with a network-order uint16 length. Each message starts with a
four-byte magic and uint16 contract version and ends with CRC32 over every
preceding byte.

DU report (`ORQD`):

`source ID, compatibility digest, exclusive cutoff, cumulative count,
21 upper-triangular d=6 design entries, 6 response entries,
active snapshot version, CRC32`.

RIC snapshot (`ORQS`):

`snapshot version, compatibility digest, sorted source-specific exclusive
cutoffs, 21 aggregate design entries, 6 aggregate response entries, CRC32`.

## Portable validation

```bash
python3 tools/check_posix_paths.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tools/validate_contract.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The deterministic C++ tests cover loss, duplication, reordering, delayed and
stale snapshots, regressive cutoffs, incompatible digests, invalid CRC,
malformed frames, concurrent local observations, suffix replay, history
completeness, no double counting, and matching capacities.

## GitHub-hosted SCTP run

The actual carrier experiment runs only on a standard GitHub-hosted
`ubuntu-24.04` runner:

```bash
scripts/capability-probe.sh evidence/capability
scripts/run-orqest-sctp-harness.sh evidence/orqest-sctp
python3 tools/evidence_gate.py --gate G1 --evidence evidence/orqest-sctp/evidence.json
python3 tools/evidence_gate.py --gate G2 --evidence evidence/orqest-sctp/evidence.json
```

It uploads the SCTP PCAP, RIC/DU logs, parsed timeline, commit SHA, dirty-tree
status, compiler configuration, binary and artifact SHA-256 checksums, GNU_STACK
inspection, and final evidence JSON. Executable stacks are rejected.

## Independent gates

- **G0 — public-stack grounding:** preserved pinned OCUDU/FlexRIC evidence,
  real SCTP, E2 Setup, KPM/RC RAN-function registration, and nonempty E2AP PCAP.
  G0 does not claim KPM subscription or indication.
- **G1 — ORQEST wire integrity:** canonical messages, CRC/malformed rejection,
  compatibility filtering, source completeness, and cutoff provenance over
  actual SCTP.
- **G2 — ORQEST closed loop:** G1 plus report → aggregate → snapshot → atomic
  install → ActiveVersion acknowledgement → exact suffix replay → local
  queue-sensitive matching.
- **G3 — optional stock interoperability:** real stock KPM subscription and
  indication. G1 and G2 do not depend on G3.

The retired generic FlexRIC KPM xApp repair path is not part of G1 or G2.
FlexRIC, SQLite, and the E42 event framework are not modified by the carrier
workflow. See `docs/ACCEPTANCE.md` for precise claim boundaries.
