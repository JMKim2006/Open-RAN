#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/evidence/orqest-sctp}"
build="$root/build/orqest-sctp"
mkdir -p "$out/logs"
exec > >(tee "$out/logs/orchestrator.log") 2>&1
cleanup() {
  set +e
  [[ -n "${du_pid:-}" ]] && kill -TERM "$du_pid" 2>/dev/null || true
  [[ -n "${ric_pid:-}" ]] && kill -TERM "$ric_pid" 2>/dev/null || true
  [[ -n "${tcpdump_pid:-}" ]] && sudo kill -INT "$tcpdump_pid" 2>/dev/null || true
}
trap cleanup EXIT
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y   build-essential cmake ninja-build libsctp-dev lksctp-tools tcpdump tshark
cmake -S "$root" -B "$build" -G Ninja   -DCMAKE_BUILD_TYPE=RelWithDebInfo   -DCMAKE_EXPORT_COMPILE_COMMANDS=ON   -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g -DNDEBUG"
cmake --build "$build" --parallel 2
ctest --test-dir "$build" --output-on-failure
readelf -W -l "$build/orqest-ric" | grep GNU_STACK | tee "$out/orqest-ric-gnu-stack.txt"
readelf -W -l "$build/orqest-du" | grep GNU_STACK | tee "$out/orqest-du-gnu-stack.txt"
if grep -Eq 'GNU_STACK.*RWE' "$out/"*gnu-stack.txt; then
  echo "Executable GNU_STACK rejected" >&2
  exit 1
fi
git -C "$root" rev-parse HEAD > "$out/repository-commit.txt"
git -C "$root" status --porcelain=v1 > "$out/dirty-tree.txt"
cp "$build/CMakeCache.txt" "$out/CMakeCache.txt"
cp "$build/compile_commands.json" "$out/compile_commands.json"
sha256sum "$build/orqest-ric" "$build/orqest-du" > "$out/binary-sha256.txt"
sudo timeout 45 tcpdump -U -i lo -s 0 -w "$out/orqest-sctp.pcap" 'sctp port 39001'   >"$out/logs/tcpdump.log" 2>&1 &
tcpdump_pid=$!
sleep 2
timeout 30 "$build/orqest-ric" --port 39001 --timeline "$out/ric-timeline.jsonl"   >"$out/logs/orqest-ric.log" 2>&1 &
ric_pid=$!
sleep 1
timeout 30 "$build/orqest-du" --port 39001 --timeline "$out/du-timeline.jsonl"   >"$out/logs/orqest-du.log" 2>&1 &
du_pid=$!
wait "$du_pid"
wait "$ric_pid"
sleep 3
sudo kill -INT "$tcpdump_pid" 2>/dev/null || true
wait "$tcpdump_pid" 2>/dev/null || true
unset tcpdump_pid
python3 "$root/tools/collect_orqest_evidence.py" "$out"
python3 "$root/tools/evidence_gate.py" --gate G1 --evidence "$out/evidence.json" | tee "$out/gate-g1.json"
python3 "$root/tools/evidence_gate.py" --gate G2 --evidence "$out/evidence.json" | tee "$out/gate-g2.json"
python3 "$root/tools/collect_orqest_evidence.py" "$out"
