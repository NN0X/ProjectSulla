#include <cstdint>
#include <dlfcn.h>
#include <functional>
#include <string>
#include <vector>

#include <filesystem>

#include "part.h"
#include "primitives.h"
#include "utils.h"
#include "appstate.h"
#include "compiler/compiler.h"

#include "framework.h"

namespace fs = std::filesystem;

static std::vector<State> toStates(const std::vector<int>& bits)
{
        std::vector<State> s(bits.size());
        for (size_t i = 0; i < bits.size(); ++i) s[i] = bits[i] ? STATE_HIGH : STATE_LOW;
        return s;
}

static std::vector<int> toBits(const std::vector<State>& states)
{
        std::vector<int> b(states.size());
        for (size_t i = 0; i < states.size(); ++i) b[i] = (states[i] == STATE_HIGH) ? 1 : 0;
        return b;
}

static std::vector<std::string> g_builtModules;

static Part buildNative(const std::string& layoutName, int& nOut, bool linkMode)
{
        AppState state;
        loadLayout(state, "layouts/" + layoutName + ".json");

        nOut = 0;
        for (const auto& kv : state.partTypes)
                if (kv.second == PART_TYPE_OUTPUT) nOut += state.inputCounts.count(kv.first) ? state.inputCounts.at(kv.first) : 0;

        std::string mod = "vtest_" + layoutName + (linkMode ? "_link" : "");
        std::string code = transpileToCpp(state, linkMode);
        if (!compileSharedLibrary(code, mod)) return nullptr;
        g_builtModules.push_back(mod);
        return loadCompiledPart(mod, nOut);
}

static void cleanupModules()
{
        for (const std::string& mod : g_builtModules)
        {
                unloadCompiledPart(mod);
                std::error_code ec;
                fs::remove("parts/lib" + mod + ".so", ec);
                fs::remove("parts/lib" + mod + ".dll", ec);
                fs::remove_all("parts/" + mod, ec);
        }
}

using GoldenFn = std::function<std::vector<int>(const std::vector<int>&)>;

static void testCombinational(const std::string& name, int nIn, const GoldenFn& golden)
{
        tf::section("combinational: " + name);

        int interpIn = 0, interpOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", interpIn, interpOut);
        if (!tf::check(interp != nullptr, name + ": interpreted engine loaded"))
                return;
        tf::check(interpIn == nIn,
                  name + ": interpreted input arity == " + std::to_string(nIn),
                  "got " + std::to_string(interpIn));

        int natOut = 0;
        Part native = buildNative(name, natOut, /*linkMode=*/false);
        if (!tf::check(native != nullptr, name + ": native engine compiled + loaded"))
                return;

        int interpFails = 0, nativeFails = 0, diffFails = 0;
        int cases = 1 << nIn;
        for (int m = 0; m < cases; ++m)
        {
                std::vector<int> in(nIn);
                for (int i = 0; i < nIn; ++i) in[i] = (m >> i) & 1;

                std::vector<int> want = golden(in);
                std::vector<int> gotI = toBits(interp(toStates(in)));
                std::vector<int> gotN = toBits(native(toStates(in)));

                if (gotI != want) interpFails++;
                if (gotN != want) nativeFails++;
                if (gotI != gotN) diffFails++;
        }

        std::string suffix = "  (" + std::to_string(cases) + " input combinations)";
        tf::check(interpFails == 0, name + ": interpreted matches golden" + suffix,
                  std::to_string(interpFails) + " mismatched cases");
        tf::check(nativeFails == 0, name + ": native matches golden" + suffix,
                  std::to_string(nativeFails) + " mismatched cases");
        tf::check(diffFails == 0, name + ": interpreted == native" + suffix,
                  std::to_string(diffFails) + " divergent cases");
}

static std::vector<int> settle(Part& p, const std::vector<int>& in, int steps)
{
        std::vector<State> out;
        std::vector<State> sin = toStates(in);
        for (int i = 0; i < steps; ++i) out = p(sin);
        return toBits(out);
}

static void srRef(int S, int R, int& Q, int& Qn)
{
        for (int i = 0; i < 8; ++i)
        {
                int nq  = !(S || Qn);
                int nqn = !(R || nq);
                Q = nq; Qn = nqn;
        }
}

