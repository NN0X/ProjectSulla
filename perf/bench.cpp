#include <cstdint>
#include <cstdio>
#include <chrono>
#include <string>
#include <vector>
#include <filesystem>

#include <dlfcn.h>
#include <unistd.h>

#include "part.h"
#include "utils.h"
#include "appstate.h"
#include "compiler/compiler.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static bool g_tty = false;
static const char* Cc(const char* s) { return g_tty ? s : ""; }
#define A_BOLD  Cc("\033[1m")
#define A_CYAN  Cc("\033[36m")
#define A_GREEN Cc("\033[32m")
#define A_RED   Cc("\033[31m")
#define A_DIM   Cc("\033[2m")
#define A_RST   Cc("\033[0m")

static int g_failures = 0;
static void expect(bool cond, const std::string& what)
{
        if (!cond) { g_failures++; std::printf("  %s[FAIL]%s %s\n", A_RED, A_RST, what.c_str()); }
}

static std::vector<State> toStates(const std::vector<int>& b)
{
        std::vector<State> s(b.size());
        for (size_t i = 0; i < b.size(); ++i) s[i] = b[i] ? STATE_HIGH : STATE_LOW;
        return s;
}
static std::vector<int> toBits(const std::vector<State>& s)
{
        std::vector<int> b(s.size());
        for (size_t i = 0; i < s.size(); ++i) b[i] = (s[i] == STATE_HIGH) ? 1 : 0;
        return b;
}

static int countOutputs(const AppState& st)
{
        int n = 0;
        for (const auto& kv : st.partTypes)
                if (kv.second == PART_TYPE_OUTPUT)
                        n += st.inputCounts.count(kv.first) ? st.inputCounts.at(kv.first) : 0;
        return n;
}

struct Result { double nsPerTick; double ticksPerSec; unsigned long long iters; };

template <class Step>
static Result timeIt(Step step, double target)
{
        volatile uint64_t sink = 0;
        for (int i = 0; i < 2000; ++i) sink += step();

        unsigned long long total = 0, batch = 1000;
        double elapsed = 0.0;
        while (elapsed < target)
        {
                auto t0 = Clock::now();
                for (unsigned long long i = 0; i < batch; ++i) sink += step();
                auto t1 = Clock::now();
                double dt = std::chrono::duration<double>(t1 - t0).count();
                elapsed += dt; total += batch;
                if (dt > 0) { double rate = (double)batch / dt; batch = (unsigned long long)(rate * (target / 4)); if (batch < 1000) batch = 1000; }
        }
        (void)sink;
        Result r;
        r.iters = total;
        r.ticksPerSec = (double)total / elapsed;
        r.nsPerTick = 1e9 * elapsed / (double)total;
        return r;
}

typedef void (*ExecFn)(const uint8_t*, uint8_t*);
typedef void (*BatchFn)(const uint64_t*, uint64_t*);

static std::vector<std::string> g_builtModules;

