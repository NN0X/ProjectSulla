#!/usr/bin/env bash
# Canonical Verilator reference build: max-speed model (OPT_FAST=-O3 -march=native,
# clang++, Verilator -O3 --x-assign fast --noassert). Prints circuit,ns_per_tick.
cd "$(dirname "$0")"
VINC=$(verilator --getenv VERILATOR_ROOT)/include
OPT='OPT_FAST=-O3 -march=native -funroll-loops -DNDEBUG'
OPTG='OPT_GLOBAL=-O3 -march=native -DNDEBUG'
build(){ c=$1; top=$2; src=$3; harness=$4; defs=$5
  rm -rf objf_$c
  verilator --cc -O3 --x-assign fast --x-initial fast --noassert --Mdir objf_$c --top-module $top $src >/dev/null 2>vf_$c.err \
    || { echo "$c verilate FAIL"; tail -3 vf_$c.err; return; }
  make -C objf_$c -f V$top.mk CXX=clang++ "$OPT" "$OPTG" >/dev/null 2>mf_$c.err \
    || { echo "$c make FAIL"; tail -5 mf_$c.err; return; }
  clang++ -O3 -march=native -std=c++17 $defs -I objf_$c -I "$VINC" -I "$VINC/vltstd" \
    $harness objf_$c/V${top}__ALL.a objf_$c/libverilated.a -o vf_$c 2>cf_$c.err \
    || { echo "$c link FAIL"; tail -5 cf_$c.err; return; }
  ./vf_$c
}
echo "circuit,verilator_ns_per_tick"
build full_adder full_adder full_adder.v vbench.cpp '-DTOP=Vfull_adder -DTOP_HEADER="Vfull_adder.h" -DTOP_NAME="full_adder"'
for n in 8 16 32; do
  build adder$n adder$n adder$n.v vbench.cpp "-DADDER_MODE -DTOP=Vadder$n -DTOP_HEADER=\"Vadder$n.h\" -DTOP_NAME=\"adder$n\""
done
build mul8  mul8  mul8.v  vmul.cpp '-DNBITS=8 -DTOP=Vmul8 -DTOP_HEADER="Vmul8.h" -DTOP_NAME="mul8"'
build mul16 mul16 mul16.v vmul.cpp '-DNBITS=16 -DTOP=Vmul16 -DTOP_HEADER="Vmul16.h" -DTOP_NAME="mul16"'
build cpu8  cpu8  cpu8.v  vcpu.cpp ''