static void testSrLatch()
{
        tf::section("sequential: sr_latch (NOR feedback)");

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/sr_latch.json", iIn, iOut);
        if (!tf::check(interp != nullptr, "sr_latch: interpreted engine loaded")) return;

        int nOut = 0;
        Part native = buildNative("sr_latch", nOut, false);
        if (!tf::check(native != nullptr, "sr_latch: native engine compiled + loaded")) return;

        std::vector<std::pair<int,int>> seq = {{1,0},{0,0},{0,1},{0,0},{1,0},{0,0}};
        const char* names[] = {"set","hold","reset","hold","set","hold"};

        int refQ = 0, refQn = 1;
        int interpFails = 0, nativeFails = 0, diffFails = 0;
        const int STEPS = 16;
        for (size_t k = 0; k < seq.size(); ++k)
        {
                int S = seq[k].first, R = seq[k].second;
                srRef(S, R, refQ, refQn);
                std::vector<int> want = { refQ, refQn };

                std::vector<int> gotI = settle(interp, {S, R}, STEPS);
                std::vector<int> gotN = settle(native, {S, R}, STEPS);

                std::string tag = std::string("[") + names[k] + " S=" + std::to_string(S) + " R=" + std::to_string(R) + "]";
                if (gotI != want) { interpFails++; }
                if (gotN != want) { nativeFails++; }
                if (gotI != gotN) { diffFails++; }
                tf::checkEq(gotI, want, "sr_latch interpreted " + tag);
                tf::checkEq(gotN, want, "sr_latch native      " + tag);
        }
        tf::check(diffFails == 0, "sr_latch: interpreted == native across sequence",
                  std::to_string(diffFails) + " divergent steps");
}

static void testClock()
{
        tf::section("sequential: clock (per-tick toggle)");

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/clock.json", iIn, iOut);
        if (!tf::check(interp != nullptr, "clock: interpreted engine loaded")) return;

        int nOut = 0;
        Part native = buildNative("clock", nOut, false);
        if (!tf::check(native != nullptr, "clock: native engine compiled + loaded")) return;

        std::vector<int> noInput;
        std::vector<int> golden, gotI, gotN;
        const int TICKS = 8;
        for (int i = 0; i < TICKS; ++i)
        {
                golden.push_back((i % 2 == 0) ? 1 : 0);
                gotI.push_back(interp(toStates(noInput))[0] == STATE_HIGH ? 1 : 0);
                gotN.push_back(native(toStates(noInput))[0] == STATE_HIGH ? 1 : 0);
        }
        tf::checkEq(gotI, golden, "clock: interpreted toggles 1010... over 8 ticks");
        tf::checkEq(gotN, golden, "clock: native toggles 1010... over 8 ticks");
        tf::check(gotI == gotN, "clock: interpreted == native");
}


static void testPinOrderConsistency()
{
        tf::section("pin-order consistency: hierarchical CUSTOM interp == native(inline) == native(dynamic-link)");
        {
                AppState child;
                loadLayout(child, "layouts/pinorder_child.json");
                std::string code = transpileToCpp(child, false);
                compilePartLibrary(code, "pinorder_child", false, true);
                g_builtModules.push_back("pinorder_child");
        }
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/pinorder_parent.json", iIn, iOut);
        int inlOut = 0, lnkOut = 0;
        Part inl = buildNative("pinorder_parent", inlOut, false);
        Part lnk = buildNative("pinorder_parent", lnkOut, true);
        if (!tf::check(interp != nullptr && inl != nullptr && lnk != nullptr, "pinorder: all three engines built")) return;
        int inlFails = 0, lnkFails = 0;
        for (int m = 0; m < (1 << iIn); ++m)
        {
                std::vector<int> in(iIn);
                for (int k = 0; k < iIn; ++k) in[k] = (m >> k) & 1;
                std::vector<int> gi = toBits(interp(toStates(in)));
                std::vector<int> gn = toBits(inl(toStates(in)));
                std::vector<int> gl = toBits(lnk(toStates(in)));
                if (gi != gn) inlFails++;
                if (gi != gl) lnkFails++;
        }
        tf::check(inlFails == 0, "pinorder: interpreted == native(inline)", std::to_string(inlFails) + " mismatched");
        tf::check(lnkFails == 0, "pinorder: interpreted == native(dynamic-link)", std::to_string(lnkFails) + " mismatched");
}

static void testNativeLink()
{
        tf::section("native dynamic-link: adder2 via libfull_adder.so");

        {
                AppState child;
                loadLayout(child, "layouts/full_adder.json");
                std::string code = transpileToCpp(child, false);
                bool ok = compilePartLibrary(code, "full_adder", false, true);
                g_builtModules.push_back("full_adder");
                if (!tf::check(ok && fs::exists("parts/full_adder/libfull_adder.so"),
                               "adder2: child library parts/full_adder/libfull_adder.so built"))
                        return;
        }

        int nOut = 0;
        Part native = buildNative("adder2", nOut, true);
        if (!tf::check(native != nullptr, "adder2: linked native engine compiled + loaded")) return;

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/adder2.json", iIn, iOut);
        if (!tf::check(interp != nullptr, "adder2: interpreted engine loaded")) return;

        auto golden = [](int A0,int A1,int B0,int B1){
                int A = A0 | (A1 << 1);
                int B = B0 | (B1 << 1);
                int sum = A + B;
                return std::vector<int>{ sum & 1, (sum >> 1) & 1, (sum >> 2) & 1 };
        };

        int interpFails = 0, linkFails = 0, diffFails = 0;
        for (int m = 0; m < 16; ++m)
        {
                int A0 = m & 1, A1 = (m >> 1) & 1, B0 = (m >> 2) & 1, B1 = (m >> 3) & 1;
                std::vector<int> in = { A0, A1, B0, B1 };
                std::vector<int> want = golden(A0, A1, B0, B1);
                std::vector<int> gotI = toBits(interp(toStates(in)));
                std::vector<int> gotL = toBits(native(toStates(in)));
                if (gotI != want) interpFails++;
                if (gotL != want) linkFails++;
                if (gotI != gotL) diffFails++;
        }
        tf::check(interpFails == 0, "adder2: interpreted matches golden (16 cases)",
                  std::to_string(interpFails) + " mismatched");
        tf::check(linkFails == 0, "adder2: native(dynamic-link) matches golden (16 cases)",
                  std::to_string(linkFails) + " mismatched");
        tf::check(diffFails == 0, "adder2: interpreted == native(dynamic-link) (16 cases)",
                  std::to_string(diffFails) + " divergent");
}

