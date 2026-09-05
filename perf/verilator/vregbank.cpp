// Verilator regbank harness (clocked). One clock cycle = one Sulla tick.
#include <cstdint>
#include <cstdio>
#include <chrono>
#include "verilated.h"
#include TOP_HEADER
using Clock=std::chrono::steady_clock;
static TOP* T;
static inline void cycle(){ T->clk=0; T->eval(); T->clk=1; T->eval(); }
template<class S> static double timeIt(S step,double target){
  volatile uint64_t sink=0; for(int i=0;i<2000;++i) sink+=step();
  unsigned long long total=0,batch=1000; double el=0;
  while(el<target){ auto t0=Clock::now(); for(unsigned long long i=0;i<batch;++i) sink+=step();
    auto t1=Clock::now(); double dt=std::chrono::duration<double>(t1-t0).count();
    el+=dt; total+=batch; if(dt>0){double r=(double)batch/dt; batch=(unsigned long long)(r*(target/4)); if(batch<1000)batch=1000;} }
  (void)sink; return 1e9*el/(double)total;
}
int main(){
  Verilated::commandArgs(0,(char**)nullptr); T=new TOP;
  uint64_t k=0;
  auto step=[&]()->uint64_t{ T->D=(0xA5A5A5A5A5A5A5A5ull^(++k)); T->EN=0xFFFFFFFFFFFFFFFFull; cycle(); return (uint64_t)T->Q; };
  double v[3]; for(int r=0;r<3;++r) v[r]=timeIt(step,0.30);
  double m=v[0]<v[1]?(v[1]<v[2]?v[1]:(v[0]<v[2]?v[2]:v[0])):(v[0]<v[2]?v[0]:(v[1]<v[2]?v[2]:v[1]));
  printf("%s,%.2f\n",TOP_NAME,m); delete T; return 0;
}
