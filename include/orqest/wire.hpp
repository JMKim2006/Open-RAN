#pragma once
#include "orqest/orqest.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace orqest::wire {

// Canonical experimental ORQEST wire contract. All integers and IEEE-754
// binary64 bit patterns are encoded in network byte order. A CRC32 over every
// preceding byte is appended as the final uint32.
struct DuReport {
  std::string source_id;
  std::string compatibility_digest;
  uint64_t exclusive_cutoff{};
  Sufficient cumulative;
  uint64_t active_snapshot_version{};
};

std::vector<uint8_t> encode_report(const DuReport&);
bool decode_report(const std::vector<uint8_t>&, DuReport&, std::string* reason=nullptr);
std::vector<uint8_t> encode_snapshot(const Snapshot&);
bool decode_snapshot(const std::vector<uint8_t>&, Snapshot&, std::string* reason=nullptr);

class RicAggregator {
 public:
  RicAggregator(std::string digest, std::set<std::string> expected_sources);
  bool accept(const DuReport&, std::string* reason=nullptr);
  bool source_complete() const;
  Snapshot snapshot(uint64_t version) const;
  uint64_t acknowledged_version(const std::string& source) const;
 private:
  std::string digest_;
  std::set<std::string> expected_;
  std::map<std::string,DuReport> reports_;
};

} // namespace orqest::wire