static void testPerInstanceState()
{
        tf::section("per-instance state: regbank8 (8 stateful dff instances)");

        {
                AppState child;
                loadLayout(child, "layouts/dff.json");
                std::string code = transpileToCpp(child, false);
                bool ok = compilePartLibrary(code, "dff", true, true);
                g_builtModules.push_back("dff");
                if (!tf::check(ok, "regbank8: stateful child dff library built")) return;
                tf::check(!sullaCodeIsStateless(code),
                          "dff: detected as stateful (internal feedback state)");
        }

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/regbank8.json", iIn, iOut);
        if (!tf::check(interp != nullptr, "regbank8: interpreted engine loaded")) return;

        const int N = 8, T = 5;
        const int Dv[T] = { 0x55, 0x00, 0xF0, 0xAA, 0x0F };
        const int Ev[T] = { 0xFF, 0x00, 0x0F, 0xFF, 0xF0 };
        std::vector<int> golden; int g = 0;
        for (int t = 0; t < T; ++t) { g = (Dv[t] & Ev[t]) | (g & ~Ev[t]); golden.push_back(g & 0xFF); }

        auto runSeq = [&](Part& p) {
                std::vector<int> qs;
                for (int t = 0; t < T; ++t) {
                        std::vector<int> in(2 * N, 0);
                        for (int k = 0; k < N; ++k) { in[k] = (Dv[t] >> k) & 1; in[N + k] = (Ev[t] >> k) & 1; }
                        std::vector<int> o = toBits(p(toStates(in)));
                        int q = 0; for (int k = 0; k < (int)o.size(); ++k) q |= (o[k] << k);
                        qs.push_back(q);
                }
                return qs;
        };

        std::vector<int> gotI = runSeq(interp);
        tf::checkEq(gotI, golden, "regbank8: interpreted keeps independent per-register state");

        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m) {
                int nOut = 0;
                Part nat = buildNative("regbank8", nOut, linkMode[m]);
                if (!tf::check(nat != nullptr,
                               std::string("regbank8: native ") + modeName[m] + " compiled + loaded")) continue;
                std::vector<int> gotN = runSeq(nat);
                tf::checkEq(gotN, golden,
                            std::string("regbank8: native ") + modeName[m] + " keeps independent per-register state");
                tf::check(gotN == gotI,
                          std::string("regbank8: interpreted == native ") + modeName[m]);
        }
}

struct RegStep { int data; int clk; int ctrl; };

static std::vector<int> runRegisterOnEngine(Part& p, const std::vector<RegStep>& seq, int settle)
{
        std::vector<int> qs;
        for (const RegStep& st : seq)
        {
                int q = 0;
                for (int t = 0; t < settle; ++t)
                {
                        std::vector<int> in(10, 0);
                        for (int k = 0; k < 8; ++k) in[k] = (st.data >> k) & 1;
                        in[8] = st.clk; in[9] = st.ctrl;
                        std::vector<int> o = toBits(p(toStates(in)));
                        q = 0; for (int k = 0; k < (int)o.size(); ++k) q |= (o[k] << k);
                }
                qs.push_back(q);
        }
        return qs;
}

static void checkRegister(const std::string& name, const std::vector<RegStep>& seq, const std::vector<int>& golden)
{
        const int SETTLE = 16;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, name + ": interpreted engine loaded")) return;
        std::vector<int> gotI = runRegisterOnEngine(interp, seq, SETTLE);
        tf::checkEq(gotI, golden, name + ": interpreted matches golden sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(name, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, name + ": native " + modeName[m] + " built")) continue;
                std::vector<int> gotN = runRegisterOnEngine(nat, seq, SETTLE);
                tf::checkEq(gotN, golden, name + ": native " + modeName[m] + " matches golden");
        }
}

static void testEdgeRegister()
{
        tf::section("74273 octal D register (edge master-slave, async clear)");
        std::vector<RegStep> seq = { {0x00,0,1},{0xA5,0,1},{0xA5,1,1},{0x00,1,1},{0x00,0,1},{0x3C,0,1},{0x3C,1,1},{0xFF,0,0},{0xFF,1,0},{0x0F,0,1},{0x0F,1,1} };
        std::vector<int> golden;
        {
                int g = 0, prev = 0;
                for (const RegStep& st : seq)
                {
                        if (!st.ctrl) g = 0;
                        else if (st.clk && !prev) g = st.data;
                        prev = st.clk;
                        golden.push_back(g);
                }
        }
        checkRegister("74273_Octal_D_Flip-Flop_with_Clear", seq, golden);
}

