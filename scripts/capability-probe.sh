#!/usr/bin/env bash
set -uo pipefail
out="${1:-evidence/capability}"
mkdir -p "$out/logs"
exec > >(tee "$out/logs/probe.log") 2>&1
status=0
probe() {
  local name="$1"
  shift
  printf '\n[%s]\n' "$name"
  "$@" || status=1
}
probe os bash -c 'cat /etc/os-release; uname -a'
probe cpu bash -c 'nproc; lscpu'
probe memory free -h
probe disk df -h
probe sudo sudo -n true
probe docker bash -c 'docker --version && docker info'
probe compose docker compose version
probe sctp-module bash -c 'sudo modprobe sctp 2>/dev/null || true; grep -w sctp /proc/net/protocols'
probe sctp-socket python3 - <<'PY'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP)
s.bind(("127.0.0.1", 0))
print("SCTP socket:", s.getsockname())
s.close()
PY
python3 - "$out/capability.json" "$status" <<'PY'
import json, pathlib, platform, shutil, sys
p, status = pathlib.Path(sys.argv[1]), int(sys.argv[2])
p.write_text(json.dumps({
    "runner_os": platform.platform(),
    "docker_cli": shutil.which("docker") is not None,
    "sudo": shutil.which("sudo") is not None,
    "probe_pass": status == 0,
}, indent=2) + "\n")
PY
(cd "$out" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
exit "$status"
