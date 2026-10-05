#include "core/http_request.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>
using R = poser_http::Request;
static int checks=0;
static void check(bool b) { ++checks; if(!b){std::cerr<<"Failed check "<<checks<<"\n";std::exit(1);} }
static R parse(const std::string& s) { R r;r.append(s.data(),s.size());return r; }
int main() {
  const std::string body="{\"frozen\":false}";
  const std::string head="POST /api/freeze HTTP/1.1\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n";
  const auto all=head+body;
  for(size_t split=0;split<all.size();++split) {
    R r;check(r.append(all.data(),split)==R::State::Waiting);
    check(r.append(all.data()+split,all.size()-split)==R::State::Complete);
    check(r.path=="/api/freeze" && r.body==body);
  }
  R one;for(char c:all)one.append(&c,1);check(one.state==R::State::Complete&&one.body==body);
  std::string large(20000,'x');R r;
  auto h="POST /api/pose HTTP/1.1\r\ncontent-length: 20000\r\n\r\n";
  r.append(h,std::char_traits<char>::length(h));
  for(size_t p=0;p<large.size();p+=4096)r.append(large.data()+p,(std::min)(size_t(4096),large.size()-p));
  check(r.state==R::State::Complete&&r.body==large);
  check(parse("GET /api/status HTTP/1.1\r\n\r\n").state==R::State::Complete);
  for(auto length:{"-1","xyz","1048577","9999999999999999999999999999",""})
    check(parse(std::string("POST / HTTP/1.1\r\nContent-Length: ")+length+"\r\n\r\n").state==R::State::Invalid);
  check(parse("POST / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx").state==R::State::Invalid);
  check(parse("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n").state==R::State::Invalid);
  check(parse("POST / HTTP/1.1\r\n\r\n").state==R::State::Invalid);
  check(parse("GET / HTTP/1.1\r\nBadheader\r\n\r\n").state==R::State::Invalid);
  check(parse(std::string(R::MaxHeader,'x')).state==R::State::Invalid);
  check(parse("POST / HTTP/1.1\r\nContent-Length: 2\r\n\r\nx").state==R::State::Waiting);
  check(parse("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n").state==R::State::Complete);
  std::cout<<checks<<" HTTP request checks passed\n";
}
