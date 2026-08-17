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
enum class AdmissionState { shadow, admitted, quarantined };
struct AdmissionDecision {
  AdmissionState state{AdmissionState::shadow};
  uint64_t samples{};
  double score{};
};

// Statistical safeguard applied only after the hard semantic compatibility
// key matches.  A fixed source model is checked on a predeclared target shadow
// window using the covariance-normalized residual-moment statistic in the
// manuscript.  The first terminal decision is frozen to avoid optional
// stopping and repeated-threshold testing.
class ResidualMomentAdmission {
 public:
  ResidualMomentAdmission(const Sufficient& source_model,
                          uint64_t shadow_samples,
                          double score_threshold,
                          double covariance_ridge=1e-8);
  AdmissionDecision observe(const Observation& target_sample);
  AdmissionDecision decision() const;
 private:
  Vec source_theta_{};
  uint64_t shadow_samples_{};
  double score_threshold_{};
  double covariance_ridge_{};
  uint64_t samples_{};
  Vec moment_sum_{};
  Mat moment_outer_sum_{};
  AdmissionDecision decision_{};
};
struct CoverageDecision {
  bool monitoring_active{};
  bool covered{true};
  uint64_t epoch{};
  double minimum_eigenvalue{};
  double required_floor{};
};
class InstalledCoverageGuard {
 public:
  InstalledCoverageGuard(double ridge_floor, double growth_rate,
                         uint64_t monitoring_start);
  CoverageDecision check(uint64_t epoch, const Sufficient& installed) const;
 private:
  double ridge_floor_{};
  double growth_rate_{};
  uint64_t monitoring_start_{};
};
struct Snapshot {
  uint64_t version{};
  std::string digest;
  std::unordered_map<std::string,uint64_t> cutoff; // exclusive
  Sufficient aggregate;
  uint32_t crc{};
};
struct Queue {
  std::string id;
  unsigned demand{};
  Vec feature{};
  double backlog{}; // Q; defaults to demand when zero
  double cost{};    // C
};
struct Rbg { std::string id; unsigned capacity{}; Vec channel{}; };
struct Assignment { std::string queue; std::string rbg; double score{}; };

struct KpmRanFunction {
  int function_id{};
  int revision_id{};
  bool is_kpm{};
};
struct KpmNodeRegistration {
  const KpmRanFunction* functions{};
  std::size_t length{};
};
struct KpmDefinitionView {
  const void* event_styles{};
  std::size_t event_style_count{};
  const void* report_styles{};
  std::size_t report_style_count{};
  std::size_t selected_report_style{};
  std::size_t callback_count{};
  bool callback_registered{};
};
std::optional<std::size_t> validate_kpm_registration(
    const KpmNodeRegistration*, int function_id, int supported_revision,
    std::string* reason=nullptr);
bool validate_kpm_subscription_construction(
    const KpmDefinitionView*, std::string* reason=nullptr);

uint32_t crc32(const Snapshot&);
bool positive_definite(const Mat&);
Sufficient add(const Sufficient&, const Observation&);

class Engine {
 public:
  Engine(std::string source, std::string digest, double exploration,
         double control_penalty=0.0);
  void observe(const Observation&);
  Sufficient current() const;
  uint64_t active_version() const;
  bool install(const Snapshot&, std::string* reason=nullptr);
  std::vector<Assignment> schedule(const std::vector<Queue>&,
                                   const std::vector<Rbg>&) const;
  CoverageDecision coverage(const InstalledCoverageGuard&,uint64_t epoch) const;
  std::vector<Assignment> schedule_guarded(
      const std::vector<Queue>&,const std::vector<Rbg>&,
      const InstalledCoverageGuard&,uint64_t epoch,
      double conservative_exploration) const;
 private:
  std::string source_, digest_;
  double exploration_;
  double control_penalty_;
  mutable std::mutex mu_;
  std::vector<Observation> local_;
  std::shared_ptr<const Snapshot> active_;
};
}
