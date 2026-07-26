#include "orqest/orqest.hpp"
#include <cassert>
#include <iostream>
using namespace orqest;
static Snapshot snap(uint64_t v,uint64_t q){
  Snapshot s;s.version=v;s.digest="compat-v1";s.cutoff["du-a"]=q;
  for(size_t i=0;i<D;i++)s.aggregate.a[i*D+i]=2;s.aggregate.count=q;s.crc=crc32(s);return s;
}
int main(){
  Engine e("du-a","compat-v1",0.25); Vec x{1,0,0,0,0,0};
  e.observe({0,x,1});e.observe({1,x,2});e.observe({2,x,3});
  auto s=snap(1,2);assert(e.install(s));assert(e.current().count==3); // only seq 2 replayed
  e.observe({3,x,4});auto s2=snap(2,3);assert(e.install(s2));assert(e.current().count==4);
  std::string why;assert(!e.install(s2,&why)&&why=="stale version");
  auto bad=snap(3,2);assert(!e.install(bad,&why)&&why=="regressive cutoff");
  bad=snap(3,3);bad.crc++;assert(!e.install(bad,&why)&&why=="invalid CRC");
  bad=snap(3,3);bad.aggregate.a.fill(0);bad.crc=crc32(bad);assert(!e.install(bad,&why));
  Vec y{0,1,0,0,0,0};
  auto a=e.schedule({{"q1",1,x},{"q2",1,y}},{{"r1",1,y},{"r2",1,x}});
  assert(a.size()==2); // total RBG capacity
  double score=0;for(auto&v:a)score+=v.score;assert(score>2.4); // crossed greedy trap, exact optimum
  std::cout<<"watermark suffix capacity stale CRC SPD matching: PASS\n";
}
