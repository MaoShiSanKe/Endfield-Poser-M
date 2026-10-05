#pragma once
#include <algorithm>
#include <cmath>
#include "quat_math.h"
#include <cstdint>
namespace poser_jelly_math {
inline double AdvanceSpin(double angle,double seconds,float speed) {
  if(!std::isfinite(angle)||!std::isfinite(seconds)||!std::isfinite(speed))return 0;
  seconds=(std::max)(0.0,(std::min)(.25,seconds));
  speed=(std::max)(-360.f,(std::min)(360.f,speed));
  return std::remainder(angle+seconds*speed,360.0);
}
inline Quat Spin(Quat baseline,double degrees) {
  if(!std::isfinite(degrees))return baseline;
  return NormQ(Quat::AxisAngle(Vec3{0,1,0},float(degrees)*(3.14159265358979f/180.f))*baseline);
}
inline float Direction(int mode,uint64_t seed) {
  if(mode==0)return 1;if(mode==1)return -1;
  uint64_t value=seed;
  value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
  value=(value^(value>>27))*0x94d049bb133111ebULL;
  value^=value>>31;
  return (value&1)?1.f:-1.f;
}
struct Scale { float x=1,y=1,z=1; };
inline Scale Sample(double seconds,float amplitude,double period) {
  if(!std::isfinite(seconds)||seconds<0||!std::isfinite(amplitude)||!std::isfinite(period))return {};
  amplitude=(std::max)(0.f,(std::min)(.6f,amplitude));
  period=(std::max)(.2,(std::min)(3.0,period));
  double u=std::fmod(seconds,period)/period;
  double d=-amplitude*std::sin(4*3.141592653589793*u)*std::exp(-2*u)*(1-u)*(1-u);
  float y=float(1+d), side=1/std::sqrt(y);
  return {side,y,side};
}
}
