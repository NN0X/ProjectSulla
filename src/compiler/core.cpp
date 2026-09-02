#include "compiler.h"

#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <functional>
#include <cmath>
#include <fstream>
#include <filesystem>

#include <glaze/glaze.hpp>

#include "../part.h"

struct FlatCircuit
{
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<int, int> inputCounts;
        std::map<int, int> outputCounts;
        std::map<int, std::string> labels;
};

struct LSCPin { int id; int pin; };
struct LSConn { LSCPin from; LSCPin to; };
struct LSPart { int id; PartType type; std::string label; float x; float y; int numInputs; int numOutputs; };
struct LLayoutData { std::vector<LSPart> parts; std::vector<LSConn> connections; };

static bool readLayout(const std::string& label, LLayoutData& out)
{
        std::ifstream file("layouts/" + label + ".json");
        if (!file.is_open()) return false;
        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (glz::read_json(out, json)) return false;
        return true;
}

enum CustomMode { CUSTOM_NONE, CUSTOM_INLINE, CUSTOM_LINK };

static CustomMode decideCustom(const std::string& label, bool linkMode)
{
#ifdef _WIN32
        bool lib = std::filesystem::exists("parts/lib" + label + ".dll");
#else
        bool lib = std::filesystem::exists("parts/lib" + label + ".so");
#endif
        bool layout = std::filesystem::exists("layouts/" + label + ".json");
        if (linkMode && lib) return CUSTOM_LINK;
        if (layout) return CUSTOM_INLINE;
        if (lib) return CUSTOM_LINK;
        return CUSTOM_NONE;
}

static std::vector<PartPin> linkCustom(FlatCircuit& fc, int nodeId, const std::string& label,
                                       int inC, int outC, const std::vector<PartPin>& inputProducers)
{
        fc.partTypes[nodeId] = PART_TYPE_CUSTOM;
        fc.inputCounts[nodeId] = inC;
        fc.outputCounts[nodeId] = outC;
        fc.labels[nodeId] = label;
        for (int q = 0; q < inC && q < (int)inputProducers.size(); ++q)
                if (inputProducers[q].first != -1) fc.connections[{nodeId, q}] = inputProducers[q];

        std::vector<PartPin> outs(outC);
        for (int p = 0; p < outC; ++p) outs[p] = {nodeId, p};
        return outs;
}