static void testEnableRegister()
{
        tf::section("74377 octal D register (edge master-slave, clock enable)");
        std::vector<RegStep> seq = { {0x00,0,0},{0xC3,0,0},{0xC3,1,0},{0x00,1,0},{0x00,0,0},{0x55,0,0},{0x55,1,0},{0xFF,0,1},{0xFF,1,1},{0xF0,0,1},{0xF0,1,1},{0x0F,0,0},{0x0F,1,0} };
        std::vector<int> golden;
        {
                int g = 0, prev = 0;
                for (const RegStep& st : seq)
                {
                        int en = !st.ctrl;
                        if (st.clk && !prev && en) g = st.data;
                        prev = st.clk;
                        golden.push_back(g);
                }
        }
        checkRegister("74377_Octal_D_Register_with_Enable", seq, golden);
}

struct CntStep { int d; int nload; int nclr; int enp; int ent; int clk; };

static void runCounterOnEngine(Part& p, const std::vector<CntStep>& seq, int settle, std::vector<int>& qs, std::vector<int>& rs)
{
        for (const CntStep& st : seq)
        {
                int q = 0, r = 0;
                for (int t = 0; t < settle; ++t)
                {
                        std::vector<int> in(9, 0);
                        for (int k = 0; k < 4; ++k) in[k] = (st.d >> k) & 1;
                        in[4] = st.nload; in[5] = st.nclr; in[6] = st.enp; in[7] = st.ent; in[8] = st.clk;
                        std::vector<int> o = toBits(p(toStates(in)));
                        q = 0; for (int k = 0; k < 4; ++k) q |= (o[k] << k);
                        r = ((int)o.size() > 4) ? o[4] : 0;
                }
                qs.push_back(q); rs.push_back(r);
        }
}

static void testCounter()
{
        tf::section("74163 4-bit synchronous counter (sync clear, load, count, RCO)");
        const std::string NAME = "74163_4-bit_Synchronous_Binary_Counter";
        const int SETTLE = 24;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "74163: interpreted engine loaded")) return;
        std::vector<CntStep> seq;
        seq.push_back({0,1,0,0,0,0}); seq.push_back({0,1,0,0,0,1});
        seq.push_back({0x9,0,1,0,0,0}); seq.push_back({0x9,0,1,0,0,1});
        for (int k = 0; k < 10; ++k) { seq.push_back({0,1,1,1,1,0}); seq.push_back({0,1,1,1,1,1}); }
        seq.push_back({0,1,1,0,1,0}); seq.push_back({0,1,1,0,1,1});
        std::vector<int> goldenQ, goldenR;
        {
                int q = 0, prev = 0;
                for (const CntStep& st : seq)
                {
                        if (st.clk && !prev) { if (!st.nclr) q = 0; else if (!st.nload) q = st.d; else if (st.enp && st.ent) q = (q + 1) & 15; }
                        prev = st.clk;
                        goldenQ.push_back(q); goldenR.push_back((st.ent && q == 15) ? 1 : 0);
                }
        }
        std::vector<int> qi, ri; runCounterOnEngine(interp, seq, SETTLE, qi, ri);
        tf::checkEq(qi, goldenQ, "74163: interpreted count/load/clear sequence");
        tf::checkEq(ri, goldenR, "74163: interpreted RCO sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("74163: native ") + modeName[m] + " built")) continue;
                std::vector<int> qn, rn; runCounterOnEngine(nat, seq, SETTLE, qn, rn);
                tf::checkEq(qn, goldenQ, std::string("74163: native ") + modeName[m] + " count sequence");
                tf::checkEq(rn, goldenR, std::string("74163: native ") + modeName[m] + " RCO sequence");
        }
}

struct PcOp { int d; int nload; int nclr; int ce; };
struct PcStep { int d; int nload; int nclr; int ce; int clk; };

static void runPcOnEngine(Part& p, const std::vector<PcStep>& seq, int settle, std::vector<int>& qs)
{
        for (const PcStep& st : seq)
        {
                int q = 0;
                for (int t = 0; t < settle; ++t)
                {
                        std::vector<int> in(20, 0);
                        for (int k = 0; k < 16; ++k) in[k] = (st.d >> k) & 1;
                        in[16] = st.nload; in[17] = st.nclr; in[18] = st.ce; in[19] = st.clk;
                        std::vector<int> o = toBits(p(toStates(in)));
                        q = 0;
                        for (int k = 0; k < 16; ++k) q |= (o[k] << k);
                }
                qs.push_back(q);
        }
}

