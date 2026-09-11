#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glaze/glaze.hpp>
#include "appstate.h"
#include "utils.h"
#include "compiler/compiler.h"

static void compileFixture(const std::string& name)
{
        std::string path = "layouts/" + name + ".json";
        if (!std::filesystem::exists(path)) { std::fprintf(stderr, "missing layout: %s\n", path.c_str()); return; }

        AppState s;
        loadLayout(s, path);
        std::string cpp = transpileToCpp(s, false);
        if (!compilePartLibrary(cpp, name, true, false))
        {
                std::fprintf(stderr, "compile failed: %s\n", name.c_str());
                return;
        }

        std::vector<int> srcs, outs;
        for (std::map<int, PartType>::iterator it = s.partTypes.begin(); it != s.partTypes.end(); ++it)
        {
                if (it->second == PART_TYPE_SOURCE) srcs.push_back(it->first);
                if (it->second == PART_TYPE_OUTPUT) outs.push_back(it->first);
        }
        auto pinLess = [&](int a, int b) -> bool {
                std::map<int, std::pair<float, float>>::iterator pa = s.positions.find(a);
                std::map<int, std::pair<float, float>>::iterator pb = s.positions.find(b);
                float xa = pa != s.positions.end() ? pa->second.first : 0.0f;
                float ya = pa != s.positions.end() ? pa->second.second : 0.0f;
                float xb = pb != s.positions.end() ? pb->second.first : 0.0f;
                float yb = pb != s.positions.end() ? pb->second.second : 0.0f;
                if (std::fabs(ya - yb) > 0.1f) return ya < yb;
                if (std::fabs(xa - xb) > 0.1f) return xa < xb;
                return a < b;
        };
        std::sort(srcs.begin(), srcs.end(), pinLess);
        std::sort(outs.begin(), outs.end(), pinLess);

        int inC = 0, outC = 0;
        std::vector<std::string> inL, outL;
        for (size_t i = 0; i < srcs.size(); ++i)
        {
                int pins = s.outputCounts[srcs[i]];
                std::string lbl = s.labels[srcs[i]];
                for (int p = 0; p < pins; ++p) inL.push_back(pins == 1 ? lbl : lbl + "[" + std::to_string(p) + "]");
                inC += pins;
        }
        for (size_t i = 0; i < outs.size(); ++i)
        {
                int pins = s.inputCounts[outs[i]];
                std::string lbl = s.labels[outs[i]];
                for (int p = 0; p < pins; ++p) outL.push_back(pins == 1 ? lbl : lbl + "[" + std::to_string(p) + "]");
                outC += pins;
        }

        std::string dir = sullaPartDir(name);
        std::filesystem::create_directories(dir);
        CompiledMeta meta{inC, outC, inL, outL};
        std::string json;
        if (!glz::write<glz::opts{.prettify = true}>(meta, json))
        {
                std::ofstream f(dir + "/" + name + ".json");
                if (f.is_open()) f << json;
        }
        std::printf("fixture compiled: %-12s  %d in / %d out\n", name.c_str(), inC, outC);
}

int main(int argc, char** argv)
{
        for (int i = 1; i < argc; ++i) compileFixture(argv[i]);
        return 0;
}
