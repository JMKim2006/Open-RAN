# Execution log

Date: 2026-07-27 (Asia/Seoul).

## Retired generic FlexRIC xApp path

The generic KPM xApp repair cycle stopped after Actions run
`30273703324`. The pinned OCUDU build was reused. The no-trampoline patch
applied, but strict compilation stopped in the unrelated upstream SQLite wrapper
on five `-Werror=format-truncation` diagnostics. The xApp did not run, no
executable stack was enabled, no KPM subscription/indication was claimed, and
the ORQEST overlay gate was not run.

No further FlexRIC, SQLite, E42, or generic xApp repair is authorized by the
current workflow.

## ORQEST carrier commands

The standard GitHub-hosted `ubuntu-24.04` workflow executes:

| Command | Expected evidence |
|---|---|
| `scripts/capability-probe.sh evidence/capability` | Docker/sudo/resources/SCTP protocol and real SCTP socket probe |
| `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` | Portable C++17 configuration |
| `cmake --build build --parallel 2` | Engine, wire library, tests, `orqest-ric`, `orqest-du` |
| `ctest --test-dir build --output-on-failure` | Engine and deterministic carrier tests |
| `python3 tools/evidence_gate.py --gate G0 --evidence evidence/g0-public-stack.json` | Expected failure: historical manifest invalidated after artifact audit |
| `scripts/run-orqest-sctp-harness.sh evidence/orqest-sctp` | Actual multi-process SCTP carrier run |
| `readelf -W -l build/orqest-ric` and `orqest-du` | Non-executable GNU_STACK evidence |
| `tcpdump -i lo -s 0 ... 'sctp port 39001'` | Actual SCTP PCAP |
| `tshark -r ... -Y sctp` | Parsed SCTP packet summary |
| `python3 tools/evidence_gate.py --gate G1 ...` | Wire/provenance gate |
| `python3 tools/evidence_gate.py --gate G2 ...` | Closed-loop gate |

Every hosted run uploads logs and exact failure evidence even when a gate fails.
Core dumps are disabled. No debugger, memory dump, environment dump, credential
collection, unrelated process inspection, or privileged host debugging is used.
