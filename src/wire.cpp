#include "orqest/wire.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace orqest::wire {
namespace {
constexpr uint16_t CONTRACT_VERSION=1;
struct Writer {
  std::vector<uint8_t> b;
  void u16(uint16_t v){b.push_back(v>>8);b.push_back(v);}
  void u32(uint32_t v){for(int s=24;s>=0;s-=8)b.push_back(v>>s);}
  void u64(uint64_t v){for(int s=56;s>=0;s-=8)b.push_back(v>>s);}
  void str(const std::string& s){if(s.size()>65535)throw std::invalid_argument("string too long");u16(s.size());b.insert(b.end(),s.begin(),s.end());}
  void dbl(double v){uint64_t n;static_assert(sizeof n==sizeof v);std::memcpy(&n,&v,sizeof n);u64(n);}
};
struct Reader {
  const std::vector<uint8_t>& b; size_t p=0,limit;
  bool take(size_t n){return n<=limit-p;}
  bool u16(uint16_t&v){if(!take(2))return false;v=(uint16_t(b[p])<<8)|b[p+1];p+=2;return true;}
  bool u32(uint32_t&v){if(!take(4))return false;v=0;for(int i=0;i<4;i++)v=(v<<8)|b[p++];return true;}
  bool u64(uint64_t&v){if(!take(8))return false;v=0;for(int i=0;i<8;i++)v=(v<<8)|b[p++];return true;}
  bool str(std::string&v){uint16_t n;if(!u16(n)||!take(n))return false;v.assign(reinterpret_cast<const char*>(b.data()+p),n);p+=n;return true;}
  bool dbl(double&v){uint64_t n;if(!u64(n))return false;std::memcpy(&v,&n,sizeof v);return std::isfinite(v);}
};
uint32_t crc_raw(const uint8_t* p,size_t n){
  uint32_t c=~0u;
  for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(int)(c&1));}
  return ~c;
}
void finish(Writer&w){w.u32(crc_raw(w.b.data(),w.b.size()));}
bool begin(const std::vector<uint8_t>&b,const char magic[4],Reader&r,std::string*why){
  auto fail=[&](const char*s){if(why)*why=s;return false;};
  if(b.size()<10)return fail("truncated frame");
  if(!std::equal(magic,magic+4,b.begin()))return fail("invalid magic");
  uint32_t got=(uint32_t(b[b.size()-4])<<24)|(uint32_t(b[b.size()-3])<<16)|
               (uint32_t(b[b.size()-2])<<8)|b.back();
  if(got!=crc_raw(b.data(),b.size()-4))return fail("invalid CRC");
  r.p=4;r.limit=b.size()-4;uint16_t v=0;
  if(!r.u16(v)||v!=CONTRACT_VERSION)return fail("unsupported contract version");
  return true;
}
void put_stats(Writer&w,const Sufficient&s){
  for(size_t i=0;i<D;i++)for(size_t j=i;j<D;j++)w.dbl(s.a[i*D+j]);
  for(double v:s.b)w.dbl(v);
}
bool get_stats(Reader&r,Sufficient&s){
  for(size_t i=0;i<D;i++)for(size_t j=i;j<D;j++){
    double v;if(!r.dbl(v))return false;s.a[i*D+j]=v;s.a[j*D+i]=v;
  }
  for(double&v:s.b)if(!r.dbl(v))return false;
  return true;
}
bool complete(Reader&r,std::string*why){if(r.p!=r.limit){if(why)*why="trailing bytes";return false;}return true;}
}

std::vector<uint8_t> encode_report(const DuReport&r){
  Writer w;w.b.insert(w.b.end(),{'O','R','Q','D'});w.u16(CONTRACT_VERSION);
  w.str(r.source_id);w.str(r.compatibility_digest);w.u64(r.exclusive_cutoff);
  w.u64(r.cumulative.count);put_stats(w,r.cumulative);w.u64(r.active_snapshot_version);
  finish(w);return w.b;
}
bool decode_report(const std::vector<uint8_t>&b,DuReport&o,std::string*why){
  Reader r{b,0,0};if(!begin(b,"ORQD",r,why))return false;
  if(!r.str(o.source_id)||!r.str(o.compatibility_digest)||!r.u64(o.exclusive_cutoff)||
     !r.u64(o.cumulative.count)||!get_stats(r,o.cumulative)||
     !r.u64(o.active_snapshot_version)){if(why)*why="truncated report";return false;}
  if(!complete(r,why))return false;
  if(o.source_id.empty()){if(why)*why="empty source";return false;}
  if(o.cumulative.count!=o.exclusive_cutoff){if(why)*why="count/cutoff mismatch";return false;}
  return true;
}

