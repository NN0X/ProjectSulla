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
        tf::section("16-bit program counter (four 74163 cascaded, load + increment)");
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
        tf::section("learning circuits: adders (exhaustive, both engines)");
        testCombinational("Half_Adder", 2, [](const std::vector<int>& v){ return std::vector<int>{ v[0] ^ v[1], v[0] & v[1] }; });
        testCombinational("Full_Adder", 3, [](const std::vector<int>& v){ int su = v[0] ^ v[1] ^ v[2]; int co = (v[0] & v[1]) | (v[2] & (v[0] ^ v[1])); return std::vector<int>{ su, co }; });

        tf::section("learning circuits: SR + D latches (sequence)");
        checkSequential("SR_Latch_NOR", { {1,0},{0,0},{0,1},{0,0},{1,0} }, { {1,0},{1,0},{0,1},{0,1},{1,0} });
        checkSequential("D_Latch_Gated", { {1,1},{0,0},{0,1},{1,0},{1,1} }, { {1},{1},{0},{0},{1} });

        tf::section("learning circuits: 4-bit shift register (SIPO)");
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

        tf::section("learning circuits: traffic-light FSM (Green->Yellow->Red cycle)");
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

static void testAlu8()
{
        tf::section("74181 x2 8-bit ALU (two 4-bit slices, ripple carry): interpreted, native inline, native link");
        const std::string NAME = "74181x2_8-bit_ALU";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "8-bit ALU: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("8-bit ALU: native ") + modeName[m] + " built");
        }
        unsigned seed = 2654435761u;
        bool okI = true, okN[2] = { true, true };
        for (int t = 0; t < 60000; ++t)
        {
                int v[22];
                for (int k = 0; k < 22; ++k) { seed = seed * 1103515245u + 12345u; v[k] = (seed >> 16) & 1; }
                int A[8], B[8], Sb[4];
                for (int i = 0; i < 8; ++i) { A[i] = v[i]; B[i] = v[8 + i]; }
                for (int i = 0; i < 4; ++i) Sb[i] = v[16 + i];
                int M = v[20], Cn = v[21], carry = 1 - Cn, F[8];
                for (int i = 0; i < 8; ++i)
                {
                        int P = A[i] | (B[i] & Sb[0]) | ((!B[i]) & Sb[1]);
                        int G = A[i] & ((B[i] & Sb[3]) | ((!B[i]) & Sb[2]));
                        int ce = M | carry; F[i] = (P ^ G) ^ ce; carry = G | (P & carry);
                }
                int c8 = 1 - carry, ab = 1;
                for (int i = 0; i < 8; ++i) ab &= F[i];
                std::vector<int> in(22);
                for (int k = 0; k < 22; ++k) in[k] = v[k];
                std::vector<int> oi = toBits(interp(toStates(in)));
                for (int i = 0; i < 8; ++i) if (oi[i] != F[i]) okI = false;
                if (oi[8] != c8 || oi[9] != ab) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> on = toBits(nat[m](toStates(in)));
                        for (int i = 0; i < 8; ++i) if (on[i] != F[i]) okN[m] = false;
                        if (on[8] != c8 || on[9] != ab) okN[m] = false;
                }
        }
        tf::check(okI, "8-bit ALU: interpreted matches golden (60000 random)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("8-bit ALU: native ") + modeName[m] + " matches golden (60000 random)");
}


struct PsOp { int fv; int lv; };
struct PsStep { int fv; int lv; int clk; };

static void runPsOnEngine(Part& p, const std::vector<PsStep>& seq, int settle, std::vector<int>& qs)
{
        for (const PsStep& st : seq)
        {
                int q = 0;
                for (int t = 0; t < settle; ++t)
                {
                        std::vector<int> in(17, 0);
                        for (int k = 0; k < 8; ++k) { in[k] = (st.fv >> k) & 1; in[8 + k] = (st.lv >> k) & 1; }
                        in[16] = st.clk;
                        std::vector<int> o = toBits(p(toStates(in)));
                        q = 0;
                        for (int k = 0; k < 8; ++k) q |= (o[k] << k);
                }
                qs.push_back(q);
        }
}

static void testPStatusRegister()
{
        tf::section("6502 P status register (8-bit, per-bit load enable; 74377 + hold/load muxes)");
        const std::string NAME = "6502_P_Status_Register";
        const int SETTLE = 40;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "P register: interpreted loaded")) return;
        std::vector<PsOp> ops = {
                { 0xA5, 0xFF },   // load all -> 0xA5
                { 0x50, 0x0F },   // update low nibble only -> 0xA0
                { 0x30, 0xF0 },   // update high nibble only -> 0x30
                { 0xFF, 0x00 },   // hold all -> 0x30
                { 0x01, 0x01 },   // set bit 0 (C) only -> 0x31
                { 0x00, 0x80 },   // clear bit 7 (N; already 0) -> 0x31
                { 0x00, 0xFF }    // clear all -> 0x00
        };
        std::vector<PsStep> seq;
        for (const PsOp& o : ops)
        {
                seq.push_back({ o.fv, o.lv, 0 });
                seq.push_back({ o.fv, o.lv, 1 });
        }
        std::vector<int> golden;
        {
                int q = 0, prev = 0;
                for (const PsStep& st : seq)
                {
                        if (st.clk && !prev)
                                for (int k = 0; k < 8; ++k)
                                        if ((st.lv >> k) & 1) { q = (q & ~(1 << k)) | (((st.fv >> k) & 1) << k); }
                        prev = st.clk;
                        golden.push_back(q);
                }
        }
        std::vector<int> qi;
        runPsOnEngine(interp, seq, SETTLE, qi);
        tf::checkEq(qi, golden, "P register: interpreted per-bit load sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("P register: native ") + modeName[m] + " built")) continue;
                std::vector<int> qn;
                runPsOnEngine(nat, seq, SETTLE, qn);
                tf::checkEq(qn, golden, std::string("P register: native ") + modeName[m] + " sequence");
        }
}


struct RfOp { int wd; int ws; int we; int rs; };

static void runRfOnEngine(Part& p, const std::vector<RfOp>& ops, int settle, std::vector<int>& reads)
{
        for (const RfOp& op : ops)
        {
                int rd = 0;
                for (int clk = 0; clk < 2; ++clk)
                {
                        for (int t = 0; t < settle; ++t)
                        {
                                std::vector<int> in(14, 0);
                                for (int k = 0; k < 8; ++k) in[k] = (op.wd >> k) & 1;
                                in[8] = op.ws & 1; in[9] = (op.ws >> 1) & 1; in[10] = op.we;
                                in[11] = op.rs & 1; in[12] = (op.rs >> 1) & 1; in[13] = clk;
                                std::vector<int> o = toBits(p(toStates(in)));
                                rd = 0;
                                for (int k = 0; k < 8; ++k) rd |= (o[k] << k);
                        }
                }
                reads.push_back(rd);
        }
}

