#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace orqest {
constexpr std::size_t D = 6;
using Vec = std::array<double,D>;
using Mat = std::array<double,D*D>;
struct Sufficient { Mat a{}; Vec b{}; uint64_t count{}; };
struct Observation { uint64_t sequence{}; Vec x{}; double reward{}; };
struct Snapshot {
  uint64_t version{};
  std::string digest;
  std::unordered_map<std::string,uint64_t> cutoff; // exclusive
  Sufficient aggregate;
  uint32_t crc{};
};
struct Queue { std::string id; unsigned demand{}; Vec feature{}; };
struct Rbg { std::string id; unsigned capacity{}; Vec channel{}; };
struct Assignment { std::string queue; std::string rbg; double score{}; };

uint32_t crc32(const Snapshot&);
bool positive_definite(const Mat&);
Sufficient add(const Sufficient&, const Observation&);

class Engine {
 public:
  Engine(std::string source, std::string digest, double exploration);
  void observe(const Observation&);
  Sufficient current() const;
  uint64_t active_version() const;
  bool install(const Snapshot&, std::string* reason=nullptr);
  std::vector<Assignment> schedule(const std::vector<Queue>&,
                                   const std::vector<Rbg>&) const;
 private:
  std::string source_, digest_;
  double exploration_;
  mutable std::mutex mu_;
  std::vector<Observation> local_;
  std::shared_ptr<const Snapshot> active_;
};
}
