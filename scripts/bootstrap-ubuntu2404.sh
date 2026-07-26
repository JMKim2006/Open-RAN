#!/usr/bin/env bash
set -euo pipefail
[[ "$(. /etc/os-release; echo "$VERSION_ID")" == "24.04" ]] || { echo "Ubuntu 24.04 required"; exit 2; }
sudo apt-get update
sudo apt-get install -y docker.io docker-compose-v2 lksctp-tools libsctp-dev build-essential cmake ninja-build \
  libzmq3-dev libfftw3-dev libmbedtls-dev libyaml-cpp-dev tshark tcpdump iperf3 git python3
sudo docker info >/dev/null || { echo "Docker unavailable"; exit 3; }
modprobe -n sctp >/dev/null && grep -qw sctp /proc/net/protocols || { echo "SCTP unavailable"; exit 4; }
