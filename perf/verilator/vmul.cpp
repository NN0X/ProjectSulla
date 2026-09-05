#include <cstdint>
#include <cstdio>
#include <chrono>
#include "verilated.h"
#include TOP_HEADER
using Clock = std::chrono::steady_clock;
template <class Step> static double timeIt(Step step, double target){
    volatile uint64_t sink=0; for(int i=0;i<2000;++i) sink+=step();
    unsigned long long total=0,batch=1000; double el=0.0;
    while(el<target){ auto t0=Clock::now(); for(unsigned long long i=0;i<batch;++i) sink+=step();
        auto t1=Clock::now(); double dt=std::chrono::duration<double>(t1-t0).count();
        el+=dt; total+=batch; if(dt>0){double r=(double)batch/dt; batch=(unsigned long long)(r*(target/4)); if(batch<1000)batch=1000;} }
    (void)sink; return 1e9*el/(double)total;
}
int main(){
    Verilated::commandArgs(0,(char**)nullptr);
    TOP* top=new TOP;
    const uint64_t MASK=(NBITS>=64)?~0ull:((1ull<<NBITS)-1);

    int bad=0; uint64_t tv[][2]={{0,0},{1,1},{MASK,MASK},{123,45},{200,201},{65535,65535},{12345,54321}};
    for(auto&t:tv){ uint64_t A=t[0]&MASK,Bv=t[1]&MASK; top->a=A; top->b=Bv; top->eval();
        uint64_t got=(uint64_t)top->p, want=(A*Bv)&((NBITS*2>=64)?~0ull:((1ull<<(NBITS*2))-1));
        if(got!=want){bad++; if(bad<=3) printf("  MUL MISMATCH a=%llu b=%llu got=%llu want=%llu\n",(unsigned long long)A,(unsigned long long)Bv,(unsigned long long)got,(unsigned long long)want);} }
    if(bad){ printf("%s,FAIL\n",TOP_NAME); return 1; }
    uint64_t x=0;
    auto step=[&]()->uint64_t{ x^=1ull; top->a=(0xA5A5A5A5ull^x)&MASK; top->b=(0x5A5A5A5Aull^x)&MASK; top->eval(); return (uint64_t)top->p; };
    double v[3]; for(int r=0;r<3;++r) v[r]=timeIt(step,0.30);
    double m = v[0]<v[1]?(v[1]<v[2]?v[1]:(v[0]<v[2]?v[2]:v[0])):(v[0]<v[2]?v[0]:(v[1]<v[2]?v[2]:v[1]));
    printf("%s,%.2f\n",TOP_NAME,m);
    delete top; return 0;
}