static void testRegisterFile()
{
        tf::section("6502 register file A/X/Y/SP (4x 74377 + 74139 write decode + 74153 read mux)");
        const std::string NAME = "6502_Register_File";
        const int SETTLE = 40;
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "register file: interpreted loaded")) return;
        std::vector<RfOp> ops = {
                { 0x11, 0, 1, 0 },   // write A, read A   -> 0x11
                { 0x22, 1, 1, 1 },   // write X, read X   -> 0x22
                { 0x33, 2, 1, 2 },   // write Y, read Y   -> 0x33
                { 0x44, 3, 1, 3 },   // write SP, read SP -> 0x44
                { 0x00, 0, 0, 0 },   // WE low, read A    -> 0x11 (held)
                { 0x55, 0, 1, 0 },   // overwrite A       -> 0x55
                { 0xFF, 2, 0, 1 },   // WE low, read X    -> 0x22 (held)
                { 0x00, 0, 0, 3 }    // read SP           -> 0x44
        };
        std::vector<int> golden;
        {
                int state[4] = { 0, 0, 0, 0 };
                for (const RfOp& op : ops)
                {
                        if (op.we) state[op.ws] = op.wd;
                        golden.push_back(state[op.rs]);
                }
        }
        std::vector<int> ri;
        runRfOnEngine(interp, ops, SETTLE, ri);
        tf::checkEq(ri, golden, "register file: interpreted write/read/hold sequence");
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("register file: native ") + modeName[m] + " built")) continue;
                std::vector<int> rn;
                runRfOnEngine(nat, ops, SETTLE, rn);
                tf::checkEq(rn, golden, std::string("register file: native ") + modeName[m] + " sequence");
        }
}

static void testAluDecodeEndToEnd()
{
        tf::section("6502 ALU decode end-to-end: opcode -> decoder -> 74181 ALU -> operation result");
        int di = 0, dou = 0;
        Part dec = loadLayoutAsPart("layouts/6502_ALU_Control_Decoder.json", di, dou);
        int ai = 0, ao = 0;
        Part alu = loadLayoutAsPart("layouts/74181x2_8-bit_ALU.json", ai, ao);
        if (!tf::check(dec != nullptr && alu != nullptr, "decode e2e: decoder + ALU loaded")) return;
        int aaas[6] = { 0, 1, 2, 3, 6, 7 };
        unsigned seed = 123457u;
        bool ok = true;
        for (int oi = 0; oi < 6 && ok; ++oi)
        {
                int aaa = aaas[oi];
                int opcode = (aaa << 5) | 0x01;
                std::vector<int> ov(8);
                for (int k = 0; k < 8; ++k) ov[k] = (opcode >> k) & 1;
                std::vector<int> dout = toBits(dec(toStates(ov)));
                int S0 = dout[0], S1 = dout[1], S2 = dout[2], S3 = dout[3], M = dout[4], isalu = dout[5];
                if (!isalu) { ok = false; break; }
                int Cn = (aaa == 3) ? 1 : (aaa == 6 || aaa == 7) ? 0 : 1;
                for (int t = 0; t < 400 && ok; ++t)
                {
                        seed = seed * 1103515245u + 12345u; int A = (seed >> 16) & 0xFF;
                        seed = seed * 1103515245u + 12345u; int B = (seed >> 16) & 0xFF;
                        std::vector<int> in(22, 0);
                        for (int k = 0; k < 8; ++k) { in[k] = (A >> k) & 1; in[8 + k] = (B >> k) & 1; }
                        in[16] = S0; in[17] = S1; in[18] = S2; in[19] = S3; in[20] = M; in[21] = Cn;
                        std::vector<int> f = toBits(alu(toStates(in)));
                        int got = 0;
                        for (int k = 0; k < 8; ++k) got |= (f[k] << k);
                        int exp = 0;
                        switch (aaa)
                        {
                                case 0: exp = (A | B) & 0xFF; break;
                                case 1: exp = (A & B) & 0xFF; break;
                                case 2: exp = (A ^ B) & 0xFF; break;
                                case 3: exp = (A + B) & 0xFF; break;
                                case 6: case 7: exp = (A - B) & 0xFF; break;
                                default: exp = 0; break;
                        }
                        if (got != exp) ok = false;
                }
        }
        tf::check(ok, "decode e2e: decoded control drives the ALU to the 6502 operation result");
}

static void testFlagDecodeEndToEnd()
{
        tf::section("6502 flag decode end-to-end: opcode -> flag decoder -> P status register");
        int di = 0, dou = 0;
        Part dec = loadLayoutAsPart("layouts/6502_Flag_Op_Decoder.json", di, dou);
        int pi = 0, po = 0;
        Part preg = loadLayoutAsPart("layouts/6502_P_Status_Register.json", pi, po);
        if (!tf::check(dec != nullptr && preg != nullptr, "flag e2e: decoder + P register loaded")) return;
        const int SETTLE = 40;
        struct Fl { int F; int L; };
        std::vector<Fl> steps;
        steps.push_back({ 0x00, 0xFF });                 // setup: clear all flags
        int flagOps[] = { 0x38, 0xF8, 0x78, 0x18, 0xD8, 0x58 };   // SEC SED SEI CLC CLD CLI
        for (int oc : flagOps)
        {
                std::vector<int> ov(8);
                for (int k = 0; k < 8; ++k) ov[k] = (oc >> k) & 1;
                std::vector<int> dd = toBits(dec(toStates(ov)));
                int F = 0, L = 0;
                for (int k = 0; k < 8; ++k) { F |= dd[k] << k; L |= dd[8 + k] << k; }
                steps.push_back({ F, L });
        }
        steps.push_back({ 0x40, 0xFF });                 // setup: set V (bit 6)
        {
                std::vector<int> ov(8);
                for (int k = 0; k < 8; ++k) ov[k] = (0xB8 >> k) & 1;   // CLV
                std::vector<int> dd = toBits(dec(toStates(ov)));
                int F = 0, L = 0;
                for (int k = 0; k < 8; ++k) { F |= dd[k] << k; L |= dd[8 + k] << k; }
                steps.push_back({ F, L });
        }
        std::vector<int> golden;
        {
                int P = 0;
                for (const Fl& st : steps)
                {
                        for (int k = 0; k < 8; ++k) if ((st.L >> k) & 1) P = (P & ~(1 << k)) | (((st.F >> k) & 1) << k);
                        golden.push_back(P);
                }
        }
        std::vector<int> reads;
        for (const Fl& st : steps)
        {
                int P = 0;
                for (int clk = 0; clk < 2; ++clk)
                {
                        for (int t = 0; t < SETTLE; ++t)
                        {
                                std::vector<int> in(17, 0);
                                for (int k = 0; k < 8; ++k) { in[k] = (st.F >> k) & 1; in[8 + k] = (st.L >> k) & 1; }
                                in[16] = clk;
                                std::vector<int> o = toBits(preg(toStates(in)));
                                P = 0;
                                for (int k = 0; k < 8; ++k) P |= o[k] << k;
                        }
                }
                reads.push_back(P);
        }
        tf::checkEq(reads, golden, "flag e2e: each flag instruction updates its own P bit, others held");
}