static std::vector<PartPin> expandCustom(FlatCircuit& fc, int& idAlloc,
                                         const std::string& label,
                                         const std::vector<PartPin>& inputProducers,
                                         bool linkMode)
{
        std::vector<PartPin> outProducers;

        LLayoutData ld;
        if (!readLayout(label, ld)) return outProducers;

        std::map<int, const LSPart*> byId;
        for (const LSPart& p : ld.parts) byId[p.id] = &p;

        auto posLess = [&](int a, int b) {
                const LSPart* pa = byId[a];
                const LSPart* pb = byId[b];
                if (std::fabs(pa->y - pb->y) > 0.1f) return pa->y < pb->y;
                return pa->x < pb->x;
        };

        std::vector<int> sources, outputs;
        for (const LSPart& p : ld.parts)
        {
                if (p.type == PART_TYPE_SOURCE) sources.push_back(p.id);
                else if (p.type == PART_TYPE_OUTPUT) outputs.push_back(p.id);
        }
        std::sort(sources.begin(), sources.end(), posLess);
        std::sort(outputs.begin(), outputs.end(), posLess);

        std::map<PartPin, int> srcPinExt;
        int extIdx = 0;
        for (int s : sources)
        {
                int pins = byId[s]->numOutputs > 0 ? byId[s]->numOutputs : 1;
                for (int j = 0; j < pins; ++j) srcPinExt[{s, j}] = extIdx++;
        }

        std::map<PartPin, PartPin> conn;
        for (const LSConn& c : ld.connections) conn[{c.to.id, c.to.pin}] = {c.from.id, c.from.pin};

        std::map<int, int> gateFlat;
        for (const LSPart& p : ld.parts)
        {
                if (p.type == PART_TYPE_SOURCE || p.type == PART_TYPE_OUTPUT ||
                    p.type == PART_TYPE_DISPLAY || p.type == PART_TYPE_CUSTOM) continue;
                int fid = idAlloc++;
                gateFlat[p.id] = fid;
                fc.partTypes[fid] = p.type;
                fc.inputCounts[fid] = p.numInputs;
                fc.outputCounts[fid] = p.numOutputs > 0 ? p.numOutputs : 1;
        }

        std::map<int, std::vector<PartPin>> customOut;
        std::set<int> expanding;
        std::function<PartPin(int, int)> resolve = [&](int id, int pin) -> PartPin
        {
                std::map<int, const LSPart*>::iterator t = byId.find(id);
                if (t == byId.end()) return {-1, -1};

                switch (t->second->type)
                {
                case PART_TYPE_SOURCE:
                {
                        std::map<PartPin, int>::iterator e = srcPinExt.find({id, pin});
                        if (e != srcPinExt.end() && e->second < (int)inputProducers.size()) return inputProducers[e->second];
                        return {-1, -1};
                }
                case PART_TYPE_OUTPUT:
                case PART_TYPE_DISPLAY:
                        return {-1, -1};
                case PART_TYPE_CUSTOM:
                {
                        if (!customOut.count(id) && !expanding.count(id))
                        {
                                expanding.insert(id);
                                int inC = t->second->numInputs;
                                int outC = t->second->numOutputs > 0 ? t->second->numOutputs : 1;
                                std::string clabel = t->second->label;
                                std::vector<PartPin> childIn(inC, PartPin{-1, -1});
                                for (int q = 0; q < inC; ++q)
                                {
                                        std::map<PartPin, PartPin>::iterator ci = conn.find({id, q});
                                        if (ci != conn.end()) childIn[q] = resolve(ci->second.first, ci->second.second);
                                }
                                if (decideCustom(clabel, linkMode) == CUSTOM_LINK)
                                        customOut[id] = linkCustom(fc, idAlloc++, clabel, inC, outC, childIn);
                                else
                                        customOut[id] = expandCustom(fc, idAlloc, clabel, childIn, linkMode);
                                expanding.erase(id);
                        }
                        std::vector<PartPin>& o = customOut[id];
                        return pin < (int)o.size() ? o[pin] : PartPin{-1, -1};
                }
                default:
                {
                        std::map<int, int>::iterator g = gateFlat.find(id);
                        return g != gateFlat.end() ? PartPin{g->second, pin} : PartPin{-1, -1};
                }
                }
        };

        for (const LSPart& p : ld.parts)
        {
                std::map<int, int>::iterator gf = gateFlat.find(p.id);
                if (gf == gateFlat.end()) continue;
                for (int q = 0; q < p.numInputs; ++q)
                {
                        std::map<PartPin, PartPin>::iterator ci = conn.find({p.id, q});
                        if (ci == conn.end()) continue;
                        PartPin prod = resolve(ci->second.first, ci->second.second);
                        if (prod.first != -1) fc.connections[{gf->second, q}] = prod;
                }
        }

        outProducers.assign(outputs.size(), PartPin{-1, -1});
        for (size_t i = 0; i < outputs.size(); ++i)
        {
                std::map<PartPin, PartPin>::iterator ci = conn.find({outputs[i], 0});
                if (ci != conn.end()) outProducers[i] = resolve(ci->second.first, ci->second.second);
        }
        return outProducers;
}

