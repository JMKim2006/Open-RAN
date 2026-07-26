# Local execution log

Date: 2026-07-27 (Asia/Seoul). Host: Windows Codex workspace.

| Command | Result |
|---|---|
| `cmake -S . -B build` | **Not run:** `cmake` is not installed on this host. |
| `python tools/validate_contract.py` | **Not run:** the Windows Store Python shim was not executable. |
| Bundled Python `tools/validate_contract.py` | **PASS:** `contract: PASS`. |
| Bundled Python `-m unittest discover -s tests -p 'test_*.py' -v` | **PASS:** 2 tests passed. |
| Bundled Python parse of every `*.json` | **PASS:** all JSON decoded. |
| Bundled Python/PyYAML parse of workflow YAML | **Not run:** `yaml` module is unavailable. |
| `winget install Kitware.CMake Ninja-build.Ninja LLVM.LLVM` | **Blocked:** the Codex sandbox cannot execute the WindowsApps shim. |
| Bundled Python `pip install --target .tools/python cmake ninja pyyaml` | **Blocked:** outbound package network access returned WinError 10013. |
| `bash -n scripts/*.sh` | **Not run:** Bash is unavailable. |
| `git init -b main` | **PASS:** initialized the local validation repository. |
| `git switch -c agent/orqest-public-stack-validation` | **PASS:** created the requested branch. |
| `gh --version` | **PASS:** 2.96.0. |
| `gh auth status` | **Unavailable in sandbox:** direct GitHub network access is blocked; publishing uses the installed GitHub Connector instead. |

No SCTP, E2AP, OCUDU, UE, OTA, PCAP, or interoperability evidence was generated
locally. Standard GitHub-hosted `ubuntu-24.04` jobs now perform both portable
validation and the staged no-RF experiment. The workflow must upload exact probe
and failure artifacts when the hosted environment or upstream integration is
insufficient.
