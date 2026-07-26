# ORQEST public-stack validation

Deployment and public-stack validation repository for the ORQEST JSAC research
project. It contains one unified O-DU ORQEST engine; queue-sensitive per-TTI
decisions remain inside the O-DU.

ORQEST KPM/RC fields are **experimental extensions**. They are not standardized
O-RAN measurements or actions.

## Portable validation

```bash
python3 tools/check_posix_paths.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tools/validate_contract.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The C++17 engine maintains a compatibility-qualified sufficient-statistic prefix
and source-exclusive cutoffs. Snapshot installation validates version, digest,
cutoff, CRC32 and positive-definiteness before atomically replacing immutable
state. Observations at or after the exclusive cutoff are replayed exactly once.

For context `x`, the scheduler uses Cholesky solves (never an explicit inverse):

```text
theta  = solve(V, b)
mu     = clip(x^T theta + alpha sqrt(x^T solve(V, x)), 0, 1)
weight = Q * mu - V_control * C
```

It then solves exact capacity-constrained queue–RBG bipartite b-matching.

## GitHub-hosted no-RF experiment

The `hosted-validation` workflow runs exclusively on standard
`ubuntu-24.04`. Its dependency chain is:

```text
capability probe -> portable tests -> minimal OCUDU/FlexRIC Gate A -> overlay Gate B
```

The capability probe records Docker, passwordless sudo, CPU, RAM, disk, the SCTP
kernel protocol, and an actual SCTP socket creation. The minimal experiment
fetches the official `https://gitlab.com/ocudu/ocudu.git` at the immutable SHA in
`integration/pins.env`, builds only OCUDU gNB and FlexRIC, starts
`ru_dummy`/`test_mode`/`no_core`, captures loopback SCTP port 36421, and requires
real E2 Setup plus KPM subscription or indication evidence.

```bash
scripts/capability-probe.sh evidence/capability
scripts/run-hosted-minimal-e2.sh evidence/hosted-minimal-e2
scripts/run-hosted-overlay.sh evidence/hosted-overlay
```

Every experiment uploads logs, PCAP where produced, exact revisions, dirty-tree
state, run metadata, and SHA-256 manifests even when it fails. A missing runner
capability or upstream incompatibility is a recorded failure, never inferred
success.

Open5GS and OAI UE are pinned for a later full-stack stage but are deliberately
not fetched or built before hosted Gate A succeeds. Gate B is dependency-blocked
until Gate A passes and the bounded KPM/RC overlay is applied to the verified
OCUDU tree. A stock OCUDU run can never pass Gate B.

See `docs/ACCEPTANCE.md` for claim boundaries and `docs/EXECUTION-LOG.md` for
commands and observed results.
