#include "common.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

#include <raylib/raylib.h>

#include "../appstate.h"
#include "../part.h"
#include "../config.h"
#include "../compiler/compiler.h"

namespace
{
        template <class Step>
        double timeNsPerTick(Step step, double targetSec)
        {
                volatile uint64_t sink = 0;
                for (int i = 0; i < 500; ++i) sink += step();

                unsigned long long total = 0, batch = 256;
                double elapsed = 0.0;
                while (elapsed < targetSec)
                {
                        auto t0 = std::chrono::steady_clock::now();
                        for (unsigned long long i = 0; i < batch; ++i) sink += step();
                        auto t1 = std::chrono::steady_clock::now();
                        double dt = std::chrono::duration<double>(t1 - t0).count();
                        elapsed += dt;
                        total += batch;
                        if (dt > 0.0)
                        {
                                double rate = (double)batch / dt;
                                batch = (unsigned long long)(rate * (targetSec / 4.0));
                                if (batch < 256) batch = 256;
                        }
                }
                (void)sink;
                return 1e9 * elapsed / (double)total;
        }

        uint64_t foldOutputs(const std::vector<State>& o)
        {
                uint64_t a = 0;
                for (State s : o) a += (s == STATE_HIGH);
                return a;
        }

        int nativeOutputCount(const AppState& state)
        {
                int n = 0;
                for (std::map<int, PartType>::const_iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
                        if (it->second == PART_TYPE_OUTPUT)
                                n += state.inputCounts.count(it->first) ? state.inputCounts.at(it->first) : 0;
                return n;
        }

        bool benchNative(AppState& state, bool linkMode, const std::vector<State>& input,
                         int outCount, double targetSec, double& nsOut)
        {
                std::string mod = linkMode ? "__bench_link" : "__bench_inline";
                bool ok = false;

                std::string code = transpileToCpp(state, linkMode);
                if (compileSharedLibrary(code, mod))
                {
                        Part np = loadCompiledPart(mod, outCount);
                        if (np)
                        {
                                nsOut = timeNsPerTick([&] { return foldOutputs(np(input)); }, targetSec);
                                ok = true;
                        }
                        unloadCompiledPart(mod);
                }

                std::error_code ec;
                std::filesystem::remove("parts/lib" + mod + ".so", ec);
                std::filesystem::remove("parts/lib" + mod + ".dll", ec);
                return ok;
        }
}

void runBenchmark(AppState& state)
{
        BenchmarkResult r;

        const double target = 0.15;

        Part savedSim = state.simulation;
        state.simulation = nullptr;
        recompileSimulation(state);
        Part interp = state.simulation;
        std::vector<State> input = state.runtimeInput;
        state.simulation = savedSim;

        if (interp)
                r.interpNs = timeNsPerTick([&] { return foldOutputs(interp(input)); }, target);

        int outCount = nativeOutputCount(state);
        r.inlineOk = benchNative(state, false, input, outCount, target, r.inlineNs);
        r.linkOk = benchNative(state, true,  input, outCount, target, r.linkNs);

        r.valid = true;
        state.benchmark = r;
}

namespace
{
        constexpr float BENCH_W = 380.0f;
        constexpr float BENCH_H = 210.0f;
        constexpr int   BENCH_HEADER = 20;
        constexpr int   BENCH_TEXT = 10;
        constexpr float BENCH_LINE = 16.0f;

        std::string fmtRate(double ns)
        {
                double tps = (ns > 0.0) ? 1e9 / ns : 0.0;
                if (tps >= 1e6) return std::format("{:.1f} M/s", tps / 1e6);
                if (tps >= 1e3) return std::format("{:.1f} k/s", tps / 1e3);
                return std::format("{:.0f} /s", tps);
        }
}

void drawBenchmark(AppState& state)
{
        if (!state.showBenchmark) return;

        Color uiBg   = getThemeColor(state, COLOR_UI_BG_LIGHT, COLOR_UI_BG_DARK);
        Color textC  = getThemeColor(state, COLOR_TEXT_LIGHT, COLOR_TEXT_DARK);

        float bx = (GetScreenWidth() - BENCH_W) / 2.0f;
        float by = (GetScreenHeight() - BENCH_H) / 2.0f;

        DrawRectangle(bx - HELP_SHADOW_OFFSET, by - HELP_SHADOW_OFFSET,
                      BENCH_W + HELP_SHADOW_OFFSET * 2, BENCH_H + HELP_SHADOW_OFFSET * 2, DARKGRAY);
        DrawRectangle(bx, by, BENCH_W, BENCH_H, uiBg);

        float x = bx + HELP_PADDING;
        float y = by + HELP_PADDING;
        DrawText("BENCHMARK (Press B to toggle)", x, y, BENCH_HEADER, textC);
        y += 28;
        DrawText("-----------------------------------", x, y, BENCH_HEADER, GRAY);
        y += 20;

        const BenchmarkResult& r = state.benchmark;
        if (!r.valid)
        {
                DrawText("Benchmarking current circuit...", x, y, BENCH_HEADER, DARKBLUE);
                y += HELP_SECTION_SPACING;
                DrawText("Compiling native engines, timing each mode.", x, y, BENCH_TEXT, textC);
                return;
        }

        DrawText("mode", x, y, BENCH_TEXT, DARKBLUE);
        DrawText("ns/tick", x + 150, y, BENCH_TEXT, DARKBLUE);
        DrawText("throughput", x + 230, y, BENCH_TEXT, DARKBLUE);
        DrawText("vs interp", x + 320, y, BENCH_TEXT, DARKBLUE);
        y += BENCH_LINE + 2;

        auto row = [&](const char* label, bool ok, double ns) {
                DrawText(label, x, y, BENCH_TEXT, textC);
                if (!ok || ns <= 0.0)
                {
                        DrawText("n/a", x + 150, y, BENCH_TEXT, GRAY);
                }
                else
                {
                        DrawText(std::format("{:.1f}", ns).c_str(), x + 150, y, BENCH_TEXT, textC);
                        DrawText(fmtRate(ns).c_str(), x + 230, y, BENCH_TEXT, textC);
                        if (r.interpNs > 0.0)
                        {
                                double sp = r.interpNs / ns;
                                DrawText(std::format("{:.1f}x", sp).c_str(), x + 320, y, BENCH_TEXT,
                                         sp >= 1.0 ? DARKGREEN : RED);
                        }
                }
                y += BENCH_LINE;
        };

        row("interpreted",   r.interpNs > 0.0, r.interpNs);
        row("native-inline", r.inlineOk,       r.inlineNs);
        row("native-link",   r.linkOk,         r.linkNs);

        y += 8;
        DrawText("One tick = one full evaluation of the circuit.", x, y, BENCH_TEXT, GRAY);
        y += BENCH_LINE;
        if (!r.inlineOk && !r.linkOk)
                DrawText("Native compile failed (is clang++ installed?).", x, y, BENCH_TEXT, RED);
}
