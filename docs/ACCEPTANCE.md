# Acceptance criteria and claim boundaries

ORQEST is one unified implementation. There are no ORQEST-T/ORQEST-P variants.

## Evidence gates

### G0 — public-stack SCTP/E2 grounding

Requires immutable OCUDU and FlexRIC revisions, a real SCTP association, E2
Setup, KPM/RC RAN-function registration, and a nonempty E2AP PCAP. Existing
evidence is preserved in `evidence/g0-public-stack.json` and its referenced
Actions artifact.

G0 explicitly does **not** claim KPM subscription or KPM indication.

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

### G3 — optional stock KPM interoperability

Requires a real E2 Setup, KPM subscription exchange, KPM indication, and
nonempty E2AP PCAP. G3 is independent and optional. G1 and G2 never depend on
G3.

## Claim levels

- **Process-level:** executable or test completion only.
- **Wire-contract-level:** canonical experimental ORQEST serialization and
  validation.
- **Public-stack grounding:** G0 only.
- **ORQEST closed-loop carrier validation:** G1/G2, over actual SCTP.
- **Stock KPM interoperability:** G3 only.
- **OTA:** requires independently identified RF hardware, spectrum, UE and RF
  captures. No hosted no-RF run makes an OTA claim.

No workflow fabricates SCTP, E2AP, KPM, OCUDU, PCAP, RF, or interoperability
evidence. Missing capabilities or observations fail the corresponding gate.
Executable stacks are never enabled.