static void testProgramCounter()
{
        tf::section("2.1e 6502 register file: 16-bit PC (four 74163 cascaded, load + increment)");
        const std::string NAME = "PC_16-bit_Program_Counter";
        const int SETTLE = 40;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "PC: interpreted engine loaded")) return;
        std::vector<PcOp> ops = {
                { 0, 1, 0, 0 },        // clear -> 0x0000
                { 0xFFFE, 0, 1, 0 },   // load 0xFFFE
                { 0, 1, 1, 1 },        // count -> 0xFFFF
                { 0, 1, 1, 1 },        // count -> 0x0000 (full 16-bit ripple carry)
                { 0, 1, 1, 1 },        // count -> 0x0001
                { 0x1234, 0, 1, 0 },   // load 0x1234
                { 0, 1, 1, 0 },        // hold (CE=0) -> 0x1234
                { 0, 1, 1, 1 },        // count -> 0x1235
                { 0x00FF, 0, 1, 0 },   // load 0x00FF
                { 0, 1, 1, 1 },        // count -> 0x0100 (byte boundary ripple)
                { 0, 1, 0, 0 }         // clear -> 0x0000
        };
        std::vector<PcStep> seq;
        for (const PcOp& o : ops)
        {
                seq.push_back({ o.d, o.nload, o.nclr, o.ce, 0 });
                seq.push_back({ o.d, o.nload, o.nclr, o.ce, 1 });
        }
        std::vector<int> golden;
        {
                int q = 0, prev = 0;
                for (const PcStep& st : seq)
                {
                        if (st.clk && !prev)
                        {
                                if (!st.nclr) q = 0;
                                else if (!st.nload) q = st.d;
                                else if (st.ce) q = (q + 1) & 0xFFFF;
                        }
                        prev = st.clk;
                        golden.push_back(q);
                }
        }
        std::vector<int> qi;
        runPcOnEngine(interp, seq, SETTLE, qi);
        tf::checkEq(qi, golden, "PC: interpreted clear/load/count/hold sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("PC: native ") + modeName[m] + " built")) continue;
                std::vector<int> qn;
                runPcOnEngine(nat, seq, SETTLE, qn);
                tf::checkEq(qn, golden, std::string("PC: native ") + modeName[m] + " sequence (hierarchical from 74163 x4)");
        }
}

static void testCounterAsync()
{
        tf::section("74161 4-bit synchronous counter (ASYNC clear, load, count, RCO)");
        const std::string NAME = "74161_4-bit_Synchronous_Counter_Async_Clear";
        const int SETTLE = 24;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "74161: interpreted engine loaded")) return;
        std::vector<CntStep> seq = { {0,1,0,0,0,0},{0xC,0,1,0,0,0},{0xC,0,1,0,0,1},{0,1,1,1,1,0},{0,1,1,1,1,1},{0,1,1,1,1,0},{0,1,1,1,1,1},{0,1,1,1,1,0},{0,1,1,1,1,1},{0,1,0,1,1,1},{0,1,1,1,1,1},{0,1,1,1,1,0},{0,1,1,1,1,1} };
        std::vector<int> goldenQ, goldenR;
        {
                int q = 0, prev = 0;
                for (const CntStep& st : seq)
                {
                        if (!st.nclr) q = 0;
                        else if (st.clk && !prev) { if (!st.nload) q = st.d; else if (st.enp && st.ent) q = (q + 1) & 15; }
                        prev = st.clk;
                        goldenQ.push_back(q); goldenR.push_back((st.ent && q == 15) ? 1 : 0);
                }
        }
        std::vector<int> qi, ri; runCounterOnEngine(interp, seq, SETTLE, qi, ri);
        tf::checkEq(qi, goldenQ, "74161: interpreted async-clear count/load sequence");
        tf::checkEq(ri, goldenR, "74161: interpreted RCO sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("74161: native ") + modeName[m] + " built")) continue;
                std::vector<int> qn, rn; runCounterOnEngine(nat, seq, SETTLE, qn, rn);
                tf::checkEq(qn, goldenQ, std::string("74161: native ") + modeName[m] + " count sequence");
                tf::checkEq(rn, goldenR, std::string("74161: native ") + modeName[m] + " RCO sequence");
        }
}

struct RamStep { int a; int din; int we; int clk; };

static std::vector<int> runRamOnEngine(Part& p, const std::vector<RamStep>& seq, int settle)
{
        std::vector<int> outs;
        for (const RamStep& st : seq)
        {
                int d = 0;
                for (int t = 0; t < settle; ++t)
                {
                        std::vector<int> in(8, 0);
                        in[0] = st.a & 1; in[1] = (st.a >> 1) & 1;
                        for (int b = 0; b < 4; ++b) in[2 + b] = (st.din >> b) & 1;
                        in[6] = st.we; in[7] = st.clk;
                        std::vector<int> o = toBits(p(toStates(in)));
                        d = 0; for (int b = 0; b < 4; ++b) d |= (o[b] << b);
                }
                outs.push_back(d);
        }
        return outs;
}

