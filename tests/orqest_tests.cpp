#include "orqest/orqest.hpp"
#include <cassert>
#include <iostream>
using namespace orqest;
static Snapshot snap(uint64_t v,uint64_t q){
  Snapshot s;s.version=v;s.digest="compat-v1";s.cutoff["du-a"]=q;
  for(size_t i=0;i<D;i++)s.aggregate.a[i*D+i]=2;s.aggregate.count=q;s.crc=crc32(s);return s;
}
int main(){
  Engine e("du-a","compat-v1",0.25,0.1); Vec x{1,0,0,0,0,0};
  e.observe({0,x,1});e.observe({1,x,2});e.observe({2,x,3});
  auto s=snap(1,2);assert(e.install(s));assert(e.current().count==3); // only seq 2 replayed
  e.observe({3,x,4});auto s2=snap(2,3);assert(e.install(s2));assert(e.current().count==4);
  std::string why;assert(!e.install(s2,&why)&&why=="stale version");
  auto bad=snap(3,2);assert(!e.install(bad,&why)&&why=="regressive cutoff");
  bad=snap(3,3);bad.crc++;assert(!e.install(bad,&why)&&why=="invalid CRC");
  bad=snap(3,3);bad.aggregate.a.fill(0);bad.crc=crc32(bad);assert(!e.install(bad,&why));
  bad=snap(3,3);bad.aggregate.a[1]=0.5;bad.crc=crc32(bad);assert(!e.install(bad,&why));
  Vec y{0,1,0,0,0,0};
  auto a=e.schedule({{"q1",1,x,10,1},{"q2",1,y,8,1}},{{"r1",1,y},{"r2",1,x}});
  assert(a.size()==2); // total RBG capacity
  double score=0;for(auto&v:a)score+=v.score;assert(score>1.0); // exact optimum
  KpmRanFunction functions[]={{1,1,false},{2,3,true}};
  KpmNodeRegistration node{functions,2};
  auto kpm=validate_kpm_registration(&node,2,3,&why);
  assert(kpm&&*kpm==1);
  assert(!validate_kpm_registration(nullptr,2,3,&why)&&why=="null node registration");
  KpmNodeRegistration null_array{nullptr,1};
  assert(!validate_kpm_registration(&null_array,2,3,&why)&&why=="null RAN-function array");
  KpmNodeRegistration empty{functions,0};
  assert(!validate_kpm_registration(&empty,2,3,&why)&&why=="empty RAN-function array");
  assert(!validate_kpm_registration(&node,2,2,&why)&&why=="unsupported KPM revision");
  int event_style=1,report_style=1;
  KpmDefinitionView def{&event_style,1,&report_style,1,0,1,true};
  assert(validate_kpm_subscription_construction(&def,&why));
  assert(!validate_kpm_subscription_construction(nullptr,&why)&&why=="null KPM definition");
  def.selected_report_style=1;
  assert(!validate_kpm_subscription_construction(&def,&why)&&why=="report-style index out of range");
  def.selected_report_style=0;def.callback_registered=false;
  assert(!validate_kpm_subscription_construction(&def,&why)&&why=="null action-definition callback");
  std::cout<<"watermark suffix capacity stale CRC SPD matching KPM transition: PASS\n";
}