static void testTransferDecodeEndToEnd()
{
        tf::section("6502 transfer decode end-to-end: opcode -> transfer decoder -> register file");
        int di = 0, dou = 0;
        Part dec = loadLayoutAsPart("layouts/6502_Transfer_Decoder.json", di, dou);
        int ri = 0, ro = 0;
        Part rf = loadLayoutAsPart("layouts/6502_Register_File.json", ri, ro);
        if (!tf::check(dec != nullptr && rf != nullptr, "transfer e2e: decoder + register file loaded")) return;
        struct Xfer { int opcode; int knownSrc; int knownDst; };
        Xfer xfers[6] = {
                { 0xAA, 0, 1 }, { 0x8A, 1, 0 }, { 0xA8, 0, 2 },
                { 0x98, 2, 0 }, { 0xBA, 3, 1 }, { 0x9A, 1, 3 }
        };
        int setupv[4] = { 0x11, 0x22, 0x33, 0x44 };
        bool ok = true;
        for (int i = 0; i < 6 && ok; ++i)
        {
                std::vector<int> ov(8);
                for (int k = 0; k < 8; ++k) ov[k] = (xfers[i].opcode >> k) & 1;
                std::vector<int> dd = toBits(dec(toStates(ov)));
                int rs = dd[0] | (dd[1] << 1);
                int ws = dd[2] | (dd[3] << 1);
                int we = dd[4];
                std::vector<RfOp> ops;
                for (int r = 0; r < 4; ++r) ops.push_back({ setupv[r], r, 1, r });   // fresh setup
                ops.push_back({ setupv[rs], ws, we, xfers[i].knownDst });             // move source value to decoded dest, read the known dest
                std::vector<int> reads;
                runRfOnEngine(rf, ops, 40, reads);
                if (reads.back() != setupv[xfers[i].knownSrc]) ok = false;
        }
        tf::check(ok, "transfer e2e: each transfer moves the source register to the destination register");
}

static void testFlagLogicEndToEnd()
{
        tf::section("6502 flag logic end-to-end: real ALU addition result -> N/Z/C/V");
        int fi = 0, fo = 0;
        Part fl = loadLayoutAsPart("layouts/6502_ALU_Flag_Logic.json", fi, fo);
        int ai = 0, ao = 0;
        Part alu = loadLayoutAsPart("layouts/74181x2_8-bit_ALU.json", ai, ao);
        if (!tf::check(fl != nullptr && alu != nullptr, "flag logic e2e: flag logic + ALU loaded")) return;
        int A[8] = { 0x50, 0x50, 0xD0, 0x00, 0x80, 0x3F, 0xFF, 0x01 };
        int B[8] = { 0x50, 0xD0, 0xD0, 0x00, 0x80, 0x01, 0x01, 0x02 };
        bool ok = true;
        for (int i = 0; i < 8 && ok; ++i)
        {
                int a = A[i], b = B[i];
                std::vector<int> in(22, 0);
                for (int k = 0; k < 8; ++k) { in[k] = (a >> k) & 1; in[8 + k] = (b >> k) & 1; }
                in[16] = 1; in[17] = 0; in[18] = 0; in[19] = 1; in[20] = 0; in[21] = 1;   // ADD: S=1001, M=0, Cn=1
                std::vector<int> fbits = toBits(alu(toStates(in)));
                int R = 0;
                for (int k = 0; k < 8; ++k) R |= fbits[k] << k;
                int Cout = 1 - fbits[8];
                std::vector<int> fin(12, 0);
                for (int k = 0; k < 8; ++k) fin[k] = fbits[k];
                fin[8] = (a >> 7) & 1; fin[9] = (b >> 7) & 1; fin[10] = Cout; fin[11] = 0;
                std::vector<int> flags = toBits(fl(toStates(fin)));
                int N = flags[0], Z = flags[1], Cf = flags[2], V = flags[3];
                int sum = a + b, eR = sum & 0xFF;
                int eN = (eR >> 7) & 1, eZ = (eR == 0) ? 1 : 0, eC = (sum >= 256) ? 1 : 0;
                int a7 = (a >> 7) & 1, b7 = (b >> 7) & 1, r7 = (eR >> 7) & 1;
                int eV = ((a7 && b7 && !r7) || (!a7 && !b7 && r7)) ? 1 : 0;
                if (R != eR || N != eN || Z != eZ || Cf != eC || V != eV) ok = false;
        }
        tf::check(ok, "flag logic e2e: N/Z/C/V from real ALU sums match the 6502 flag semantics");
}

static void testShifter()
{
        tf::section("6502 single-bit shifter (ASL/LSR/ROL/ROR core): interp, native inline, native link");
        const std::string NAME = "6502_Shifter";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "shifter: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("shifter: native ") + modeName[m] + " built");
        }
        bool okI = true, okN[2] = { true, true };
        for (int D = 0; D < 256; ++D)
        for (int cin = 0; cin < 2; ++cin)
        for (int dir = 0; dir < 2; ++dir)
        for (int rot = 0; rot < 2; ++rot)
        {
                std::vector<int> in(11, 0);
                for (int k = 0; k < 8; ++k) in[k] = (D >> k) & 1;
                in[8] = cin; in[9] = dir; in[10] = rot;
                int inbit = rot ? cin : 0;
                int R, Cout;
                if (dir == 0) { R = ((D << 1) | inbit) & 0xFF; Cout = (D >> 7) & 1; }
                else { R = (D >> 1) | (inbit << 7); Cout = D & 1; }
                std::vector<int> oi = toBits(interp(toStates(in)));
                int ro = 0; for (int k = 0; k < 8; ++k) ro |= oi[k] << k;
                if (ro != R || oi[8] != Cout) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> on = toBits(nat[m](toStates(in)));
                        int rn = 0; for (int k = 0; k < 8; ++k) rn |= on[k] << k;
                        if (rn != R || on[8] != Cout) okN[m] = false;
                }
        }
        tf::check(okI, "shifter: interpreted matches golden (2048 exhaustive: left/right x shift/rotate)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("shifter: native ") + modeName[m] + " matches golden");
}

