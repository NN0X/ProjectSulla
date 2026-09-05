#!/usr/bin/env bash
cd "$(dirname "$0")"
VINC=$(verilator --getenv VERILATOR_ROOT)/include
echo "circuit,verilator_ns_per_tick"
for spec in "mul8 8 gate" "mul8 8 behav" "mul16 16 gate" "mul16 16 behav"; do
  set -- $spec; c=$1; N=$2; kind=$3
  src="$c.v"; tag="$c"; [ "$kind" = "behav" ] && src="${c}_behav.v" && tag="${c}_behav"
  rm -rf obj_$tag
  verilator --cc --build -O3 -CFLAGS "-O3 -march=native" --Mdir obj_$tag --top-module $c $src >/dev/null 2>vlog_$tag.err || { echo "$tag verilate FAILED"; tail -3 vlog_$tag.err; continue; }
  clang++ -O3 -march=native -std=c++17 -DNBITS=$N -DTOP=V$c -DTOP_HEADER="\"V$c.h\"" -DTOP_NAME="\"$tag\"" \
    -I obj_$tag -I "$VINC" -I "$VINC/vltstd" vmul.cpp obj_$tag/V${c}__ALL.a obj_$tag/libverilated.a -o vmul_$tag 2>cc_$tag.err || { echo "$tag compile FAILED"; tail -5 cc_$tag.err; continue; }
  ./vmul_$tag
done
