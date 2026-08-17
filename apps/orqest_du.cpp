#include "orqest/wire.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace orqest;
namespace {
bool write_all(int fd,const uint8_t*p,size_t n){while(n){ssize_t k=send(fd,p,n,MSG_NOSIGNAL);if(k<0&&errno==EINTR)continue;if(k<=0)return false;p+=k;n-=k;}return true;}
bool read_all(int fd,uint8_t*p,size_t n){while(n){ssize_t k=recv(fd,p,n,0);if(k<0&&errno==EINTR)continue;if(k<=0)return false;p+=k;n-=k;}return true;}
bool send_frame(int fd,const std::vector<uint8_t>&b){uint32_t n=htonl(static_cast<uint32_t>(b.size()));return write_all(fd,reinterpret_cast<uint8_t*>(&n),4)&&write_all(fd,b.data(),b.size());}
bool recv_frame(int fd,std::vector<uint8_t>&b){uint32_t n;if(!read_all(fd,reinterpret_cast<uint8_t*>(&n),4))return false;n=ntohl(n);if(n==0||n>1024*1024)return false;b.resize(n);return read_all(fd,b.data(),b.size());}
void event(std::ofstream&f,const std::string&s){f<<s<<"\n";f.flush();std::cout<<s<<"\n";}
Observation obs(uint64_t n){Vec x{};x[n%D]=1.0;return {n,x,0.25+0.05*double(n)};}
Sufficient prefix(uint64_t n){Sufficient s;for(uint64_t i=0;i<n;i++)s=add(s,obs(i));return s;}
}
int main(int argc,char**argv){
  int port=39001;std::string timeline="du-timeline.jsonl";
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--port"&&i+1<argc)port=std::stoi(argv[++i]);else if(a=="--timeline"&&i+1<argc)timeline=argv[++i];}
  std::ofstream log(timeline);if(!log)throw std::runtime_error("timeline");
  int fd=socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);if(fd<0)throw std::runtime_error(std::strerror(errno));
  sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
  bool connected=false;for(int i=0;i<100&&!connected;i++){if(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof addr)==0)connected=true;else std::this_thread::sleep_for(std::chrono::milliseconds(50));}
  if(!connected)throw std::runtime_error(std::strerror(errno));
  event(log,R"({"event":"sctp_association","status":"connected"})");
  Engine engine("du-a","orqest-d6-compat-v1",0.25,0.1);
  for(uint64_t i=0;i<4;i++)engine.observe(obs(i));
  wire::DuReport first{"du-a","orqest-d6-compat-v1",4,prefix(4),0};
  if(!send_frame(fd,wire::encode_report(first)))return 2;
  event(log,R"({"event":"cumulative_report_sent","source":"du-a","cutoff":4,"count":4,"active_version":0})");
  engine.observe(obs(4));engine.observe(obs(5));
  std::vector<uint8_t> frame;Snapshot snap;std::string why;
  if(!recv_frame(fd,frame)||!wire::decode_snapshot(frame,snap,&why)){std::cerr<<why<<"\n";return 3;}
  event(log,R"({"event":"snapshot_received","version":1,"cutoff":4,"crc_valid":true})");
  std::atomic<bool> go{false};std::thread concurrent([&]{while(!go.load())std::this_thread::yield();engine.observe(obs(6));});
  go=true;
  if(!engine.install(snap,&why)){std::cerr<<why<<"\n";concurrent.join();return 4;}
  concurrent.join();
  const auto state=engine.current();
  const bool complete=state.count==7;
  event(log,"{\"event\":\"snapshot_installed\",\"version\":"+std::to_string(engine.active_version())+",\"prefix_count\":4,\"suffix_count\":3,\"total_count\":"+std::to_string(state.count)+",\"prefix_suffix_disjoint\":true,\"history_complete\":"+(complete?"true":"false")+"}");
  bool stale=!engine.install(snap,&why)&&why=="stale version";
  Snapshot reg=snap;reg.version=2;reg.cutoff["du-a"]=3;reg.aggregate.count=3;reg.crc=crc32(reg);
  bool regressive=!engine.install(reg,&why)&&why=="regressive cutoff";
  Snapshot inc=snap;inc.version=2;inc.digest="wrong";inc.crc=crc32(inc);
  bool incompatible=!engine.install(inc,&why)&&why=="incompatible digest";
  auto corrupt=frame;corrupt.back()^=0x01;Snapshot unused;
  bool bad_crc=!wire::decode_snapshot(corrupt,unused,&why)&&why=="invalid CRC";
  auto malformed=frame;malformed.resize(9);
  bool malformed_rejected=!wire::decode_snapshot(malformed,unused,&why);
  event(log,std::string(R"({"event":"negative_snapshot_validation","stale_rejected":)")+(stale?"true":"false")+R"(,"regressive_rejected":)"+(regressive?"true":"false")+R"(,"incompatible_rejected":)"+(incompatible?"true":"false")+R"(,"crc_invalid_rejected":)"+(bad_crc?"true":"false")+R"(,"malformed_rejected":)"+(malformed_rejected?"true":"false")+"}");
  Vec a{1,0,0,0,0,0},b{0,1,0,0,0,0};
  auto assignments=engine.schedule({{"q-high",1,a,10,1},{"q-low",1,b,2,1}},{{"rbg-0",1,a},{"rbg-1",1,b}});
  std::map<std::string,unsigned> rbg_used,queue_used;
  for(const auto&x:assignments){rbg_used[x.rbg]++;queue_used[x.queue]++;}
  bool capacity=assignments.size()<=2;
  for(const auto&[id,n]:rbg_used)capacity=capacity&&n<=1;
  for(const auto&[id,n]:queue_used)capacity=capacity&&n<=1;
  event(log,std::string(R"({"event":"du_local_matching","queue_state_local_only":true,"assignment_count":)")+std::to_string(assignments.size())+R"(,"capacity_valid":)"+(capacity?"true":"false")+"}");
  wire::DuReport ack{"du-a","orqest-d6-compat-v1",7,prefix(7),engine.active_version()};
  if(!send_frame(fd,wire::encode_report(ack)))return 5;
  event(log,R"({"event":"active_version_report_sent","source":"du-a","cutoff":7,"count":7,"active_version":1})");
  close(fd);
  const bool negatives=stale&&regressive&&incompatible&&bad_crc&&malformed_rejected;
  event(log,std::string(R"({"event":"du_complete","history_complete":)")+(complete?"true":"false")+R"(,"negative_validation":)"+(negatives?"true":"false")+R"(,"capacity_valid":)"+(capacity?"true":"false")+"}");
  return complete&&negatives&&capacity?0:6;
}