static FlatCircuit flattenState(const AppState& state, bool linkMode)
{
        FlatCircuit fc;

        int idAlloc = 1000000;
        for (std::map<int, PartType>::const_iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
                if (it->first >= idAlloc) idAlloc = it->first + 1;

        for (std::map<int, PartType>::const_iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                if (it->second == PART_TYPE_CUSTOM) continue;
                int id = it->first;
                fc.partTypes[id] = it->second;
                fc.inputCounts[id] = state.inputCounts.count(id) ? state.inputCounts.at(id) : 0;
                fc.outputCounts[id] = state.outputCounts.count(id) ? state.outputCounts.at(id) : 0;
        }

        std::map<int, std::vector<PartPin>> customOut;
        std::set<int> expanding;
        std::function<PartPin(int, int)> resolve = [&](int id, int pin) -> PartPin
        {
                std::map<int, PartType>::const_iterator t = state.partTypes.find(id);
                if (t == state.partTypes.end()) return {-1, -1};
                if (t->second != PART_TYPE_CUSTOM) return {id, pin};

                if (!customOut.count(id) && !expanding.count(id))
                {
                        expanding.insert(id);
                        int inC = state.inputCounts.count(id) ? state.inputCounts.at(id) : 0;
                        int outC = state.outputCounts.count(id) ? state.outputCounts.at(id) : 0;
                        std::string label = state.labels.count(id) ? state.labels.at(id) : "";
                        std::vector<PartPin> childIn(inC, PartPin{-1, -1});
                        for (int q = 0; q < inC; ++q)
                        {
                                std::map<PartPin, PartPin>::const_iterator ci = state.connections.find({id, q});
                                if (ci != state.connections.end()) childIn[q] = resolve(ci->second.first, ci->second.second);
                        }
                        if (decideCustom(label, linkMode) == CUSTOM_LINK)
                                customOut[id] = linkCustom(fc, id, label, inC, outC, childIn);
                        else
                                customOut[id] = expandCustom(fc, idAlloc, label, childIn, linkMode);
                        expanding.erase(id);
                }
                std::vector<PartPin>& o = customOut[id];
                return pin < (int)o.size() ? o[pin] : PartPin{-1, -1};
        };

        for (std::map<PartPin, PartPin>::const_iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                int toId = it->first.first;
                std::map<int, PartType>::const_iterator tt = state.partTypes.find(toId);
                if (tt != state.partTypes.end() && tt->second == PART_TYPE_CUSTOM) continue; // absorbed by expansion
                PartPin prod = resolve(it->second.first, it->second.second);
                if (prod.first != -1) fc.connections[{toId, it->first.second}] = prod;
        }

        return fc;
}

static void resolvePassThrough(FlatCircuit& fc)
{
        std::map<PartPin, PartPin> updated;
        std::vector<PartPin> drop;
        for (std::map<PartPin, PartPin>::iterator it = fc.connections.begin(); it != fc.connections.end(); ++it)
        {
                PartPin prod = it->second;
                std::set<PartPin> seen;
                while (true)
                {
                        std::map<int, PartType>::iterator t = fc.partTypes.find(prod.first);
                        if (t == fc.partTypes.end()) break;
                        if (t->second != PART_TYPE_OUTPUT && t->second != PART_TYPE_DISPLAY) break;
                        if (!seen.insert(prod).second) break;
                        std::map<PartPin, PartPin>::iterator up = fc.connections.find(prod);
                        if (up == fc.connections.end()) break;
                        prod = up->second;
                }
                std::map<int, PartType>::iterator t = fc.partTypes.find(prod.first);
                if (t != fc.partTypes.end() && (t->second == PART_TYPE_OUTPUT || t->second == PART_TYPE_DISPLAY))
                        drop.push_back(it->first);
                else
                        updated[it->first] = prod;
        }
        for (std::map<PartPin, PartPin>::iterator it = updated.begin(); it != updated.end(); ++it) fc.connections[it->first] = it->second;
        for (size_t i = 0; i < drop.size(); ++i) fc.connections.erase(drop[i]);
}

static std::string emitCpp(const FlatCircuit& c)
{
        std::ostringstream code;
        code << "#include <stdint.h>\n";

        bool hasLinked = false;
        for (std::map<int, PartType>::const_iterator it = c.partTypes.begin(); it != c.partTypes.end(); ++it)
                if (it->second == PART_TYPE_CUSTOM) hasLinked = true;
        if (hasLinked)
        {
                code << "#include <stdio.h>\n";
                code << "#ifdef _WIN32\n";
                code << "#include <windows.h>\n";
                code << "#define SULLA_EXT \".dll\"\n";
                code << "#else\n";
                code << "#include <dlfcn.h>\n";
                code << "#include <unistd.h>\n";
                code << "#define SULLA_EXT \".so\"\n";
                code << "#endif\n";
                code << "typedef void (*sulla_fn)(const uint8_t*, uint8_t*);\n";
                code << "static void* sulla_load_unique(const char* base) {\n";
                code << "        static unsigned long ctr = 0;\n";
                code << "        char src[512], tmp[512];\n";
                code << "        snprintf(src, sizeof(src), \"%s%s\", base, SULLA_EXT);\n";
                code << "#ifdef _WIN32\n";
                code << "        char dir[512]; GetTempPathA((DWORD)sizeof(dir), dir);\n";
                code << "        snprintf(tmp, sizeof(tmp), \"%ssulla_%lu_%p_%lu.dll\", dir, (unsigned long)GetCurrentProcessId(), (void*)&ctr, ctr++);\n";
                code << "#else\n";
                code << "        snprintf(tmp, sizeof(tmp), \"/tmp/sulla_%ld_%p_%lu.so\", (long)getpid(), (void*)&ctr, ctr++);\n";
                code << "#endif\n";
                code << "        FILE* in = fopen(src, \"rb\"); if (!in) return 0;\n";
                code << "        FILE* out = fopen(tmp, \"wb\"); if (!out) { fclose(in); return 0; }\n";
                code << "        char buf[65536]; size_t n;\n";
                code << "        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);\n";
                code << "        fclose(in); fclose(out);\n";
                code << "#ifdef _WIN32\n";
                code << "        return (void*)LoadLibraryA(tmp);\n";
                code << "#else\n";
                code << "        void* h = dlopen(tmp, RTLD_LAZY | RTLD_LOCAL); remove(tmp); return h;\n";
                code << "#endif\n";
                code << "}\n";
                code << "static sulla_fn sulla_sym(void* h) {\n";
                code << "#ifdef _WIN32\n";
                code << "        return h ? (sulla_fn)GetProcAddress((HMODULE)h, \"executeTick\") : 0;\n";
                code << "#else\n";
                code << "        return h ? (sulla_fn)dlsym(h, \"executeTick\") : 0;\n";
                code << "#endif\n";
                code << "}\n";
        }
        code << "\n";

        for (std::map<int, PartType>::const_iterator it = c.partTypes.begin(); it != c.partTypes.end(); ++it)
        {
                int id = it->first;
                int outC = c.outputCounts.count(id) ? c.outputCounts.at(id) : 0;
                for (int p = 0; p < outC; ++p) code << "static uint8_t n_" << id << "_out_" << p << " = 0;\n";
                if (it->second == PART_TYPE_CLOCK) code << "static uint8_t clk_" << id << " = 0;\n";
        }

        code << "\nextern \"C\" {\n";
        code << "void executeTick(const uint8_t* in, uint8_t* out) {\n";

        std::vector<int> sources;
        std::vector<int> outputs;
        std::map<int, std::vector<int>> adj;
        std::map<int, int> inDegree;

        for (std::map<int, PartType>::const_iterator it = c.partTypes.begin(); it != c.partTypes.end(); ++it)
        {
                int id = it->first;
                inDegree[id] = 0;
                if (it->second == PART_TYPE_SOURCE) sources.push_back(id);
                if (it->second == PART_TYPE_OUTPUT) outputs.push_back(id);
        }

        std::sort(sources.begin(), sources.end());
        std::sort(outputs.begin(), outputs.end());

        for (std::map<PartPin, PartPin>::const_iterator it = c.connections.begin(); it != c.connections.end(); ++it)
        {
                int toID = it->first.first;
                int fromID = it->second.first;
                adj[fromID].push_back(toID);
                inDegree[toID]++;
        }

        std::vector<int> q;
        for (std::map<int, int>::iterator it = inDegree.begin(); it != inDegree.end(); ++it)
                if (it->second == 0) q.push_back(it->first);

        std::vector<int> order;
        std::map<int, bool> emitted;
        while (!q.empty())
        {
                int u = q.front();
                q.erase(q.begin());
                order.push_back(u);
                emitted[u] = true;
                for (size_t i = 0; i < adj[u].size(); ++i)
                {
                        int v = adj[u][i];
                        inDegree[v]--;
                        if (inDegree[v] == 0) q.push_back(v);
                }
        }

        for (std::map<int, PartType>::const_iterator it = c.partTypes.begin(); it != c.partTypes.end(); ++it)
        {
                int id = it->first;
                PartType type = it->second;
                if (type != PART_TYPE_SOURCE && type != PART_TYPE_OUTPUT && type != PART_TYPE_DISPLAY && !emitted[id])
                        order.push_back(id);
        }

        int inIdx = 0;
        for (size_t i = 0; i < sources.size(); ++i)
        {
                int id = sources[i];
                int outC = c.outputCounts.count(id) ? c.outputCounts.at(id) : 0;
                for (int p = 0; p < outC; ++p) code << "        n_" << id << "_out_" << p << " = in[" << inIdx++ << "];\n";
        }

        for (size_t i = 0; i < order.size(); ++i)
        {
                int u = order[i];
                PartType type = c.partTypes.count(u) ? c.partTypes.at(u) : PART_TYPE_CUSTOM;
                if (type == PART_TYPE_SOURCE || type == PART_TYPE_OUTPUT || type == PART_TYPE_DISPLAY) continue;

                int inC = c.inputCounts.count(u) ? c.inputCounts.at(u) : 0;
                int outC = c.outputCounts.count(u) ? c.outputCounts.at(u) : 0;
                std::vector<std::string> inVars(inC, "0");
                for (int p = 0; p < inC; ++p)
                {
                        std::map<PartPin, PartPin>::const_iterator cit = c.connections.find({u, p});
                        if (cit != c.connections.end())
                                inVars[p] = "n_" + std::to_string(cit->second.first) + "_out_" + std::to_string(cit->second.second);
                }

                if (inC == 0 && type != PART_TYPE_CUSTOM && type != PART_TYPE_CLOCK) continue;

                if (type == PART_TYPE_AND)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " &= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_OR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " |= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_NOT)
                {
                        code << "        n_" << u << "_out_0 = !" << inVars[0] << ";\n";
                }
                else if (type == PART_TYPE_NAND)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " &= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_NOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " |= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_XOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " ^= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_XNOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " ^= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_CLOCK)
                {
                        code << "        clk_" << u << " = !clk_" << u << ";\n";
                        code << "        n_" << u << "_out_0 = clk_" << u << ";\n";
                }
                else if (type == PART_TYPE_CUSTOM)
                {
                        std::string lib = c.labels.count(u) ? c.labels.at(u) : "";
                        code << "        {\n";
                        code << "                static void* h = sulla_load_unique(\"./parts/lib" << lib << "\");\n";
                        code << "                static sulla_fn fn = sulla_sym(h);\n";
                        code << "                uint8_t ci[" << (inC > 0 ? inC : 1) << "] = {0};\n";
                        code << "                uint8_t co[" << (outC > 0 ? outC : 1) << "] = {0};\n";
                        for (int p = 0; p < inC; ++p) code << "                ci[" << p << "] = " << inVars[p] << ";\n";
                        code << "                if (fn) fn(ci, co);\n";
                        for (int p = 0; p < outC; ++p) code << "                n_" << u << "_out_" << p << " = co[" << p << "];\n";
                        code << "        }\n";
                }
        }

        int outIdx = 0;
        for (size_t i = 0; i < outputs.size(); ++i)
        {
                int id = outputs[i];
                int inC = c.inputCounts.count(id) ? c.inputCounts.at(id) : 0;
                for (int p = 0; p < inC; ++p)
                {
                        std::map<PartPin, PartPin>::const_iterator cit = c.connections.find({id, p});
                        if (cit != c.connections.end())
                                code << "        out[" << outIdx++ << "] = n_" << cit->second.first << "_out_" << cit->second.second << ";\n";
                        else
                                code << "        out[" << outIdx++ << "] = 0;\n";
                }
        }

        code << "}\n}\n";
        return code.str();
}

std::string transpileToCpp(const AppState& state, bool linkCustomParts)
{
        FlatCircuit fc = flattenState(state, linkCustomParts);
        resolvePassThrough(fc);
        return emitCpp(fc);
}