static void testRamPart()
{
        tf::section("RAM 4-word x 4-bit synchronous (write-enable, async read)");
        const std::string NAME = "RAM_4word_4bit_Synchronous";
        const int SETTLE = 22;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "RAM: interpreted engine loaded")) return;
        std::vector<RamStep> seq = { {0,0x5,1,0},{0,0x5,1,1},{1,0xA,1,0},{1,0xA,1,1},{2,0x3,1,0},{2,0x3,1,1},{3,0xC,1,0},{3,0xC,1,1},{0,0,0,0},{1,0,0,0},{2,0,0,0},{3,0,0,0},{0,0xF,0,0},{0,0xF,0,1},{0,0,0,0},{1,0xF,1,0},{1,0xF,1,1},{1,0,0,0} };
        std::vector<int> golden;
        {
                int mem[4] = {0,0,0,0}, prev = 0;
                for (const RamStep& st : seq)
                {
                        if (st.clk && !prev && st.we) mem[st.a] = st.din;
                        prev = st.clk;
                        golden.push_back(mem[st.a]);
                }
        }
        std::vector<int> gi = runRamOnEngine(interp, seq, SETTLE);
        tf::checkEq(gi, golden, "RAM: interpreted write/read sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("RAM: native ") + modeName[m] + " built")) continue;
                std::vector<int> gn = runRamOnEngine(nat, seq, SETTLE);
                tf::checkEq(gn, golden, std::string("RAM: native ") + modeName[m] + " write/read sequence");
        }
}

static void testRom()
{
        tf::section("ROM primitive (16x8 lookup table, hex-loadable contents)");
        const std::string NAME = "ROM_16x8_Lookup_Table";
        int nIn = 0, nOut = 0;
        Part rom = loadLayoutAsPart("layouts/" + NAME + ".json", nIn, nOut);
        if (!tf::check(rom != nullptr, "ROM: fixture loaded")) return;
        bool ok = true;
        for (int a = 0; a < 16; ++a)
        {
                std::vector<int> in(nIn, 0);
                for (int k = 0; k < 4; ++k) in[k] = (a >> k) & 1;
                std::vector<int> o = toBits(rom(toStates(in)));
                int got = 0; for (int b = 0; b < 8; ++b) got |= (o[b] << b);
                if (got != ((0xA0 + a) & 0xFF)) ok = false;
        }
        tf::check(ok, "ROM: all 16 addresses read back their stored word (interpreted)");
        const char* romMode[2] = { "inline", "link" };
        bool romLink[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, romLink[m]);
                if (!tf::check(nat != nullptr, std::string("ROM: native ") + romMode[m] + " built")) continue;
                bool nok = true;
                for (int a = 0; a < 16; ++a)
                {
                        std::vector<int> in(nIn, 0);
                        for (int k = 0; k < 4; ++k) in[k] = (a >> k) & 1;
                        std::vector<int> o = toBits(nat(toStates(in)));
                        int got = 0; for (int b = 0; b < 8 && b < (int)o.size(); ++b) got |= (o[b] << b);
                        if (got != ((0xA0 + a) & 0xFF)) nok = false;
                }
                tf::check(nok, std::string("ROM: native ") + romMode[m] + " matches lookup table");
        }
        std::vector<uint32_t> parsed = parseHexDump("A0 A1\n0f FF  12", 8);
        std::vector<int> pv(parsed.begin(), parsed.end());
        tf::checkEq(pv, std::vector<int>{ 0xA0, 0xA1, 0x0F, 0xFF, 0x12 }, "ROM: parseHexDump reads whitespace/newline-separated hex bytes");
}

static void checkDualRom(Part& p, int nIn, const std::string& tag)
{
        const int t1[4] = { 1, 2, 4, 8 };
        const int t2[4] = { 3, 5, 7, 9 };
        bool okp = true;
        for (int a = 0; a < 4; ++a)
        {
                std::vector<int> in(nIn, 0); in[0] = a & 1; in[1] = (a >> 1) & 1;
                std::vector<int> o = toBits(p(toStates(in)));
                int r1 = 0, r2 = 0;
                for (int b = 0; b < 4; ++b) r1 |= (o[b] << b);
                for (int b = 0; b < 4; ++b) r2 |= (o[4 + b] << b);
                if (r1 != t1[a] || r2 != t2[a]) okp = false;
        }
        tf::check(okp, "dual-ROM: both ROMs keep their own table (" + tag + ")");
}

static void testRomMulti()
{
        tf::section("Multiple ROM parts in one circuit (independent contents)");
        const std::string NAME = "ROM_Dual_Independent";
        int nIn = 0, nOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", nIn, nOut);
        if (!tf::check(interp != nullptr, "dual-ROM: fixture loaded")) return;
        checkDualRom(interp, nIn, "interpreted");
        int no = 0; Part ni = buildNative(NAME, no, false); if (ni) checkDualRom(ni, nIn, "native inline");
        int no2 = 0; Part nl = buildNative(NAME, no2, true); if (nl) checkDualRom(nl, nIn, "native link");
}

