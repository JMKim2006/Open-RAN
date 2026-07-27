#include "orqest/wire.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace orqest;
namespace {
bool write_all(int fd,const uint8_t*p,size_t n){while(n){ssize_t k=send(fd,p,n,MSG_NOSIGNAL);if(k<0&&errno==EINTR)continue;if(k<=0)return false;p+=k;n-=k;}return true;}
bool read_all(int fd,uint8_t*p,size_t n){while(n){ssize_t k=recv(fd,p,n,0);if(k<0&&errno==EINTR)continue;if(k<=0)return false;p+=k;n-=k;}return true;}
bool send_frame(int fd,const std::vector<uint8_t>&b){uint32_t n=htonl(static_cast<uint32_t>(b.size()));return write_all(fd,reinterpret_cast<uint8_t*>(&n),4)&&write_all(fd,b.data(),b.size());}
bool recv_frame(int fd,std::vector<uint8_t>&b){uint32_t n;if(!read_all(fd,reinterpret_cast<uint8_t*>(&n),4))return false;n=ntohl(n);if(n==0||n>1024*1024)return false;b.resize(n);return read_all(fd,b.data(),b.size());}
void event(std::ofstream&f,const std::string&s){f<<s<<"\n";f.flush();std::cout<<s<<"\n";}
}
int main(int argc,char**argv){
  int port=39001;std::string timeline="ric-timeline.jsonl";
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--port"&&i+1<argc)port=std::stoi(argv[++i]);else if(a=="--timeline"&&i+1<argc)timeline=argv[++i];}
  std::ofstream log(timeline);if(!log)throw std::runtime_error("timeline");
  int listener=socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);if(listener<0)throw std::runtime_error(std::strerror(errno));
  int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
  sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
  if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof addr)<0||listen(listener,1)<0)throw std::runtime_error(std::strerror(errno));
  event(log,R"({"event":"ric_listen","transport":"SCTP","queue_state_received":false,"per_tti_assignment_selected":false})");
  int fd=accept(listener,nullptr,nullptr);if(fd<0)throw std::runtime_error(std::strerror(errno));
  event(log,R"({"event":"sctp_association","status":"accepted"})");
  wire::RicAggregator agg("orqest-d6-compat-v1",{"du-a"});
  std::vector<uint8_t> frame;wire::DuReport report;std::string why;
  if(!recv_frame(fd,frame)||!wire::decode_report(frame,report,&why)||!agg.accept(report,&why)){
    std::cerr<<"report rejected: "<<why<<"\n";return 2;
  }
  event(log,"{\"event\":\"report_accepted\",\"source\":\""+report.source_id+"\",\"cutoff\":"+std::to_string(report.exclusive_cutoff)+",\"count\":"+std::to_string(report.cumulative.count)+",\"active_version\":"+std::to_string(report.active_snapshot_version)+",\"compatibility_qualified\":true}");
  if(!agg.source_complete())return 3;
  event(log,R"({"event":"source_complete_prefix","sources":1,"queue_state_received":false})");
  Snapshot snap=agg.snapshot(1);
  if(!send_frame(fd,wire::encode_snapshot(snap)))return 4;
  event(log,R"({"event":"snapshot_sent","version":1,"source_cutoffs":{"du-a":4}})");
  frame.clear();wire::DuReport ack;
  if(!recv_frame(fd,frame)||!wire::decode_report(frame,ack,&why)||!agg.accept(ack,&why)){
    std::cerr<<"ack report rejected: "<<why<<"\n";return 5;
  }
  const bool active=ack.active_snapshot_version==1;
  event(log,"{\"event\":\"active_version_ack\",\"source\":\"du-a\",\"version\":"+std::to_string(ack.active_snapshot_version)+",\"accepted\":"+(active?"true":"false")+"}");
  close(fd);close(listener);
  event(log,R"({"event":"ric_complete","queue_state_received":false,"per_tti_assignment_selected":false})");
  return active?0:6;
}
