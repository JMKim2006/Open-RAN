# Acceptance criteria and claim boundaries

ORQEST is one unified implementation. There are no ORQEST-T/ORQEST-P variants.

## Evidence gates

### G0 — invalidated historical public-stack attempt

The former positive manifest is retained in `evidence/g0-public-stack.json` as
a negative audit record. Its referenced artifact shows the ASan-linked RIC
terminating before E2 Setup and repeated gNB connection refusals. A nonempty
PCAP is insufficient, so G0 must not be cited as positive evidence. The new G3
workflow must establish public-stack interoperability from decoded protocol
fields.

### G1 — ORQEST wire-contract and provenance integrity

Requires an actual SCTP association and nonempty SCTP PCAP between
`orqest-du` and `orqest-ric`, canonical network-byte-order serialization,
valid CRC handling, rejection of malformed frames, compatibility filtering,
source-complete aggregation, and source-specific exclusive cutoff provenance.

This is experimental ORQEST wire-contract evidence, not an O-RAN
standardization claim.

### G2 — ORQEST closed loop

Requires G1 and the complete sequence:

`DU cumulative report → RIC compatibility filter → source-complete aggregate →
versioned snapshot → atomic DU install → ActiveVersion acknowledgement → exact
post-cutoff suffix replay → queue-sensitive local matching`.

Acceptance additionally requires disjoint and complete prefix/suffix histories,
no double counting, no permanent holes, rejection of stale, regressive,
incompatible, CRC-invalid and malformed snapshots, and capacity-correct
b-matching. RIC evidence must show that it received no queue state and selected
no per-TTI assignment.

### G3 — stock KPM interoperability

Requires a real E2 Setup request/response, KPM subscription request/response,
KPM indication, and nonempty E2AP PCAP. Each observation is derived from the
E2AP procedure code and PDU choice rather than application-log strings. G3 is
independent of G1/G2.

## Claim levels

- **Process-level:** executable or test completion only.
- **Wire-contract-level:** canonical experimental ORQEST serialization and
  validation.
- **Public-stack grounding:** only a passing G3 artifact; historical G0 is
  invalidated.
- **ORQEST closed-loop carrier validation:** G1/G2, over actual SCTP.
- **Stock KPM interoperability:** G3 only.
- **OTA:** requires independently identified RF hardware, spectrum, UE and RF
  captures. No hosted no-RF run makes an OTA claim.

No workflow fabricates SCTP, E2AP, KPM, OCUDU, PCAP, RF, or interoperability
evidence. Missing capabilities or observations fail the corresponding gate.
Executable stacks are never enabled.