static void testIncDec()
{
        tf::section("6502 increment/decrement (INC/DEC/INX/DEX/INY/DEY core): interp, native inline, native link");
        const std::string NAME = "6502_IncDec";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "incdec: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("incdec: native ") + modeName[m] + " built");
        }
        bool okI = true, okN[2] = { true, true };
        for (int D = 0; D < 256; ++D)
        for (int dec = 0; dec < 2; ++dec)
        {
                std::vector<int> in(9, 0);
                for (int k = 0; k < 8; ++k) in[k] = (D >> k) & 1;
                in[8] = dec;
                int R = dec ? ((D + 0xFF) & 0xFF) : ((D + 1) & 0xFF);
                int eN = (R >> 7) & 1, eZ = (R == 0) ? 1 : 0;
                std::vector<int> oi = toBits(interp(toStates(in)));
                int ro = 0; for (int k = 0; k < 8; ++k) ro |= oi[k] << k;
                if (ro != R || oi[8] != eN || oi[9] != eZ) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> on = toBits(nat[m](toStates(in)));
                        int rn = 0; for (int k = 0; k < 8; ++k) rn |= on[k] << k;
                        if (rn != R || on[8] != eN || on[9] != eZ) okN[m] = false;
                }
        }
        tf::check(okI, "incdec: interpreted matches golden (512 exhaustive: inc + dec, N/Z)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("incdec: native ") + modeName[m] + " matches golden");
}

static void testCompareBit()
{
        tf::section("6502 compare + BIT flags (CPX/CPY subtract, BIT test): interp, native inline, native link");
        const std::string NAME = "6502_Compare_BIT";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "compare/BIT: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("compare/BIT: native ") + modeName[m] + " built");
        }
        int opcodes[4] = { 0xE0, 0xC0, 0x24, 0xEA };   // CPX CPY BIT NOP(pass-through)
        unsigned seed = 271828u;
        bool okI = true, okN[2] = { true, true };
        for (int t = 0; t < 4000; ++t)
        {
                seed = seed * 1103515245u + 12345u; int opcode = opcodes[(seed >> 16) & 3];
                seed = seed * 1103515245u + 12345u; int R = (seed >> 16) & 0xFF;
                seed = seed * 1103515245u + 12345u; int M = (seed >> 16) & 0xFF;
                seed = seed * 1103515245u + 12345u; int fl = (seed >> 16) & 0xF;
                int Nin = fl & 1, Zin = (fl >> 1) & 1, Cin = (fl >> 2) & 1, Vin = (fl >> 3) & 1;
                std::vector<int> in(28, 0);
                for (int k = 0; k < 8; ++k) { in[k] = (opcode >> k) & 1; in[8 + k] = (R >> k) & 1; in[16 + k] = (M >> k) & 1; }
                in[24] = Nin; in[25] = Zin; in[26] = Cin; in[27] = Vin;
                bool cc00 = ((opcode & 1) == 0) && (((opcode >> 1) & 1) == 0);
                bool isCompare = cc00 && ((opcode >> 7) & 1) && ((opcode >> 6) & 1);
                bool isBit = cc00 && !((opcode >> 7) & 1) && !((opcode >> 6) & 1) && ((opcode >> 5) & 1);
                int diff = R + (M ^ 0xFF) + 1;
                int Fsub = diff & 0xFF, Csub = (diff >= 256) ? 1 : 0;
                int Nsub = (Fsub >> 7) & 1, Zsub = (Fsub == 0) ? 1 : 0;
                int Zbit = ((R & M) == 0) ? 1 : 0, M7 = (M >> 7) & 1, M6 = (M >> 6) & 1;
                int eN = isCompare ? Nsub : (isBit ? M7 : Nin);
                int eZ = isCompare ? Zsub : (isBit ? Zbit : Zin);
                int eC = isCompare ? Csub : Cin;
                int eV = isBit ? M6 : Vin;
                std::vector<int> oi = toBits(interp(toStates(in)));
                if (oi[0] != eN || oi[1] != eZ || oi[2] != eC || oi[3] != eV) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> on = toBits(nat[m](toStates(in)));
                        if (on[0] != eN || on[1] != eZ || on[2] != eC || on[3] != eV) okN[m] = false;
                }
        }
        tf::check(okI, "compare/BIT: interpreted matches 6502 golden (CPX/CPY/BIT + pass-through, 4000 random)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("compare/BIT: native ") + modeName[m] + " matches 6502 golden");
}

static void testBranchCondition()
{
        tf::section("6502 branch condition (BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ taken logic): interp, native inline, native link");
        const std::string NAME = "6502_Branch_Condition";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "branch: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("branch: native ") + modeName[m] + " built");
        }
        bool okI = true, okN[2] = { true, true };
        for (int opcode = 0; opcode < 256; ++opcode)
        for (int fl = 0; fl < 16; ++fl)
        {
                int N = fl & 1, Z = (fl >> 1) & 1, Cc = (fl >> 2) & 1, V = (fl >> 3) & 1;
                std::vector<int> in(12, 0);
                for (int k = 0; k < 8; ++k) in[k] = (opcode >> k) & 1;
                in[8] = N; in[9] = Z; in[10] = Cc; in[11] = V;
                bool isBranch = (opcode & 0x1F) == 0x10;
                int sel = (opcode >> 6) & 3;
                int flag = (sel == 0) ? N : (sel == 1) ? V : (sel == 2) ? Cc : Z;
                int want = (flag == ((opcode >> 5) & 1)) ? 1 : 0;
                int eT = (isBranch && want) ? 1 : 0;
                std::vector<int> oi = toBits(interp(toStates(in)));
                if (oi[0] != eT) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> on = toBits(nat[m](toStates(in)));
                        if (on[0] != eT) okN[m] = false;
                }
        }
        tf::check(okI, "branch: interpreted matches 6502 golden (all 256 opcodes x 16 flag states)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("branch: native ") + modeName[m] + " matches 6502 golden");
}

struct CcStep { int rst; int done; };

static void runCycleOnEngine(Part& p, const std::vector<CcStep>& seq, int settleSteps, std::vector<int>& tOut, std::vector<int>& fOut)
{
        std::vector<State> out;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                for (int clk = 0; clk < 2; ++clk)
                {
                        std::vector<int> in(3, 0);
                        in[0] = seq[i].rst; in[1] = seq[i].done; in[2] = clk;
                        std::vector<State> sin = toStates(in);
                        for (int t = 0; t < settleSteps; ++t) out = p(sin);
                }
                std::vector<int> b = toBits(out);
                tOut.push_back(b[0] | (b[1] << 1) | (b[2] << 2));
                fOut.push_back(b[3]);
        }
}

