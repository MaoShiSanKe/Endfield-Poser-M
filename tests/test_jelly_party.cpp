#include "math/jelly_party.h"
#include <iostream>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool v){++checks;if(!v){std::cerr<<"Failed "<<checks<<"\n";std::exit(1);}}
int main(){
  for(int hz:{30,60,144})for(int i=0;i<hz*8;++i){
    auto s=poser_jelly_math::Sample(double(i)/hz,.6f,.65);
    check(s.y>=.4f&&s.y<=1.6f&&std::isfinite(s.x));
    check(std::abs(s.x*s.y*s.z-1)<.00001f);
  }
  for(double t:{0.,.65,1.3}){auto s=poser_jelly_math::Sample(t,.5f,.65);check(std::abs(s.y-1)<.00001);}
  auto a=poser_jelly_math::Sample(.1,.5f,.65),b=poser_jelly_math::Sample(.75,.5f,.65);
  check(std::abs(a.y-b.y)<.00001);
  check(poser_jelly_math::Sample(-1,.5f,.65).y==1);
  check(poser_jelly_math::Sample(std::numeric_limits<double>::quiet_NaN(),.5f,.65).y==1);
  check(poser_jelly_math::Sample(.1,0,.65).y==1);
  auto l=poser_jelly_math::Sample(.65-1e-7,.5f,.65),r=poser_jelly_math::Sample(.65+1e-7,.5f,.65);
  check(std::abs(l.y-r.y)<.00001);
  for(int hz:{30,60,144}) {
    double angle=0;for(int i=0;i<hz;++i)angle=poser_jelly_math::AdvanceSpin(angle,1.0/hz,90);
    check(std::abs(angle-90)<.00001);
  }
  check(poser_jelly_math::AdvanceSpin(0,.1,-90)==-9);
  check(poser_jelly_math::AdvanceSpin(179,.1,90)==-172);
  check(poser_jelly_math::AdvanceSpin(0,10,360)==90);
  check(poser_jelly_math::AdvanceSpin(17,-1,90)==17);
  check(poser_jelly_math::AdvanceSpin(42,.1,0)==42);
  check(poser_jelly_math::AdvanceSpin(0,std::numeric_limits<double>::quiet_NaN(),90)==0);
  auto baseline=Quat::FromEulerDeg(Vec3{12,33,8});
  check(std::abs(DotQ(poser_jelly_math::Spin(baseline,360),baseline))>.99999);
  check(std::abs(DotQ(poser_jelly_math::Spin(poser_jelly_math::Spin(baseline,90),-90),baseline))>.99999);
  auto facing=poser_jelly_math::Spin(Quat{},90)*Vec3{0,0,1};
  check(std::abs(facing.x-1)<.00001 && std::abs(facing.z)<.00001);
  int positive=0,negative=0;
  for(uint64_t seed=0;seed<128;++seed) {
    check(poser_jelly_math::Direction(0,seed)==1);
    check(poser_jelly_math::Direction(1,seed)==-1);
    auto d=poser_jelly_math::Direction(2,seed);
    check(d==1||d==-1);
    positive+=d>0;negative+=d<0;
  }
  check(positive>0&&negative>0);
  std::cout<<checks<<" jelly checks passed\n";
}
