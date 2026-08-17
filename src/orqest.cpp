#include "orqest/orqest.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace orqest {
std::optional<std::size_t> validate_kpm_registration(
    const KpmNodeRegistration* node, int function_id, int supported_revision,
    std::string* reason) {
  auto fail=[&](const char* value)->std::optional<std::size_t>{
    if(reason)*reason=value;
    return std::nullopt;
  };
  if(node==nullptr)return fail("null node registration");
  if(node->functions==nullptr)return fail("null RAN-function array");
  if(node->length==0)return fail("empty RAN-function array");
  for(std::size_t i=0;i<node->length;i++) {
    const auto& function=node->functions[i];
    if(function.function_id!=function_id)continue;
    if(!function.is_kpm)return fail("function is not KPM");
    if(function.revision_id!=supported_revision)return fail("unsupported KPM revision");
    return i;
  }
  return fail("KPM function not found");
}
bool validate_kpm_subscription_construction(
    const KpmDefinitionView* definition, std::string* reason) {
  auto fail=[&](const char* value){if(reason)*reason=value;return false;};
  if(definition==nullptr)return fail("null KPM definition");
  if(definition->event_styles==nullptr||definition->event_style_count==0)
    return fail("missing event-trigger style");
  if(definition->report_styles==nullptr||definition->report_style_count==0)
    return fail("missing report style");
  if(definition->selected_report_style>=definition->report_style_count)
    return fail("report-style index out of range");
  if(definition->selected_report_style>=definition->callback_count)
    return fail("callback index out of range");
  if(!definition->callback_registered)return fail("null action-definition callback");
  return true;
}
static void crc_bytes(uint32_t& c,const void* p,size_t n) {
  auto b=static_cast<const unsigned char*>(p);
  for(size_t i=0;i<n;i++){c^=b[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(int)(c&1));}
}
uint32_t crc32(const Snapshot& s) {
  uint32_t c=~0u; crc_bytes(c,&s.version,sizeof s.version);
  crc_bytes(c,s.digest.data(),s.digest.size());
  std::vector<std::pair<std::string,uint64_t>> cuts(s.cutoff.begin(),s.cutoff.end());
  std::sort(cuts.begin(),cuts.end());
  for(auto& [k,v]:cuts){crc_bytes(c,k.data(),k.size());crc_bytes(c,&v,sizeof v);}
  crc_bytes(c,s.aggregate.a.data(),sizeof(double)*s.aggregate.a.size());
  crc_bytes(c,s.aggregate.b.data(),sizeof(double)*s.aggregate.b.size());
  crc_bytes(c,&s.aggregate.count,sizeof s.aggregate.count); return ~c;
}
bool positive_definite(const Mat& a) {
  for(size_t i=0;i<D;i++)for(size_t j=i+1;j<D;j++)
    if(!std::isfinite(a[i*D+j])||std::abs(a[i*D+j]-a[j*D+i])>1e-10)return false;
  double l[D][D]{};
  for(size_t i=0;i<D;i++) for(size_t j=0;j<=i;j++) {
    double v=a[i*D+j]; for(size_t k=0;k<j;k++)v-=l[i][k]*l[j][k];
    if(i==j){if(!(v>1e-12)||!std::isfinite(v))return false;l[i][j]=std::sqrt(v);}
    else l[i][j]=v/l[j][j];
  } return true;
}
static bool cholesky(const Mat& a, double l[D][D]) {
  for(size_t i=0;i<D;i++) for(size_t j=0;j<=i;j++) {
    double v=a[i*D+j];
    for(size_t k=0;k<j;k++)v-=l[i][k]*l[j][k];
    if(i==j) {
      if(!(v>1e-12)||!std::isfinite(v))return false;
      l[i][j]=std::sqrt(v);
    } else {
      l[i][j]=v/l[j][j];
    }
  }
  return true;
}
static Vec cholesky_solve(const double l[D][D], const Vec& rhs) {
  Vec y{},x{};
  for(size_t i=0;i<D;i++) {
    double v=rhs[i];for(size_t k=0;k<i;k++)v-=l[i][k]*y[k];
    y[i]=v/l[i][i];
  }
  for(size_t ii=D;ii-->0;) {
    double v=y[ii];for(size_t k=ii+1;k<D;k++)v-=l[k][ii]*x[k];
    x[ii]=v/l[ii][ii];
  }
  return x;
}
static double minimum_eigenvalue(const Mat&a) {
  double m[D][D]{};
  for(size_t i=0;i<D;i++)for(size_t j=0;j<D;j++) {
    if(!std::isfinite(a[i*D+j])||std::abs(a[i*D+j]-a[j*D+i])>1e-10)
      throw std::invalid_argument("non-symmetric coverage matrix");
    m[i][j]=a[i*D+j];
  }
  // Symmetric Jacobi iterations are deterministic for fixed d=6 and avoid a
  // heavy linear-algebra dependency in the O-DU carrier.
  for(size_t iteration=0;iteration<100*D*D;iteration++) {
    size_t p=0,q=1;double largest=0;
    for(size_t i=0;i<D;i++)for(size_t j=i+1;j<D;j++)
      if(std::abs(m[i][j])>largest){largest=std::abs(m[i][j]);p=i;q=j;}
    if(largest<1e-12)break;
    const double angle=0.5*std::atan2(2*m[p][q],m[q][q]-m[p][p]);
    const double c=std::cos(angle),s=std::sin(angle);
    const double app=m[p][p],aqq=m[q][q],apq=m[p][q];
    m[p][p]=c*c*app-2*s*c*apq+s*s*aqq;
    m[q][q]=s*s*app+2*s*c*apq+c*c*aqq;
    m[p][q]=m[q][p]=0;
    for(size_t k=0;k<D;k++)if(k!=p&&k!=q) {
      const double mkp=m[k][p],mkq=m[k][q];
      m[k][p]=m[p][k]=c*mkp-s*mkq;
      m[k][q]=m[q][k]=s*mkp+c*mkq;
    }
  }
  double value=m[0][0];
  for(size_t i=1;i<D;i++)value=std::min(value,m[i][i]);
  return value;
}
Sufficient add(const Sufficient& s,const Observation& o) {
  Sufficient r=s; for(size_t i=0;i<D;i++){r.b[i]+=o.x[i]*o.reward;
    for(size_t j=0;j<D;j++)r.a[i*D+j]+=o.x[i]*o.x[j];}
  ++r.count; return r;
}
ResidualMomentAdmission::ResidualMomentAdmission(
    const Sufficient& source_model,uint64_t shadow_samples,
    double score_threshold,double covariance_ridge)
 :shadow_samples_(shadow_samples),score_threshold_(score_threshold),
  covariance_ridge_(covariance_ridge) {
  if(shadow_samples_<2||!std::isfinite(score_threshold_)||score_threshold_<0||
     !std::isfinite(covariance_ridge_)||covariance_ridge_<=0)
    throw std::invalid_argument("admission calibration");
  double l[D][D]{};
  if(!cholesky(source_model.a,l))
    throw std::invalid_argument("non-positive-definite source model");
  source_theta_=cholesky_solve(l,source_model.b);
}
AdmissionDecision ResidualMomentAdmission::observe(const Observation&o) {
  if(decision_.state!=AdmissionState::shadow)return decision_;
  if(!std::isfinite(o.reward)||
     !std::all_of(o.x.begin(),o.x.end(),[](double x){return std::isfinite(x);}))
    throw std::invalid_argument("non-finite admission sample");
  const double prediction=std::inner_product(
      o.x.begin(),o.x.end(),source_theta_.begin(),0.0);
  Vec moment{};
  for(size_t i=0;i<D;i++)moment[i]=o.x[i]*(o.reward-prediction);
  for(size_t i=0;i<D;i++) {
    moment_sum_[i]+=moment[i];
    for(size_t j=0;j<D;j++)
      moment_outer_sum_[i*D+j]+=moment[i]*moment[j];
  }
  ++samples_;
  decision_.samples=samples_;
  if(samples_<shadow_samples_)return decision_;

  Vec mean{};
  for(size_t i=0;i<D;i++)mean[i]=moment_sum_[i]/double(samples_);
  Mat covariance{};
  for(size_t i=0;i<D;i++)for(size_t j=0;j<D;j++)
    covariance[i*D+j]=moment_outer_sum_[i*D+j]/double(samples_)-mean[i]*mean[j];
  for(size_t i=0;i<D;i++)covariance[i*D+i]+=covariance_ridge_;
  double l[D][D]{};
  if(!cholesky(covariance,l))
    throw std::runtime_error("non-positive-definite admission covariance");
  const Vec normalized=cholesky_solve(l,mean);
  decision_.score=double(samples_)*std::inner_product(
      mean.begin(),mean.end(),normalized.begin(),0.0);
  decision_.state=decision_.score<=score_threshold_
      ?AdmissionState::admitted:AdmissionState::quarantined;
  return decision_;
}
AdmissionDecision ResidualMomentAdmission::decision()const{return decision_;}
InstalledCoverageGuard::InstalledCoverageGuard(
    double ridge_floor,double growth_rate,uint64_t monitoring_start)
 :ridge_floor_(ridge_floor),growth_rate_(growth_rate),
  monitoring_start_(monitoring_start) {
  if(!std::isfinite(ridge_floor_)||ridge_floor_<=0||
     !std::isfinite(growth_rate_)||growth_rate_<=0)
    throw std::invalid_argument("coverage calibration");
}
CoverageDecision InstalledCoverageGuard::check(
    uint64_t epoch,const Sufficient&installed)const {
  CoverageDecision out;out.epoch=epoch;
  out.minimum_eigenvalue=minimum_eigenvalue(installed.a);
  out.monitoring_active=epoch>=monitoring_start_;
  out.required_floor=out.monitoring_active
      ?ridge_floor_+growth_rate_*double(epoch-monitoring_start_+1)
      :ridge_floor_;
  // A small scale-aware tolerance prevents a roundoff-only transition.
  const double tolerance=1e-10*std::max(1.0,std::abs(out.required_floor));
  out.covered=!out.monitoring_active||
      out.minimum_eigenvalue+tolerance>=out.required_floor;
  return out;
}
Engine::Engine(std::string source,std::string digest,double exploration,double control_penalty)
 :source_(std::move(source)),digest_(std::move(digest)),exploration_(exploration),
  control_penalty_(control_penalty) {
  if(exploration_<0||control_penalty_<0)throw std::invalid_argument("calibration");
  Snapshot s; s.digest=digest_; for(size_t i=0;i<D;i++)s.aggregate.a[i*D+i]=1;
  s.crc=crc32(s); active_=std::make_shared<Snapshot>(s);
}
void Engine::observe(const Observation& o){std::lock_guard<std::mutex> g(mu_);
  if(!local_.empty()&&o.sequence<=local_.back().sequence)throw std::invalid_argument("sequence");
  local_.push_back(o);
}
Sufficient Engine::current() const {std::lock_guard<std::mutex> g(mu_);auto r=active_->aggregate;
  auto q=active_->cutoff.count(source_)?active_->cutoff.at(source_):0;
  for(const auto& o:local_) {
    if(o.sequence>=q)r=add(r,o);
  }
  return r;
}
uint64_t Engine::active_version()const{std::lock_guard<std::mutex>g(mu_);return active_->version;}
bool Engine::install(const Snapshot& s,std::string* why){
  std::lock_guard<std::mutex>g(mu_); auto fail=[&](const char*x){if(why)*why=x;return false;};
  if(s.version<=active_->version)return fail("stale version");
  if(s.digest!=digest_)return fail("incompatible digest");
  if(s.crc!=crc32(s))return fail("invalid CRC");
  if(!positive_definite(s.aggregate.a))return fail("non-positive-definite design matrix");
  for(auto&[k,v]:active_->cutoff)
    if(!s.cutoff.count(k)||s.cutoff.at(k)<v)return fail("regressive cutoff");
  auto q=s.cutoff.count(source_)?s.cutoff.at(source_):0;
  local_.erase(std::remove_if(local_.begin(),local_.end(),[&](auto&o){return o.sequence<q;}),local_.end());
  active_=std::make_shared<Snapshot>(s); return true;
}
static std::vector<Assignment> schedule_model(
    const Sufficient&model,double exploration,double control_penalty,
    const std::vector<Queue>&qs,const std::vector<Rbg>&rs) {
  double l[D][D]{};
  if(!cholesky(model.a,l))throw std::runtime_error("non-positive-definite active design matrix");
  const Vec theta=cholesky_solve(l,model.b);
  // Exact branch-and-bound over the capacity-expanded bipartite graph.
  std::vector<size_t> slots;for(size_t r=0;r<rs.size();r++)for(unsigned k=0;k<rs[r].capacity;k++)slots.push_back(r);
  std::vector<unsigned> used(qs.size());std::vector<Assignment> cur,best;double bestw=-1;
  std::function<void(size_t,double)> dfs=[&](size_t n,double total){
    if(n==slots.size()){if(total>bestw){bestw=total;best=cur;}return;}
    dfs(n+1,total); auto r=slots[n];
    for(size_t q=0;q<qs.size();q++)if(used[q]<qs[q].demand){
      Vec x{};for(size_t i=0;i<D;i++)x[i]=qs[q].feature[i]*rs[r].channel[i];
      const Vec vinvx=cholesky_solve(l,x);
      const double mean=std::inner_product(x.begin(),x.end(),theta.begin(),0.0);
      const double radius=std::sqrt(std::max(0.0,
        std::inner_product(x.begin(),x.end(),vinvx.begin(),0.0)));
      const double mu=std::clamp(mean+exploration*radius,0.0,1.0);
      const double backlog=qs[q].backlog>0?qs[q].backlog:static_cast<double>(qs[q].demand);
      const double w=backlog*mu-control_penalty*qs[q].cost;
      ++used[q];cur.push_back({qs[q].id,rs[r].id,w});dfs(n+1,total+w);cur.pop_back();--used[q];
    }
  };dfs(0,0);return best;
}
std::vector<Assignment> Engine::schedule(
    const std::vector<Queue>&qs,const std::vector<Rbg>&rs)const {
  return schedule_model(current(),exploration_,control_penalty_,qs,rs);
}
CoverageDecision Engine::coverage(
    const InstalledCoverageGuard&guard,uint64_t epoch)const {
  return guard.check(epoch,current());
}
std::vector<Assignment> Engine::schedule_guarded(
    const std::vector<Queue>&qs,const std::vector<Rbg>&rs,
    const InstalledCoverageGuard&guard,uint64_t epoch,
    double conservative_exploration)const {
  if(!std::isfinite(conservative_exploration)||conservative_exploration<0)
    throw std::invalid_argument("conservative exploration");
  const Sufficient model=current();
  const auto state=guard.check(epoch,model);
  const double exploration=state.covered?exploration_:
      std::max(exploration_,conservative_exploration);
  return schedule_model(model,exploration,control_penalty_,qs,rs);
}
}