static void testCycleCounter()
{
        tf::section("6502 cycle counter (instruction T-state timing): interp, native inline, native link");
        const std::string NAME = "6502_Cycle_Counter";
        const int SETTLE = 40;
        std::vector<CcStep> seq = {
                { 1, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
                { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 }, { 1, 0 } };

        std::vector<int> gT, gF;
        int state = 0;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                int rd = (seq[i].rst || seq[i].done) ? 1 : 0;
                state = rd ? 0 : ((state + 1) & 7);
                gT.push_back(state);
                gF.push_back(state == 0 ? 1 : 0);
        }

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "cycle: interpreted loaded")) return;
        std::vector<int> iT, iF;
        runCycleOnEngine(interp, seq, SETTLE, iT, iF);
        int gfI = 0;
        for (size_t i = 0; i < seq.size(); ++i) if (iT[i] != gT[i] || iF[i] != gF[i]) gfI++;
        tf::check(gfI == 0, "cycle: interpreted matches golden T-state sequence (reset, wrap, done-restart)");

        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("cycle: native ") + modeName[m] + " built")) continue;
                std::vector<int> nT, nF;
                runCycleOnEngine(nat, seq, SETTLE, nT, nF);
                int gfN = 0, df = 0;
                for (size_t i = 0; i < seq.size(); ++i)
                {
                        if (nT[i] != gT[i] || nF[i] != gF[i]) gfN++;
                        if (nT[i] != iT[i] || nF[i] != iF[i]) df++;
                }
                tf::check(gfN == 0, std::string("cycle: native ") + modeName[m] + " matches golden");
                tf::check(df == 0, std::string("cycle: interpreted == native ") + modeName[m]);
        }
}

struct FuStep { int db; int rst; int done; };

static void runFetchOnEngine(Part& p, const std::vector<FuStep>& seq, int settleSteps,
                             std::vector<int>& irOut, std::vector<int>& tOut, std::vector<int>& fOut)
{
        std::vector<State> out;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                for (int clk = 0; clk < 2; ++clk)
                {
                        std::vector<int> in(11, 0);
                        for (int k = 0; k < 8; ++k) in[k] = (seq[i].db >> k) & 1;
                        in[8] = seq[i].rst; in[9] = seq[i].done; in[10] = clk;
                        std::vector<State> sin = toStates(in);
                        for (int t = 0; t < settleSteps; ++t) out = p(sin);
                }
                std::vector<int> b = toBits(out);
                int ir = 0; for (int k = 0; k < 8; ++k) ir |= b[k] << k;
                irOut.push_back(ir);
                tOut.push_back(b[8] | (b[9] << 1) | (b[10] << 2));
                fOut.push_back(b[11]);
        }
}

static void testFetchUnit()
{
        tf::section("6502 fetch unit (cycle counter + instruction register): interp, native inline, native link");
        const std::string NAME = "6502_Fetch_Unit";
        const int SETTLE = 60;
        std::vector<FuStep> seq = {
                { 0xFF, 1, 0 }, { 0xA9, 0, 0 }, { 0x11, 0, 0 }, { 0x22, 0, 0 }, { 0x33, 0, 1 },
                { 0x42, 0, 0 }, { 0x55, 0, 0 }, { 0x66, 0, 1 }, { 0x77, 0, 0 }, { 0x88, 1, 0 } };

        std::vector<int> gIR, gT, gF;
        int cs = 0, ir = 0;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                int fetchDuring = (cs == 0) ? 1 : 0;
                int nc = (seq[i].rst || seq[i].done) ? 0 : ((cs + 1) & 7);
                int ni = fetchDuring ? seq[i].db : ir;
                cs = nc; ir = ni;
                gIR.push_back(ir); gT.push_back(cs); gF.push_back(cs == 0 ? 1 : 0);
        }

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "fetch: interpreted loaded")) return;
        std::vector<int> iIR, iT, iF;
        runFetchOnEngine(interp, seq, SETTLE, iIR, iT, iF);
        int gi = 0;
        for (size_t i = 0; i < seq.size(); ++i) if (iIR[i] != gIR[i] || iT[i] != gT[i] || iF[i] != gF[i]) gi++;
        tf::check(gi == 0, "fetch: interpreted matches golden (opcode latched on FETCH, held through execute)");

        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("fetch: native ") + modeName[m] + " built")) continue;
                std::vector<int> nIR, nT, nF;
                runFetchOnEngine(nat, seq, SETTLE, nIR, nT, nF);
                int gn = 0, df = 0;
                for (size_t i = 0; i < seq.size(); ++i)
                {
                        if (nIR[i] != gIR[i] || nT[i] != gT[i] || nF[i] != gF[i]) gn++;
                        if (nIR[i] != iIR[i] || nT[i] != iT[i] || nF[i] != iF[i]) df++;
                }
                tf::check(gn == 0, std::string("fetch: native ") + modeName[m] + " matches golden");
                tf::check(df == 0, std::string("fetch: interpreted == native ") + modeName[m]);
        }
}

static void runProgFetchOnEngine(Part& p, const std::vector<FuStep>& seq, int settleSteps,
                                 std::vector<int>& pcOut, std::vector<int>& irOut, std::vector<int>& tOut, std::vector<int>& fOut)
{
        std::vector<State> out;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                for (int clk = 0; clk < 2; ++clk)
                {
                        std::vector<int> in(11, 0);
                        for (int k = 0; k < 8; ++k) in[k] = (seq[i].db >> k) & 1;
                        in[8] = seq[i].rst; in[9] = seq[i].done; in[10] = clk;
                        std::vector<State> sin = toStates(in);
                        for (int t = 0; t < settleSteps; ++t) out = p(sin);
                }
                std::vector<int> b = toBits(out);
                int pc = 0; for (int k = 0; k < 16; ++k) pc |= b[k] << k;
                int ir = 0; for (int k = 0; k < 8; ++k) ir |= b[16 + k] << k;
                pcOut.push_back(pc); irOut.push_back(ir);
                tOut.push_back(b[24] | (b[25] << 1) | (b[26] << 2));
                fOut.push_back(b[27]);
        }
}