int main()
{
        g_tty = isatty(fileno(stdout));
        std::printf("%s%sSulla performance suite%s  (interpreted vs native, all modes)\n", A_BOLD, A_CYAN, A_RST);
        std::printf("%stiming: auto-scaled batches, ~0.30s per measurement%s\n", A_DIM, A_RST);

        struct Row { std::string name; int nIn; int nOut;
                     Result interp, inl, link, raw; };
        std::vector<Row> rows;

        const double TARGET = 0.30;
        const char* circuits[] = { "full_adder", "adder8", "adder16", "adder32", "mul8", "mul16", "cpu8", "regbank64", "ram_async", "ram_sync", "wadd8", "wmul8", "wmul16" };

        {
                AppState child;
                loadLayout(child, "layouts/full_adder.json");
                std::string code = transpileToCpp(child, false);
                bool ok = compilePartLibrary(code, "full_adder", true, true);
                g_builtModules.push_back("full_adder");
                expect(ok && fs::exists("parts/full_adder/libfull_adder.so"), "child parts/full_adder/libfull_adder.so built for link mode");
        }
        {
                AppState child;
                loadLayout(child, "layouts/dff.json");
                std::string code = transpileToCpp(child, false);
                bool ok = compilePartLibrary(code, "dff", true, true);
                g_builtModules.push_back("dff");
                expect(ok && fs::exists("parts/dff/libdff.so"), "child parts/dff/libdff.so built for link mode");
        }

        for (const std::string name : circuits)
        {
                std::string layout = "layouts/" + name + ".json";
                std::printf("\n%s%s== %s ==%s\n", A_BOLD, A_CYAN, name.c_str(), A_RST);

                int nIn = 0, nOut = 0;
                Part interp = loadLayoutAsPart(layout, nIn, nOut);
                expect(interp != nullptr, name + ": interpreted engine loaded");
                if (!interp) continue;

                AppState st;
                loadLayout(st, layout);
                int oc = countOutputs(st);

                std::string modI = "perf_" + name + "_inl";
                std::string modL = "perf_" + name + "_lnk";
                bool cok = compileSharedLibrary(transpileToCpp(st, false), modI);
                bool lok = compileSharedLibrary(transpileToCpp(st, true),  modL);
                g_builtModules.push_back(modI);
                g_builtModules.push_back(modL);
                expect(cok, name + ": native-inline compiled");
                expect(lok, name + ": native-link compiled");

                Part natInl  = loadCompiledPart(modI, oc);
                Part natLink = loadCompiledPart(modL, oc);
                expect(natInl  != nullptr, name + ": native-inline loaded");
                expect(natLink != nullptr, name + ": native-link loaded");
                if (!natInl || !natLink) continue;

                void* rawHandle = dlopen(("./parts/lib" + modI + ".so").c_str(), RTLD_LAZY | RTLD_LOCAL);
                ExecFn rawFn = rawHandle ? (ExecFn)dlsym(rawHandle, "executeTick") : nullptr;
                expect(rawFn != nullptr, name + ": native-raw executeTick resolved");

                std::vector<int> pat(nIn);
                for (int i = 0; i < nIn; ++i) pat[i] = (int)((2654435761u * (uint32_t)(i + 1)) >> 17) & 1;
                std::vector<State> in = toStates(pat);

                {
                        std::vector<int> ref = toBits(interp(in));
                        expect(toBits(natInl(in))  == ref, name + ": native-inline output == interpreted");
                        expect(toBits(natLink(in)) == ref, name + ": native-link output == interpreted");
                }

                Result rInterp = timeIt([&]{ auto o = interp(in);  uint64_t a = 0; for (auto s : o) a += (s == STATE_HIGH); return a; }, TARGET);
                Result rInl    = timeIt([&]{ auto o = natInl(in);  uint64_t a = 0; for (auto s : o) a += (s == STATE_HIGH); return a; }, TARGET);
                Result rLink   = timeIt([&]{ auto o = natLink(in); uint64_t a = 0; for (auto s : o) a += (s == STATE_HIGH); return a; }, TARGET);

                Result rRaw{};
                if (rawFn)
                {
                        std::vector<uint8_t> ri(nIn ? nIn : 1, 0), ro(oc ? oc : 1, 0);
                        for (int i = 0; i < nIn; ++i) ri[i] = (uint8_t)pat[i];
                        rRaw = timeIt([&]{ rawFn(ri.data(), ro.data()); uint64_t a = 0; for (int i = 0; i < oc; ++i) a += ro[i]; return a; }, TARGET);
                }

                expect(rInl.ticksPerSec > rInterp.ticksPerSec, name + ": native-inline faster than interpreted");

                rows.push_back({ name, nIn, nOut, rInterp, rInl, rLink, rRaw });

                std::printf("  %-13s %14s %13s %10s\n", "mode", "ticks/sec", "ns/tick", "speedup");
                auto line = [&](const char* label, const Result& r) {
                        std::printf("  %-13s %14.0f %13.1f %9.1fx\n", label, r.ticksPerSec, r.nsPerTick, r.ticksPerSec / rInterp.ticksPerSec);
                };
                line("interpreted",   rInterp);
                line("native-inline", rInl);
                line("native-link",   rLink);
                if (rawFn) line("native-raw", rRaw);

                if (rawHandle) dlclose(rawHandle);
        }

        std::printf("\n%s%s== summary: ns/tick (lower is better) ==%s\n", A_BOLD, A_CYAN, A_RST);
        std::printf("  %-10s %6s %6s %12s %12s %12s %12s\n", "circuit", "in", "out",
                    "interp", "nat-inline", "nat-link", "nat-raw");
        for (const Row& r : rows)
                std::printf("  %-10s %6d %6d %12.1f %12.1f %12.1f %12.1f\n",
                            r.name.c_str(), r.nIn, r.nOut,
                            r.interp.nsPerTick, r.inl.nsPerTick, r.link.nsPerTick, r.raw.nsPerTick);

        std::printf("\n%s%s== summary: speedup vs interpreted (higher is better) ==%s\n", A_BOLD, A_CYAN, A_RST);
        std::printf("  %-10s %14s %14s %14s\n", "circuit", "nat-inline", "nat-link", "nat-raw");
        for (const Row& r : rows)
                std::printf("  %-10s %13.1fx %13.1fx %13.1fx\n", r.name.c_str(),
                            r.inl.ticksPerSec / r.interp.ticksPerSec,
                            r.link.ticksPerSec / r.interp.ticksPerSec,
                            r.raw.ticksPerSec / r.interp.ticksPerSec);

        std::printf("\n%s-- csv --%s\n", A_DIM, A_RST);
        std::printf("circuit,inputs,outputs,mode,ticks_per_sec,ns_per_tick,speedup_vs_interp\n");
        for (const Row& r : rows)
        {
                auto emit = [&](const char* mode, const Result& x) {
                        std::printf("%s,%d,%d,%s,%.0f,%.2f,%.2f\n", r.name.c_str(), r.nIn, r.nOut, mode,
                                    x.ticksPerSec, x.nsPerTick, x.ticksPerSec / r.interp.ticksPerSec);
                };
                emit("interpreted",   r.interp);
                emit("native-inline", r.inl);
                emit("native-link",   r.link);
                emit("native-raw",    r.raw);
        }

        std::printf("\n%s%s== bit-sliced (64-lane) batch throughput: ns per vector ==%s\n", A_BOLD, A_CYAN, A_RST);
        std::printf("  %-10s %14s %16s %12s\n", "circuit", "raw ns/tick", "bitsliced/vec", "speedup");
        for (const std::string name : circuits)
        {
                AppState st;
                loadLayout(st, "layouts/" + name + ".json");
                std::string code = transpileToCppBitsliced(st);
                if (code.empty()) continue;
                int nIn = 0;
                for (const auto& kv : st.partTypes) if (kv.second == PART_TYPE_SOURCE) nIn++;
                int nOut = countOutputs(st);
                std::string mod = "perf_" + name + "_bs";
                if (!compileSharedLibrary(code, mod)) continue;
                g_builtModules.push_back(mod);
                void* h = dlopen(("./parts/lib" + mod + ".so").c_str(), RTLD_LAZY | RTLD_LOCAL);
                BatchFn bf = h ? (BatchFn)dlsym(h, "executeTickBatch") : nullptr;
                if (!bf) continue;
                std::vector<uint64_t> bin(nIn ? nIn : 1, 0x9e3779b97f4a7c15ull), bout(nOut ? nOut : 1, 0);
                Result rb = timeIt([&]{ bf(bin.data(), bout.data()); return (uint64_t)bout[0]; }, TARGET);
                double perVec = rb.nsPerTick / 64.0;
                double rawns = 0.0;
                for (const Row& r : rows) if (r.name == name) rawns = r.raw.nsPerTick;
                std::printf("  %-10s %14.1f %16.3f %11.1fx\n", name.c_str(), rawns, perVec, rawns > 0 ? rawns / perVec : 0.0);
                std::printf("%s%s,%d,%d,bitsliced-per-vector,%.0f,%.3f,%.2f%s\n", A_DIM,
                            name.c_str(), nIn, nOut, rb.ticksPerSec * 64.0, perVec, rawns > 0 ? rawns / perVec : 0.0, A_RST);
        }

        for (const std::string& mod : g_builtModules)
        {
                unloadCompiledPart(mod);
                std::error_code ec;
                fs::remove("parts/lib" + mod + ".so", ec);
                fs::remove("parts/lib" + mod + ".dll", ec);
                fs::remove_all("parts/" + mod, ec);
        }

        std::printf("\n%s%s---------------------------------------------%s\n", A_BOLD, A_CYAN, A_RST);
        if (g_failures == 0)
                std::printf("%s%sPERF OK%s  all modes agree with interpreted and native beats interpreted\n", A_BOLD, A_GREEN, A_RST);
        else
                std::printf("%s%sPERF FAILED%s  %d check(s) failed\n", A_BOLD, A_RED, A_RST, g_failures);
        std::printf("%s%s---------------------------------------------%s\n\n", A_BOLD, A_CYAN, A_RST);
        return g_failures == 0 ? 0 : 1;
}
