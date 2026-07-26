#!/usr/bin/env bash
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/evidence/hosted-minimal-e2}"
mkdir -p "$out/logs"
exec > >(tee "$out/logs/orchestrator.log") 2>&1
finalize() {
  local rc=$?
  set +e
  sudo pkill -TERM -f 'nearRT-RIC|ocudu_gnb|/gnb|xapp_kpm' 2>/dev/null
  [[ -f /tmp/ocudu-gnb.log ]] && cp /tmp/ocudu-gnb.log "$out/logs/ocudu-file.log"
  {
    echo "workflow_repository=${GITHUB_REPOSITORY:-local}"
    echo "workflow_sha=${GITHUB_SHA:-local}"
    echo "workflow_run_id=${GITHUB_RUN_ID:-local}"
    echo "exit_code=$rc"
  } > "$out/run-metadata.txt"
  for d in "$root/vendor/ocudu" "$root/vendor/flexric"; do
    [[ -d "$d/.git" ]] || continue
    printf '%s %s\n' "$(basename "$d")" "$(git -C "$d" rev-parse HEAD)" >> "$out/commits.txt"
    printf '%s ' "$(basename "$d")" >> "$out/dirty-trees.txt"
    git -C "$d" status --porcelain=v1 >> "$out/dirty-trees.txt"
  done
  (cd "$out" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
  exit "$rc"
}
trap finalize EXIT
set -e
"$root/scripts/capability-probe.sh" "$out/capability"
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential cmake ninja-build git python3 python3-pip libsctp-dev lksctp-tools \
  libzmq3-dev libfftw3-dev libmbedtls-dev libyaml-cpp-dev libpcre2-dev \
  libboost-all-dev libconfig++-dev tcpdump tshark
"$root/scripts/fetch-minimal-pinned.sh"
"$root/scripts/build-hosted-minimal.sh" 2>&1 | tee "$out/logs/build.log"
sudo timeout 180 tcpdump -i lo -s 0 -w "$out/e2ap.pcap" 'sctp port 36421' \
  >"$out/logs/tcpdump.log" 2>&1 &
tcpdump_pid=$!
"$root/scripts/launch-flexric.sh" "$out" &
ric_pid=$!
sleep 5
"$root/scripts/launch-ocudu-testmode.sh" "$out" &
gnb_pid=$!
sleep 15
"$root/scripts/launch-kpm-xapp.sh" "$out" &
xapp_pid=$!
set +e
wait "$xapp_pid"; xapp_rc=$?
kill "$gnb_pid" "$ric_pid" "$tcpdump_pid" 2>/dev/null
wait "$gnb_pid" "$ric_pid" "$tcpdump_pid" 2>/dev/null
set -e
[[ "$xapp_rc" -eq 0 || "$xapp_rc" -eq 124 ]]
python3 "$root/tools/collect_e2_evidence.py" "$out"
python3 "$root/tools/evidence_gate.py" --gate A --evidence "$out/evidence.json" \
  | tee "$out/gate-a.json"