static std::vector<std::vector<int> > runSequential(Part& p, const std::vector<std::vector<int> >& inputs, int settle)
{
        std::vector<std::vector<int> > outs;
        for (const std::vector<int>& in : inputs)
        {
                std::vector<int> o;
                for (int t = 0; t < settle; ++t) o = toBits(p(toStates(in)));
                outs.push_back(o);
        }
        return outs;
}

static bool seqMatches(const std::vector<std::vector<int> >& got, const std::vector<std::vector<int> >& golden)
{
        for (std::size_t s = 0; s < golden.size(); ++s)
                for (std::size_t k = 0; k < golden[s].size(); ++k)
                        if (s >= got.size() || k >= got[s].size() || got[s][k] != golden[s][k]) return false;
        return true;
}

static void checkSequential(const std::string& name, const std::vector<std::vector<int> >& inputs, const std::vector<std::vector<int> >& golden)
{
        const int SETTLE = 16;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, name + ": interpreted loaded")) return;
        tf::check(seqMatches(runSequential(interp, inputs, SETTLE), golden), name + ": interpreted sequence matches");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(name, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, name + ": native " + modeName[m] + " built")) continue;
                tf::check(seqMatches(runSequential(nat, inputs, SETTLE), golden), name + ": native " + modeName[m] + " sequence matches");
        }
}

static void testLearningCircuits()
{
        tf::section("2.1d learning circuits: adders (exhaustive, both engines)");
        testCombinational("Half_Adder", 2, [](const std::vector<int>& v){ return std::vector<int>{ v[0] ^ v[1], v[0] & v[1] }; });
        testCombinational("Full_Adder", 3, [](const std::vector<int>& v){ int su = v[0] ^ v[1] ^ v[2]; int co = (v[0] & v[1]) | (v[2] & (v[0] ^ v[1])); return std::vector<int>{ su, co }; });

        tf::section("2.1d learning circuits: SR + D latches (sequence)");
        checkSequential("SR_Latch_NOR", { {1,0},{0,0},{0,1},{0,0},{1,0} }, { {1,0},{1,0},{0,1},{0,1},{1,0} });
        checkSequential("D_Latch_Gated", { {1,1},{0,0},{0,1},{1,0},{1,1} }, { {1},{1},{0},{0},{1} });

        tf::section("2.1d learning circuits: 4-bit shift register (SIPO)");
        {
                std::vector<std::vector<int> > in, gold;
                int sr[4] = {0,0,0,0}, prev = 0;
                int din[8] = {1,1,0,0,1,1,1,1};
                int clk[8] = {0,1,0,1,0,1,0,1};
                for (int i = 0; i < 8; ++i)
                {
                        in.push_back({ din[i], clk[i] });
                        if (clk[i] && !prev) { for (int q = 3; q > 0; --q) sr[q] = sr[q - 1]; sr[0] = din[i]; }
                        prev = clk[i];
                        gold.push_back({ sr[0], sr[1], sr[2], sr[3] });
                }
                checkSequential("Shift_Register_4-bit_SIPO", in, gold);
        }

        tf::section("2.1d learning circuits: traffic-light FSM (Green->Yellow->Red cycle)");
        {
                std::vector<std::vector<int> > in, gold;
                int stt = 0, prev = 0;
                int clk[9] = {0,0,0,1,0,1,0,1,0};
                int rst[9] = {0,1,1,1,1,1,1,1,1};
                for (int i = 0; i < 9; ++i)
                {
                        in.push_back({ clk[i], rst[i] });
                        if (!rst[i]) stt = 0; else if (clk[i] && !prev) stt = (stt == 2) ? 0 : stt + 1;
                        prev = clk[i];
                        gold.push_back({ stt == 0 ? 1 : 0, stt == 1 ? 1 : 0, stt == 2 ? 1 : 0 });
                }
                checkSequential("Traffic_Light_Controller_FSM", in, gold);
        }
}

