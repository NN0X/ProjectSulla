#include <cstdint>
#include <cstdio>
#include <chrono>
#include "verilated.h"
#include "Vcpu8.h"
using Clock=std::chrono::steady_clock;
static Vcpu8* T;
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
  Verilated::commandArgs(0,(char**)nullptr); T=new Vcpu8;

  T->op=2; T->imm=1; T->clk=0; T->eval();
  int bad=0;
  for(int c=1;c<=20;c++){ cycle(); if((int)T->acc!=(c&0xFF)){bad++; if(bad<=3) printf("  CPU MISMATCH cycle %d acc=%u want=%d\n",c,(unsigned)T->acc,c&0xFF);} }
  if(bad){ printf("cpu8,FAIL\n"); return 1; }

  uint8_t k=0;
  auto step=[&]()->uint64_t{ T->op=2; T->imm=(++k); cycle(); return (uint64_t)T->acc; };
  double v[3]; for(int r=0;r<3;++r) v[r]=timeIt(step,0.30);
  double m=v[0]<v[1]?(v[1]<v[2]?v[1]:(v[0]<v[2]?v[2]:v[0])):(v[0]<v[2]?v[0]:(v[1]<v[2]?v[2]:v[1]));
  printf("cpu8,%.2f\n",m);
  delete T; return 0;
}