static void testProgramFetch()
{
        tf::section("6502 program fetch (fetch unit + program counter walking memory): interp, native inline, native link");
        const std::string NAME = "6502_Program_Fetch";
        const int SETTLE = 100;
        std::vector<FuStep> seq = {
                { 0xA9, 1, 0 }, { 0xB8, 0, 0 }, { 0x11, 0, 0 }, { 0x22, 0, 1 }, { 0xC0, 0, 0 },
                { 0x33, 0, 1 }, { 0xD0, 0, 0 }, { 0x44, 0, 0 }, { 0x55, 0, 0 }, { 0x66, 0, 1 },
                { 0xE0, 0, 0 }, { 0x77, 1, 0 }, { 0x99, 0, 0 } };

        std::vector<int> gPC, gIR, gT, gF;
        int cs = 0, ir = 0, pc = 0;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                int fd = (cs == 0) ? 1 : 0;
                int ncs = (seq[i].rst || seq[i].done) ? 0 : ((cs + 1) & 7);
                int nir = fd ? seq[i].db : ir;
                int npc = seq[i].rst ? 0 : (fd ? ((pc + 1) & 0xFFFF) : pc);
                cs = ncs; ir = nir; pc = npc;
                gPC.push_back(pc); gIR.push_back(ir); gT.push_back(cs); gF.push_back(cs == 0 ? 1 : 0);
        }

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "progfetch: interpreted loaded")) return;
        std::vector<int> iPC, iIR, iT, iF;
        runProgFetchOnEngine(interp, seq, SETTLE, iPC, iIR, iT, iF);
        int gi = 0;
        for (size_t i = 0; i < seq.size(); ++i) if (iPC[i] != gPC[i] || iIR[i] != gIR[i] || iT[i] != gT[i] || iF[i] != gF[i]) gi++;
        tf::check(gi == 0, "progfetch: interpreted matches golden (PC increments per fetch, clears on reset)");

        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, std::string("progfetch: native ") + modeName[m] + " built")) continue;
                std::vector<int> nPC, nIR, nT, nF;
                runProgFetchOnEngine(nat, seq, SETTLE, nPC, nIR, nT, nF);
                int gn = 0, df = 0;
                for (size_t i = 0; i < seq.size(); ++i)
                {
                        if (nPC[i] != gPC[i] || nIR[i] != gIR[i] || nT[i] != gT[i] || nF[i] != gF[i]) gn++;
                        if (nPC[i] != iPC[i] || nIR[i] != iIR[i] || nT[i] != iT[i] || nF[i] != iF[i]) df++;
                }
                tf::check(gn == 0, std::string("progfetch: native ") + modeName[m] + " matches golden");
                tf::check(df == 0, std::string("progfetch: interpreted == native ") + modeName[m]);
        }
}

static void testAluExecute()
{
        tf::section("6502 ALU execute datapath (opcode + A + M -> new A + N/Z/C/V): interp, native inline, native link");
        const std::string NAME = "6502_ALU_Execute";
        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, "ALU execute: interpreted loaded")) return;
        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        Part nat[2];
        bool built[2] = { false, false };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                nat[m] = buildNative(NAME, nOut, linkMode[m]);
                built[m] = tf::check(nat[m] != nullptr, std::string("ALU execute: native ") + modeName[m] + " built");
        }
        int opcodes[6] = { 0x01, 0x21, 0x41, 0x61, 0xC1, 0xE1 };   // ORA AND EOR ADC CMP SBC (cc=01)
        int aaa[6] = { 0, 1, 2, 3, 6, 7 };
        unsigned seed = 99991u;
        bool okI = true, okN[2] = { true, true };
        for (int t = 0; t < 3000; ++t)
        {
                seed = seed * 1103515245u + 12345u; int oi = (seed >> 16) % 6;
                seed = seed * 1103515245u + 12345u; int A = (seed >> 16) & 0xFF;
                seed = seed * 1103515245u + 12345u; int M = (seed >> 16) & 0xFF;
                seed = seed * 1103515245u + 12345u; int Cin = (seed >> 16) & 1;
                int opcode = opcodes[oi];
                std::vector<int> in(25, 0);
                for (int k = 0; k < 8; ++k) { in[k] = (opcode >> k) & 1; in[8 + k] = (A >> k) & 1; in[16 + k] = (M >> k) & 1; }
                in[24] = Cin;
                int res = 0, eC = 0, eV = 0;
                bool isAdc = (aaa[oi] == 3);
                bool isSub = (aaa[oi] == 6 || aaa[oi] == 7);
                if (aaa[oi] == 0) res = A | M;
                else if (aaa[oi] == 1) res = A & M;
                else if (aaa[oi] == 2) res = A ^ M;
                else if (isAdc) { int sum = A + M + Cin; res = sum & 0xFF; eC = (sum >= 256) ? 1 : 0;
                        int a7 = (A >> 7) & 1, m7 = (M >> 7) & 1, r7 = (res >> 7) & 1;
                        eV = ((a7 && m7 && !r7) || (!a7 && !m7 && r7)) ? 1 : 0; }
                else { int diff = A + (M ^ 0xFF) + Cin; res = diff & 0xFF; eC = (diff >= 256) ? 1 : 0;
                        int a7 = (A >> 7) & 1, m7 = (M >> 7) & 1, r7 = (res >> 7) & 1;
                        eV = ((a7 && !m7 && !r7) || (!a7 && m7 && r7)) ? 1 : 0; }
                int eN = (res >> 7) & 1, eZ = (res == 0) ? 1 : 0;
                bool checkCV = isAdc || isSub;
                std::vector<int> oiv = toBits(interp(toStates(in)));
                int aout = 0; for (int k = 0; k < 8; ++k) aout |= oiv[k] << k;
                if (aout != res || oiv[8] != eN || oiv[9] != eZ) okI = false;
                if (checkCV && (oiv[10] != eC || oiv[11] != eV)) okI = false;
                for (int m = 0; m < 2; ++m)
                {
                        if (!built[m]) continue;
                        std::vector<int> onv = toBits(nat[m](toStates(in)));
                        int ao = 0; for (int k = 0; k < 8; ++k) ao |= onv[k] << k;
                        if (ao != res || onv[8] != eN || onv[9] != eZ) okN[m] = false;
                        if (checkCV && (onv[10] != eC || onv[11] != eV)) okN[m] = false;
                }
        }
        tf::check(okI, "ALU execute: interpreted matches 6502 semantics (ORA/AND/EOR/ADC/CMP/SBC, 3000 random)");
        for (int m = 0; m < 2; ++m) if (built[m]) tf::check(okN[m], std::string("ALU execute: native ") + modeName[m] + " matches 6502 semantics");
}

struct EsOp { int opcode; int M; };
struct EsState { int A; int N; int Z; int C; int V; };

