#include "orqest/orqest.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace orqest {
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
  double l[D][D]{};
  for(size_t i=0;i<D;i++) for(size_t j=0;j<=i;j++) {
    double v=a[i*D+j]; for(size_t k=0;k<j;k++)v-=l[i][k]*l[j][k];
    if(i==j){if(!(v>1e-12)||!std::isfinite(v))return false;l[i][j]=std::sqrt(v);}
    else l[i][j]=v/l[j][j];
  } return true;
}
Sufficient add(const Sufficient& s,const Observation& o) {
  Sufficient r=s; for(size_t i=0;i<D;i++){r.b[i]+=o.x[i]*o.reward;
    for(size_t j=0;j<D;j++)r.a[i*D+j]+=o.x[i]*o.x[j];}
  ++r.count; return r;
}
Engine::Engine(std::string source,std::string digest,double exploration)
 :source_(std::move(source)),digest_(std::move(digest)),exploration_(exploration) {
  if(exploration_<0)throw std::invalid_argument("exploration");
  Snapshot s; s.digest=digest_; for(size_t i=0;i<D;i++)s.aggregate.a[i*D+i]=1;
  s.crc=crc32(s); active_=std::make_shared<Snapshot>(s);
}
void Engine::observe(const Observation& o){std::lock_guard<std::mutex> g(mu_);
  if(!local_.empty()&&o.sequence<=local_.back().sequence)throw std::invalid_argument("sequence");
  local_.push_back(o);
}
Sufficient Engine::current() const {std::lock_guard<std::mutex> g(mu_);auto r=active_->aggregate;
  auto q=active_->cutoff.count(source_)?active_->cutoff.at(source_):0;
  for(auto&o:local_)if(o.sequence>=q)r=add(r,o); return r;
}
uint64_t Engine::active_version()const{std::lock_guard<std::mutex>g(mu_);return active_->version;}
bool Engine::install(const Snapshot& s,std::string* why){
  std::lock_guard<std::mutex>g(mu_); auto fail=[&](const char*x){if(why)*why=x;return false;};
  if(s.version<=active_->version)return fail("stale version");
  if(s.digest!=digest_)return fail("incompatible digest");
  if(s.crc!=crc32(s))return fail("invalid CRC");
  if(!positive_definite(s.aggregate.a))return fail("non-positive-definite design matrix");
  for(auto&[k,v]:active_->cutoff)if(s.cutoff.count(k)&&s.cutoff.at(k)<v)return fail("regressive cutoff");
  auto q=s.cutoff.count(source_)?s.cutoff.at(source_):0;
  local_.erase(std::remove_if(local_.begin(),local_.end(),[&](auto&o){return o.sequence<q;}),local_.end());
  active_=std::make_shared<Snapshot>(s); return true;
}
std::vector<Assignment> Engine::schedule(const std::vector<Queue>&qs,const std::vector<Rbg>&rs)const{
  // Exact branch-and-bound over the capacity-expanded bipartite graph.
  std::vector<size_t> slots;for(size_t r=0;r<rs.size();r++)for(unsigned k=0;k<rs[r].capacity;k++)slots.push_back(r);
  std::vector<unsigned> used(qs.size());std::vector<Assignment> cur,best;double bestw=-1;
  std::function<void(size_t,double)> dfs=[&](size_t n,double total){
    if(n==slots.size()){if(total>bestw){bestw=total;best=cur;}return;}
    dfs(n+1,total); auto r=slots[n];
    for(size_t q=0;q<qs.size();q++)if(used[q]<qs[q].demand){
      double norm=std::inner_product(qs[q].feature.begin(),qs[q].feature.end(),qs[q].feature.begin(),0.0);
      double w=std::inner_product(qs[q].feature.begin(),qs[q].feature.end(),rs[r].channel.begin(),0.0)
               +exploration_*std::sqrt(norm);
      ++used[q];cur.push_back({qs[q].id,rs[r].id,w});dfs(n+1,total+w);cur.pop_back();--used[q];
    }
  };dfs(0,0);return best;
}
}