std::vector<uint8_t> encode_snapshot(const Snapshot&s){
  Writer w;w.b.insert(w.b.end(),{'O','R','Q','S'});w.u16(CONTRACT_VERSION);
  w.u64(s.version);w.str(s.digest);
  std::vector<std::pair<std::string,uint64_t>> cuts(s.cutoff.begin(),s.cutoff.end());
  std::sort(cuts.begin(),cuts.end());if(cuts.size()>65535)throw std::invalid_argument("too many sources");
  w.u16(cuts.size());for(const auto&[id,q]:cuts){w.str(id);w.u64(q);}
  put_stats(w,s.aggregate);finish(w);return w.b;
}
bool decode_snapshot(const std::vector<uint8_t>&b,Snapshot&o,std::string*why){
  Reader r{b,0,0};if(!begin(b,"ORQS",r,why))return false;
  uint16_t n=0;if(!r.u64(o.version)||!r.str(o.digest)||!r.u16(n)){if(why)*why="truncated snapshot";return false;}
  o.cutoff.clear();o.aggregate={};
  for(uint16_t i=0;i<n;i++){std::string id;uint64_t q;if(!r.str(id)||!r.u64(q)){if(why)*why="truncated provenance";return false;}
    if(id.empty()||!o.cutoff.emplace(id,q).second){if(why)*why="invalid provenance";return false;}}
  if(!get_stats(r,o.aggregate)||!complete(r,why)){if(why&&why->empty())*why="truncated snapshot";return false;}
  for(const auto&[id,q]:o.cutoff)o.aggregate.count+=q;
  o.crc=crc32(o);return true;
}

RicAggregator::RicAggregator(std::string digest,std::set<std::string> expected_sources)
 :digest_(std::move(digest)),expected_(std::move(expected_sources)){
  if(digest_.empty()||expected_.empty())throw std::invalid_argument("aggregator identity");
}
bool RicAggregator::accept(const DuReport&r,std::string*why){
  auto fail=[&](const char*s){if(why)*why=s;return false;};
  if(!expected_.count(r.source_id))return fail("unknown source");
  if(r.compatibility_digest!=digest_)return fail("incompatible digest");
  if(r.cumulative.count!=r.exclusive_cutoff)return fail("count/cutoff mismatch");
  auto it=reports_.find(r.source_id);
  if(it!=reports_.end()&&r.exclusive_cutoff<=it->second.exclusive_cutoff)
    return fail("duplicate or reordered report");
  reports_[r.source_id]=r;return true;
}
bool RicAggregator::source_complete()const{
  return std::all_of(expected_.begin(),expected_.end(),[&](const auto&s){return reports_.count(s);});
}
Snapshot RicAggregator::snapshot(uint64_t version)const{
  if(!source_complete())throw std::logic_error("source-incomplete aggregate");
  Snapshot s;s.version=version;s.digest=digest_;
  for(size_t i=0;i<D;i++)s.aggregate.a[i*D+i]=1.0;
  for(const auto&id:expected_){
    const auto&r=reports_.at(id);s.cutoff[id]=r.exclusive_cutoff;
    for(size_t i=0;i<D*D;i++)s.aggregate.a[i]+=r.cumulative.a[i];
    for(size_t i=0;i<D;i++)s.aggregate.b[i]+=r.cumulative.b[i];
    s.aggregate.count+=r.cumulative.count;
  }
  s.crc=crc32(s);return s;
}
uint64_t RicAggregator::acknowledged_version(const std::string&source)const{
  auto it=reports_.find(source);return it==reports_.end()?0:it->second.active_snapshot_version;
}
} // namespace orqest::wire