static void esGoldenStep(EsState& st, const EsOp& op)
{
        bool isAslA = op.opcode == 0x0A;
        bool isRolA = op.opcode == 0x2A;
        bool isLsrA = op.opcode == 0x4A;
        bool isRorA = op.opcode == 0x6A;
        if (isAslA || isRolA || isLsrA || isRorA)
        {
                int dir = (isLsrA || isRorA) ? 1 : 0;
                int rot = (isRolA || isRorA) ? 1 : 0;
                int inbit = rot ? st.C : 0;
                int R, Cout;
                if (dir == 0) { R = ((st.A << 1) | inbit) & 0xFF; Cout = (st.A >> 7) & 1; }
                else { R = (st.A >> 1) | (inbit << 7); Cout = st.A & 1; }
                st.A = R;
                st.N = (R >> 7) & 1;
                st.Z = (R == 0) ? 1 : 0;
                st.C = Cout;
                return;
        }
        bool isOra = op.opcode == 0x01;
        bool isAnd = op.opcode == 0x21;
        bool isEor = op.opcode == 0x41;
        bool isAdc = op.opcode == 0x61;
        bool isCmp = op.opcode == 0xC1;
        bool isSbc = op.opcode == 0xE1;
        bool isLogic = isOra || isAnd || isEor;
        bool isArith = isAdc || isSbc || isCmp;
        int R = st.A;
        int newC = st.C, newV = st.V;
        if (isOra) R = st.A | op.M;
        else if (isAnd) R = st.A & op.M;
        else if (isEor) R = st.A ^ op.M;
        else if (isAdc)
        {
                int sum = st.A + op.M + st.C;
                R = sum & 0xFF;
                newC = (sum >= 256) ? 1 : 0;
                int a7 = (st.A >> 7) & 1, m7 = (op.M >> 7) & 1, r7 = (R >> 7) & 1;
                newV = ((a7 && m7 && !r7) || (!a7 && !m7 && r7)) ? 1 : 0;
        }
        else if (isSbc || isCmp)
        {
                int cin = isCmp ? 1 : st.C;
                int diff = st.A + (op.M ^ 0xFF) + cin;
                R = diff & 0xFF;
                newC = (diff >= 256) ? 1 : 0;
                int a7 = (st.A >> 7) & 1, m7 = (op.M >> 7) & 1, r7 = (R >> 7) & 1;
                newV = ((a7 && !m7 && !r7) || (!a7 && m7 && r7)) ? 1 : 0;
        }
        if (isLogic || isArith)
        {
                st.N = (R >> 7) & 1;
                st.Z = (R == 0) ? 1 : 0;
        }
        if (isLogic || isAdc || isSbc) st.A = R;
        if (isArith) st.C = newC;
        if (isAdc || isSbc) st.V = newV;
}

static void runEseqOnEngine(Part& p, const std::vector<EsOp>& seq, int settleSteps, std::vector<EsState>& perOp)
{
        std::vector<State> out;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                for (int clk = 0; clk < 2; ++clk)
                {
                        std::vector<int> in(17, 0);
                        for (int k = 0; k < 8; ++k) { in[k] = (seq[i].opcode >> k) & 1; in[8 + k] = (seq[i].M >> k) & 1; }
                        in[16] = clk;
                        std::vector<State> sin = toStates(in);
                        for (int t = 0; t < settleSteps; ++t) out = p(sin);
                }
                std::vector<int> b = toBits(out);
                EsState s{ 0, 0, 0, 0, 0 };
                for (int k = 0; k < 8; ++k) s.A |= b[k] << k;
                s.N = b[8]; s.Z = b[9]; s.C = b[10]; s.V = b[11];
                perOp.push_back(s);
        }
}

static void runSeqExecuteUnit(const std::string& NAME, const std::string& section, const std::vector<EsOp>& seq)
{
        tf::section(section);
        const int SETTLE = 80;

        std::vector<EsState> golden;
        EsState g{ 0, 0, 0, 0, 0 };
        for (size_t i = 0; i < seq.size(); ++i) { esGoldenStep(g, seq[i]); golden.push_back(g); }

        int iIn = 0, iOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + NAME + ".json", iIn, iOut);
        if (!tf::check(interp != nullptr, NAME + ": interpreted loaded")) return;

        std::vector<EsState> gotI;
        runEseqOnEngine(interp, seq, SETTLE, gotI);
        int goldFailsI = 0;
        for (size_t i = 0; i < seq.size(); ++i)
        {
                const EsState& a = gotI[i]; const EsState& e = golden[i];
                if (a.A != e.A || a.N != e.N || a.Z != e.Z || a.C != e.C || a.V != e.V) goldFailsI++;
        }
        tf::check(goldFailsI == 0, NAME + ": interpreted matches 6502 golden (" + std::to_string(seq.size()) + "-op program)",
                  std::to_string(goldFailsI) + " mismatched ops");

        const char* modeName[2] = { "inline", "link" };
        bool linkMode[2] = { false, true };
        for (int m = 0; m < 2; ++m)
        {
                int nOut = 0;
                Part nat = buildNative(NAME, nOut, linkMode[m]);
                if (!tf::check(nat != nullptr, NAME + ": native " + modeName[m] + " built")) continue;

                std::vector<EsState> gotN;
                runEseqOnEngine(nat, seq, SETTLE, gotN);

                int goldFailsN = 0, diffFails = 0;
                for (size_t i = 0; i < seq.size(); ++i)
                {
                        const EsState& e = golden[i]; const EsState& a = gotN[i]; const EsState& b = gotI[i];
                        if (a.A != e.A || a.N != e.N || a.Z != e.Z || a.C != e.C || a.V != e.V) goldFailsN++;
                        if (a.A != b.A || a.N != b.N || a.Z != b.Z || a.C != b.C || a.V != b.V) diffFails++;
                }
                tf::check(goldFailsN == 0, NAME + ": native " + modeName[m] + " matches 6502 golden",
                          std::to_string(goldFailsN) + " mismatched ops");
                tf::check(diffFails == 0, NAME + ": interpreted == native " + std::string(modeName[m]),
                          std::to_string(diffFails) + " divergent ops");
        }
}

static void testAluExecuteSequential()
{
        std::vector<EsOp> seq = {
                { 0x01, 0x0F }, { 0x21, 0xF0 }, { 0x41, 0xFF },
                { 0x61, 0x01 }, { 0x61, 0x00 }, { 0x61, 0x7F }, { 0x01, 0x00 } };
        runSeqExecuteUnit("6502_ALU_Execute_Sequential",
                          "6502 sequential ALU execute (clocked A + P flags fed back through execute): interp, native inline, native link",
                          seq);
}

