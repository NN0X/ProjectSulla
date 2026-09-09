#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <filesystem>

#include "part.h"
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

static void testPerfCircuit(const std::string& name, int ticks)
{
        std::printf("\n%s%s== perf circuit in validation set: %s ==%s\n",
                    tf::ansiBold(), tf::ansiCyan(), name.c_str(), tf::ansiRst());
        int nIn = 0, nOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", nIn, nOut);
        tf::check(interp != nullptr, name + ": interpreted engine loaded");
        if (!interp) return;

        int ocI = 0, ocL = 0;
        Part inl = buildNative(name, ocI, false);
        Part lnk = buildNative(name, ocL, true);
        tf::check(inl != nullptr, name + ": native-inline compiled + loaded");
        tf::check(lnk != nullptr, name + ": native-link compiled + loaded");
        if (!inl || !lnk) return;

        bool agree = true;
        for (int t = 0; t < ticks; ++t)
        {
                std::vector<int> pat(nIn);
                for (int i = 0; i < nIn; ++i)
                        pat[i] = (int)((2654435761u * (uint32_t)(i + 1 + t * 7)) >> 13) & 1;
                std::vector<State> in = toStates(pat);
                std::vector<int> ri = toBits(interp(in));
                std::vector<int> rn = toBits(inl(in));
                std::vector<int> rl = toBits(lnk(in));
                if (rn != ri || rl != ri) agree = false;
        }
        tf::check(agree, name + ": interp == native-inline == native-link across " + std::to_string(ticks) + " ticks");
}

static void testRam(const std::string& name, bool sync)
{
        std::printf("\n%s%s== memory primitive in validation set: %s ==%s\n",
                    tf::ansiBold(), tf::ansiCyan(), name.c_str(), tf::ansiRst());
        const int A = 8, W = 8;
        int nIn = 0, nOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", nIn, nOut);
        tf::check(interp != nullptr, name + ": interpreted engine loaded");
        if (!interp) return;
        int ocI = 0, ocL = 0;
        Part inl = buildNative(name, ocI, false);
        Part lnk = buildNative(name, ocL, true);
        tf::check(inl != nullptr, name + ": native-inline compiled + loaded");
        tf::check(lnk != nullptr, name + ": native-link compiled + loaded");
        if (!inl || !lnk) return;

        struct Op { int we; unsigned a, d; };
        std::vector<Op> seq = { {1,5,0xAA},{0,5,0},{1,5,0x11},{1,9,0x33},{0,9,0},{0,5,0},{1,5,0x77},{0,5,0} };
        std::vector<unsigned> mem(1u << A, 0); unsigned dreg = 0;
        bool ok = true;
        for (size_t s = 0; s < seq.size(); ++s)
        {
                Op op = seq[s];
                std::vector<int> pat(nIn, 0);
                if (nIn > 0) pat[0] = op.we ? 1 : 0;
                for (int k = 0; k < A && 1 + k < nIn; ++k) pat[1 + k] = (op.a >> k) & 1;
                for (int k = 0; k < W && 1 + A + k < nIn; ++k) pat[1 + A + k] = (op.d >> k) & 1;
                std::vector<State> in = toStates(pat);
                unsigned gi = 0, gn = 0, gl = 0;
                { std::vector<int> b = toBits(interp(in)); for (size_t k = 0; k < b.size(); ++k) if (b[k]) gi |= 1u << k; }
                { std::vector<int> b = toBits(inl(in));    for (size_t k = 0; k < b.size(); ++k) if (b[k]) gn |= 1u << k; }
                { std::vector<int> b = toBits(lnk(in));    for (size_t k = 0; k < b.size(); ++k) if (b[k]) gl |= 1u << k; }
                unsigned gold = sync ? dreg : mem[op.a];
                if (sync) dreg = mem[op.a];
                if (op.we) mem[op.a] = op.d;
                if (gi != gold || gn != gold || gl != gold) ok = false;
        }
        tf::check(ok, name + ": interp == inline == link == golden over read/write sequence");
}

static void testArith(const std::string& name, bool isMul, int W)
{
        std::printf("\n%s%s== word-level arithmetic primitive in validation set: %s ==%s\n",
                    tf::ansiBold(), tf::ansiCyan(), name.c_str(), tf::ansiRst());
        int nIn = 0, nOut = 0;
        Part interp = loadLayoutAsPart("layouts/" + name + ".json", nIn, nOut);
        tf::check(interp != nullptr, name + ": interpreted engine loaded");
        if (!interp) return;
        int ocI = 0, ocL = 0;
        Part inl = buildNative(name, ocI, false);
        Part lnk = buildNative(name, ocL, true);
        tf::check(inl != nullptr, name + ": native-inline compiled + loaded");
        tf::check(lnk != nullptr, name + ": native-link compiled + loaded");
        if (!inl || !lnk) return;

        unsigned long long mask = (W >= 64) ? ~0ull : ((1ull << W) - 1);
        unsigned long long tv[][2] = { {0,0},{1,1},{mask,mask},{123,45},{200,201},{65535,65535},{12345,54321},{7,9} };
        bool ok = true;
        for (size_t t = 0; t < sizeof(tv) / sizeof(tv[0]); ++t)
        {
                unsigned long long a = tv[t][0] & mask, b = tv[t][1] & mask;
                unsigned long long gold = isMul ? (a * b) : (a + b);
                std::vector<int> pat(nIn, 0);
                for (int k = 0; k < W && k < nIn; ++k) pat[k] = (a >> k) & 1;
                for (int k = 0; k < W && W + k < nIn; ++k) pat[W + k] = (b >> k) & 1;
                std::vector<State> in = toStates(pat);
                unsigned long long gi = 0, gn = 0, gl = 0;
                { std::vector<int> z = toBits(interp(in)); for (size_t k = 0; k < z.size(); ++k) if (z[k]) gi |= 1ull << k; }
                { std::vector<int> z = toBits(inl(in));    for (size_t k = 0; k < z.size(); ++k) if (z[k]) gn |= 1ull << k; }
                { std::vector<int> z = toBits(lnk(in));    for (size_t k = 0; k < z.size(); ++k) if (z[k]) gl |= 1ull << k; }
                if (gi != gold || gn != gold || gl != gold) ok = false;
        }
        tf::check(ok, name + ": interp == inline == link == golden (a " + std::string(isMul ? "*" : "+") + " b)");
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
        testPerInstanceState();

        testPerfCircuit("adder8", 4);
        testPerfCircuit("adder16", 4);
        testPerfCircuit("adder32", 4);
        testPerfCircuit("mul8", 4);
        testPerfCircuit("mul16", 4);
        testPerfCircuit("cpu8", 8);
        testPerfCircuit("regbank64", 8);
        testRam("ram_async", false);
        testRam("ram_sync", true);

        testArith("wadd8", false, 8);
        testArith("wmul8", true, 8);
        testArith("wmul16", true, 16);

        cleanupModules();
        return tf::summary();
}
