#include "orqest/wire.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <thread>
using namespace orqest;

static Observation make_obs(uint64_t n){Vec x{};x[n%D]=1;return {n,x,0.5};}
static Sufficient stats(uint64_t n){Sufficient s;for(uint64_t i=0;i<n;i++)s=add(s,make_obs(i));return s;}
static wire::DuReport report(const char*id,uint64_t q,uint64_t active=0,const char*digest="orqest-d6-compat-v1"){
  return {id,digest,q,stats(q),active};
}
int main(){
  std::string why;
  auto encoded=wire::encode_report(report("du-a",2));
  assert(encoded[0]=='O'&&encoded[1]=='R'&&encoded[2]=='Q'&&encoded[3]=='D');
  assert(encoded[4]==0&&encoded[5]==1); // network-order contract version
  wire::DuReport decoded;assert(wire::decode_report(encoded,decoded,&why));
  assert(decoded.source_id=="du-a"&&decoded.exclusive_cutoff==2&&decoded.cumulative.count==2);
  auto bad=encoded;bad.back()^=1;assert(!wire::decode_report(bad,decoded,&why)&&why=="invalid CRC");
  bad.resize(8);assert(!wire::decode_report(bad,decoded,&why)); // malformed/truncated

  wire::RicAggregator agg("orqest-d6-compat-v1",{"du-a","du-b"});
  assert(agg.accept(report("du-a",2),&why));
  assert(!agg.source_complete()); // deterministic message loss: du-b absent
  assert(!agg.accept(report("du-a",2),&why)&&why=="duplicate or reordered report");
  assert(!agg.accept(report("du-a",1),&why)&&why=="duplicate or reordered report");
  assert(!agg.accept(report("du-b",2,0,"other"),&why)&&why=="incompatible digest");
  assert(agg.accept(report("du-b",2),&why)&&agg.source_complete());
  Snapshot s=agg.snapshot(1);
  assert(s.cutoff.size()==2&&s.cutoff.at("du-a")==2&&s.cutoff.at("du-b")==2);
  auto sw=wire::encode_snapshot(s);Snapshot round;assert(wire::decode_snapshot(sw,round,&why));
  assert(round.version==1&&round.aggregate.count==4&&round.cutoff==s.cutoff);
  auto sw_bad=sw;sw_bad[10]^=1;assert(!wire::decode_snapshot(sw_bad,round,&why)&&why=="invalid CRC");

  Engine du("du-a","orqest-d6-compat-v1",0.25,0.1);
  for(uint64_t i=0;i<5;i++)du.observe(make_obs(i));
  std::atomic<bool> go{false};
  std::thread concurrent([&]{while(!go.load())std::this_thread::yield();du.observe(make_obs(5));});
  go=true;assert(du.install(s,&why));concurrent.join();
  // Global prefix=4 (two sources), DU-local suffix is exactly seq >= 2: 2,3,4,5.
  assert(du.current().count==8); // disjoint and complete; no duplication or permanent hole
  assert(du.active_version()==1);
  assert(!du.install(s,&why)&&why=="stale version"); // delayed duplicate snapshot
  Snapshot reg=s;reg.version=2;reg.cutoff["du-a"]=1;reg.crc=crc32(reg);
  assert(!du.install(reg,&why)&&why=="regressive cutoff");
  Snapshot inc=s;inc.version=2;inc.digest="other";inc.crc=crc32(inc);
  assert(!du.install(inc,&why)&&why=="incompatible digest");

  Vec x{1,0,0,0,0,0},y{0,1,0,0,0,0};
  auto a=du.schedule({{"q1",2,x,12,1},{"q2",1,y,4,1}},{{"r1",1,x},{"r2",2,y}});
  std::map<std::string,unsigned> rbgs,queues;
  for(const auto&v:a){rbgs[v.rbg]++;queues[v.queue]++;}
  assert(rbgs["r1"]<=1&&rbgs["r2"]<=2&&queues["q1"]<=2&&queues["q2"]<=1);
  assert(a.size()<=3);
  std::cout<<"loss duplication reordering delayed stale regressive digest CRC concurrent suffix holes matching: PASS\n";
}