static void testAluWriteback()
{
        std::vector<EsOp> seq = {
                { 0x01, 0x3C }, { 0x61, 0x10 }, { 0xC1, 0x4C }, { 0xC1, 0x50 }, { 0xE1, 0x0C },
                { 0xE1, 0x40 }, { 0x61, 0x50 }, { 0xE1, 0x80 }, { 0xC1, 0xD0 }, { 0x41, 0xFF } };
        runSeqExecuteUnit("6502_ALU_Writeback",
                          "6502 ALU write-back (register-file accumulator; ORA/AND/EOR/ADC/CMP/SBC with per-op write-enable and flag mask): interp, native inline, native link",
                          seq);
}

static void testAccumulatorExecute()
{
        std::vector<EsOp> seq = {
                { 0x01, 0x81 }, { 0x0A, 0x00 }, { 0x2A, 0x00 }, { 0x4A, 0x00 }, { 0x6A, 0x00 },
                { 0x61, 0x01 }, { 0x0A, 0x00 }, { 0x6A, 0x00 }, { 0xC1, 0x82 }, { 0x4A, 0x00 } };
        runSeqExecuteUnit("6502_Accumulator_Execute",
                          "6502 accumulator datapath (ALU group + shifts ASL/ROL/LSR/ROR A, muxed by opcode, into the register file): interp, native inline, native link",
                          seq);
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
        testCombinational("74181_4-bit_Arithmetic_Logic_Unit", 14, [](const std::vector<int>& v){ int A[4] = { v[0], v[1], v[2], v[3] }; int B[4] = { v[4], v[5], v[6], v[7] }; int S[4] = { v[8], v[9], v[10], v[11] }; int M = v[12], Cn = v[13], carry = 1 - Cn, F[4]; for (int i = 0; i < 4; ++i) { int P = A[i] | (B[i] & S[0]) | ((!B[i]) & S[1]); int G = A[i] & ((B[i] & S[3]) | ((!B[i]) & S[2])); int ce = M | carry; F[i] = (P ^ G) ^ ce; carry = G | (P & carry); } return std::vector<int>{ F[0], F[1], F[2], F[3], 1 - carry, F[0] & F[1] & F[2] & F[3] }; });

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
        testAlu8();
        testPStatusRegister();
        testRegisterFile();
        testCombinational("6502_ALU_Control_Decoder", 8, [](const std::vector<int>& v){
                int cc = v[0] | (v[1] << 1);
                int aaa = v[5] | (v[6] << 1) | (v[7] << 2);
                int S0 = 0, S1 = 0, S2 = 0, S3 = 0, M = 0, ISALU = 0;
                if (cc == 1)
                {
                        switch (aaa)
                        {
                                case 0: S0 = 0; S1 = 1; S2 = 1; S3 = 1; M = 1; ISALU = 1; break;
                                case 1: S0 = 1; S1 = 1; S2 = 0; S3 = 1; M = 1; ISALU = 1; break;
                                case 2: S0 = 0; S1 = 1; S2 = 1; S3 = 0; M = 1; ISALU = 1; break;
                                case 3: S0 = 1; S1 = 0; S2 = 0; S3 = 1; M = 0; ISALU = 1; break;
                                case 6: S0 = 0; S1 = 1; S2 = 1; S3 = 0; M = 0; ISALU = 1; break;
                                case 7: S0 = 0; S1 = 1; S2 = 1; S3 = 0; M = 0; ISALU = 1; break;
                                default: break;
                        }
                }
                return std::vector<int>{ S0, S1, S2, S3, M, ISALU };
        });
        testAluDecodeEndToEnd();
        testCombinational("6502_Flag_Op_Decoder", 8, [](const std::vector<int>& v){
                int cc = v[0] | (v[1] << 1);
                int bbb = v[2] | (v[3] << 1) | (v[4] << 2);
                int aaa = v[5] | (v[6] << 1) | (v[7] << 2);
                int F[8] = { 0 }, L[8] = { 0 };
                if (cc == 0 && bbb == 6)
                {
                        switch (aaa)
                        {
                                case 0: L[0] = 1; F[0] = 0; break;
                                case 1: L[0] = 1; F[0] = 1; break;
                                case 2: L[2] = 1; F[2] = 0; break;
                                case 3: L[2] = 1; F[2] = 1; break;
                                case 5: L[6] = 1; F[6] = 0; break;
                                case 6: L[3] = 1; F[3] = 0; break;
                                case 7: L[3] = 1; F[3] = 1; break;
                                default: break;
                        }
                }
                std::vector<int> r;
                for (int k = 0; k < 8; ++k) r.push_back(F[k]);
                for (int k = 0; k < 8; ++k) r.push_back(L[k]);
                return r;
        });
        testFlagDecodeEndToEnd();
        testCombinational("6502_Transfer_Decoder", 8, [](const std::vector<int>& v){
                int opc = 0;
                for (int k = 0; k < 8; ++k) opc |= v[k] << k;
                int rs = 0, ws = 0, we = 0;
                switch (opc)
                {
                        case 0xAA: rs = 0; ws = 1; we = 1; break;   // TAX  A->X
                        case 0x8A: rs = 1; ws = 0; we = 1; break;   // TXA  X->A
                        case 0xA8: rs = 0; ws = 2; we = 1; break;   // TAY  A->Y
                        case 0x98: rs = 2; ws = 0; we = 1; break;   // TYA  Y->A
                        case 0xBA: rs = 3; ws = 1; we = 1; break;   // TSX  SP->X
                        case 0x9A: rs = 1; ws = 3; we = 1; break;   // TXS  X->SP
                        default: break;
                }
                return std::vector<int>{ rs & 1, (rs >> 1) & 1, ws & 1, (ws >> 1) & 1, we };
        });
        testTransferDecodeEndToEnd();
        testCombinational("6502_ALU_Flag_Logic", 12, [](const std::vector<int>& v){
                int R = 0;
                for (int k = 0; k < 8; ++k) R |= v[k] << k;
                int A7 = v[8], B7 = v[9], Cout = v[10], isSub = v[11], R7 = v[7];
                int N = R7;
                int Z = (R == 0) ? 1 : 0;
                int Cf = Cout;
                int Vadd = ((A7 && B7 && !R7) || (!A7 && !B7 && R7)) ? 1 : 0;
                int Vsub = ((A7 && !B7 && !R7) || (!A7 && B7 && R7)) ? 1 : 0;
                int V = isSub ? Vsub : Vadd;
                return std::vector<int>{ N, Z, Cf, V };
        });
        testFlagLogicEndToEnd();
        testShifter();
        testIncDec();
        testCompareBit();
        testBranchCondition();
        testCycleCounter();
        testFetchUnit();
        testProgramFetch();
        testAluExecute();
        testAluExecuteSequential();
        testAluWriteback();
        testAccumulatorExecute();
        testRamPart();
        testRom();
        testRomMulti();
        testLearningCircuits();




        cleanupModules();
        return tf::summary();
}
