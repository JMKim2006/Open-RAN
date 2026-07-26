# Acceptance criteria and claim boundaries

ORQEST is one unified implementation. There are no ORQEST-T/ORQEST-P variants.

## Evidence levels

- **Process-level:** a process started or a unit/contract test passed. This says nothing about the wire.
- **Wire-contract-level:** experimental fields serialize/validate according to this repository's contract. They are not standardized O-RAN measurements/actions.
- **Stock-E2 interoperability (Gate A):** real E2 Setup, KPM subscription, KPM indication, and a nonempty E2AP PCAP. Decode with tshark only when its installed dissector supports E2AP.
- **ORQEST-overlay interoperability (Gate B):** Gate A plus observed Cutoff/Count, exercised InstallSnapshot, confirmed ActiveVersion, no permanent history holes, no accounting errors, and rejection of malformed messages. Stock OCUDU can never pass.
- **OTA:** requires independently identified RF hardware, spectrum conditions, UE and captures. A no-RF run makes no OTA claim.

No script fabricates SCTP, E2AP, OCUDU, UE, RF, PCAP, log, or interoperability evidence. Unavailable prerequisites fail the run.