int main()
{
        std::printf("%s%sSulla validation suite%s  (interpreted + native engines)\n",
                    tf::ansiBold(), tf::ansiCyan(), tf::ansiRst());

        auto B  = [](int x){ return x; };
        (void)B;

        testCombinational("and2",  2, [](const std::vector<int>& v){ return std::vector<int>{ v[0] & v[1] }; });
        testCombinational("or2",   2, [](const std::vector<int>& v){ return std::vector<int>{ v[0] | v[1] }; });
        testCombinational("not1",  1, [](const std::vector<int>& v){ return std::vector<int>{ v[0] ? 0 : 1 }; });
        testCombinational("nand2", 2, [](const std::vector<int>& v){ return std::vector<int>{ (v[0] & v[1]) ? 0 : 1 }; });
        testCombinational("nor2",  2, [](const std::vector<int>& v){ return std::vector<int>{ (v[0] | v[1]) ? 0 : 1 }; });
        testCombinational("xor2",  2, [](const std::vector<int>& v){ return std::vector<int>{ v[0] ^ v[1] }; });
        testCombinational("xnor2", 2, [](const std::vector<int>& v){ return std::vector<int>{ (v[0] ^ v[1]) ? 0 : 1 }; });

        testCombinational("7400_Quad_2-input_NAND_Gates", 8, [](const std::vector<int>& v){ return std::vector<int>{ (v[0]&v[1])?0:1, (v[2]&v[3])?0:1, (v[4]&v[5])?0:1, (v[6]&v[7])?0:1 }; });
        testCombinational("7402_Quad_2-input_NOR_Gates", 8, [](const std::vector<int>& v){ return std::vector<int>{ (v[0]|v[1])?0:1, (v[2]|v[3])?0:1, (v[4]|v[5])?0:1, (v[6]|v[7])?0:1 }; });
        testCombinational("7404_Hex_Inverters", 6, [](const std::vector<int>& v){ return std::vector<int>{ v[0]?0:1, v[1]?0:1, v[2]?0:1, v[3]?0:1, v[4]?0:1, v[5]?0:1 }; });
        testCombinational("7408_Quad_2-input_AND_Gates", 8, [](const std::vector<int>& v){ return std::vector<int>{ v[0]&v[1], v[2]&v[3], v[4]&v[5], v[6]&v[7] }; });
        testCombinational("7432_Quad_2-input_OR_Gates", 8, [](const std::vector<int>& v){ return std::vector<int>{ v[0]|v[1], v[2]|v[3], v[4]|v[5], v[6]|v[7] }; });
        testCombinational("7486_Quad_2-input_XOR_Gates", 8, [](const std::vector<int>& v){ return std::vector<int>{ v[0]^v[1], v[2]^v[3], v[4]^v[5], v[6]^v[7] }; });

        testCombinational("74283_4-bit_Binary_Full_Adder", 9, [](const std::vector<int>& v){ int A = v[0] | v[1] << 1 | v[2] << 2 | v[3] << 3; int B = v[4] | v[5] << 1 | v[6] << 2 | v[7] << 3; int s = A + B + v[8]; return std::vector<int>{ s & 1, (s >> 1) & 1, (s >> 2) & 1, (s >> 3) & 1, (s >> 4) & 1 }; });
        testCombinational("74139_Dual_2-to-4_Line_Decoder", 6, [](const std::vector<int>& v){ std::vector<int> r(8); for (int d = 0; d < 2; ++d) { int en = !v[d * 3 + 2]; int sel = v[d * 3] | v[d * 3 + 1] << 1; for (int k = 0; k < 4; ++k) r[d * 4 + k] = (en && sel == k) ? 0 : 1; } return r; });
        testCombinational("74157_Quad_2-to-1_Multiplexer", 10, [](const std::vector<int>& v){ int S = v[8]; int en = !v[9]; std::vector<int> r(4); for (int i = 0; i < 4; ++i) r[i] = en ? (S ? v[4 + i] : v[i]) : 0; return r; });
        testCombinational("74153_Dual_4-to-1_Multiplexer", 12, [](const std::vector<int>& v){ int sel = v[8] | v[9] << 1; std::vector<int> r(2); for (int d = 0; d < 2; ++d) { int en = !v[10 + d]; r[d] = en ? v[d * 4 + sel] : 0; } return r; });

        testCombinational("and3",  3, [](const std::vector<int>& v){ return std::vector<int>{ v[0] & v[1] & v[2] }; });
        testCombinational("xor4",  4, [](const std::vector<int>& v){ return std::vector<int>{ v[0] ^ v[1] ^ v[2] ^ v[3] }; });

        testCombinational("passthrough", 1, [](const std::vector<int>& v){ return std::vector<int>{ v[0] }; });
        testCombinational("fanout",       1, [](const std::vector<int>& v){ return std::vector<int>{ v[0], v[0] ? 0 : 1 }; });

        testCombinational("half_adder", 2, [](const std::vector<int>& v){
                return std::vector<int>{ v[0] ^ v[1], v[0] & v[1] }; });
        testCombinational("full_adder", 3, [](const std::vector<int>& v){
                int s = v[0] ^ v[1] ^ v[2];
                int c = (v[0] & v[1]) | (v[2] & (v[0] ^ v[1]));
                return std::vector<int>{ s, c }; });
        testCombinational("mux2", 3, [](const std::vector<int>& v){
                int d0 = v[0], d1 = v[1], sel = v[2];
                return std::vector<int>{ sel ? d1 : d0 }; });

        testCombinational("adder2", 4, [](const std::vector<int>& v){
                int A = v[0] | (v[1] << 1);
                int B = v[2] | (v[3] << 1);
                int sum = A + B;
                return std::vector<int>{ sum & 1, (sum >> 1) & 1, (sum >> 2) & 1 }; });

        testSrLatch();
        testClock();

        testNativeLink();
        testPinOrderConsistency();
        testPerInstanceState();
        testEdgeRegister();
        testEnableRegister();
        testCounter();
        testCounterAsync();
        testProgramCounter();
        testRamPart();
        testRom();
        testRomMulti();
        testLearningCircuits();




        cleanupModules();
        return tf::summary();
}
