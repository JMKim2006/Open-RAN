#!/usr/bin/env bash
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/evidence/hosted-minimal-e2}"
mkdir -p "$out/logs"
exec > >(tee "$out/logs/orchestrator.log") 2>&1
finalize() {
  local rc=$?
  set +e
  for pid in "${xapp_pid:-}" "${gnb_pid:-}" "${ric_pid:-}" "${tcpdump_pid:-}"; do
    [[ -n "$pid" ]] && kill -TERM "$pid" 2>/dev/null || true
  done
  [[ -f /tmp/ocudu-gnb.log ]] && cp /tmp/ocudu-gnb.log "$out/logs/ocudu-file.log"
  [[ -f "$root/build/upstream/flexric/kpm-xapp-gnu-stack.txt" ]] &&
    cp "$root/build/upstream/flexric/kpm-xapp-gnu-stack.txt" "$out/kpm-xapp-gnu-stack.txt"
  {
    echo "workflow_repository=${GITHUB_REPOSITORY:-local}"
    echo "workflow_sha=${GITHUB_SHA:-local}"
    echo "workflow_run_id=${GITHUB_RUN_ID:-local}"
    echo "exit_code=$rc"
  } > "$out/run-metadata.txt"
  : > "$out/commits.txt"
  for d in "$root/vendor/ocudu" "$root/vendor/flexric"; do
    [[ -d "$d/.git" ]] || continue
    printf '%s %s\n' "$(basename "$d")" "$(git -C "$d" rev-parse HEAD)" >> "$out/commits.txt"
  done
  cp "$root/integration/pins.env" "$out/pins.env"
  sha256sum     "$root/integration/patches/flexric-gcc14-kpm-v3.patch"     "$root/integration/patches/flexric-kpm-safe-transition.patch"     "$root/integration/patches/flexric-f1c08ed-kpm-xapp-no-trampolines.patch"     > "$out/patch-sha256.txt"
  exit "$rc"
}
trap finalize EXIT
set -e
"$root/scripts/capability-probe.sh" "$out/capability"
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y   build-essential ccache cmake ninja-build git python3 python3-pip libsctp-dev lksctp-tools   libzmq3-dev libfftw3-dev libmbedtls-dev libyaml-cpp-dev libpcre2-dev   libboost-all-dev libconfig++-dev libgtest-dev tcpdump tshark
tshark --version > "$out/tshark-version.txt"
"$root/scripts/fetch-minimal-pinned.sh"
"$root/scripts/build-hosted-minimal.sh" 2>&1 | tee "$out/logs/build.log"
cp "$root/build/upstream/flexric/kpm-xapp-gnu-stack.txt" "$out/kpm-xapp-gnu-stack.txt"
cp /usr/local/etc/flexric/flexric.conf "$out/flexric.conf"
grep -E '^(CMAKE_(C_FLAGS|C_FLAGS_DEBUG|CXX_FLAGS|CXX_FLAGS_DEBUG|BUILD_TYPE|EXE_LINKER_FLAGS|SHARED_LINKER_FLAGS)|E2AP_VERSION|KPM_VERSION):'   "$root/build/upstream/flexric/CMakeCache.txt" > "$out/flexric-build-flags.txt"
find /usr/local/lib/flexric -maxdepth 1 -type f -printf '%f\n'   | sort > "$out/generated-service-model-libraries.txt"
sudo timeout 180 tcpdump -i lo -s 0 -w "$out/e2ap.pcap" 'sctp port 36421'   >"$out/logs/tcpdump.log" 2>&1 &
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
python3 "$root/tools/evidence_gate.py" --gate G3 --evidence "$out/evidence.json" \
  | tee "$out/gate-g3.json"
