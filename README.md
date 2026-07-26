# ORQEST public-stack validation

Deployment and public-stack validation repository for the ORQEST JSAC research project. It contains one unified O-DU ORQEST engine; queue-sensitive per-TTI decisions never leave the O-DU.

ORQEST KPM/RC fields are **experimental extensions**. They are not standardized O-RAN measurements or actions.

## Portable validation

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
python3 tools/validate_contract.py
python3 tools/evidence_gate.py --gate A --evidence evidence.json
python3 tools/evidence_gate.py --gate B --evidence evidence.json
```

The C++17 engine maintains a compatibility-qualified global sufficient-statistic prefix and source-exclusive cutoffs. Snapshot install is serialized and atomically replaces immutable active state only after version, digest, cutoff, CRC32 and positive-definiteness validation. Local observations at or after the exclusive cutoff are replayed exactly once. Scheduling uses current local queue state and capacity-bounded queue–RBG matching with a fixed calibrated exploration coefficient.

## Ubuntu 24.04 public stack

All upstreams are immutable full-SHA pins in `integration/pins.env`. The OCUDU verifier checks both the pinned commit and Git blob identities at the five bounded extension points.

```bash
scripts/bootstrap-ubuntu2404.sh
scripts/fetch-pinned.sh
scripts/build-pinned.sh
python3 tools/verify_ocudu_tree.py vendor/ocudu
scripts/run-experiment.sh evidence/run
```

The intended no-RF E2 path is Open5GS → FlexRIC → OCUDU gNB → software UE → KPM xApp. Because interface names, credentials and subscriber data are site-specific, `integration/run-stock-e2-local.sh` is deliberately untracked and must perform that deployment and write real captures/logs into the supplied evidence directory. The runner fails if that reviewed local adapter is absent rather than pretending a deployment exists. Captures must include E2AP PCAP, component logs, exact commits, dirty status and SHA-256 manifests.

> **Security:** attach a self-hosted Actions runner only to a private, trusted repository. Pull-request code can execute with the runner's host and network privileges.

See `docs/ACCEPTANCE.md` for Gate A/Gate B requirements and exact claim boundaries.
