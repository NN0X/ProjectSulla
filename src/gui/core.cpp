#include "gui.h"
#include "common.h"
#include "../compiler/compiler.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <utility>
#include <cmath>
#include <format>

#include <raylib/raylib.h>
#include <raylib/raymath.h>
#include <glaze/glaze.hpp>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"
#include "../compiler/compiler.h"

Color getThemeColor(const AppState& state, Color light, Color dark)
{
        return state.darkMode ? dark : light;
}

static float partTitleHeight(const AppState& state, int id)
{
        return (state.labels.count(id) && !state.labels.at(id).empty()) ? PART_TITLE_HEIGHT : 0.0f;
}

static float maxPinLabelWidth(const AppState& state, int id, bool isInput)
{
        const std::map<int, std::vector<std::string>>& src = isInput ? state.inputPinLabels : state.outputPinLabels;
        if (!src.count(id)) return 0.0f;
        float m = 0.0f;
        for (const std::string& l : src.at(id))
        {
                float w = (float)MeasureText(l.c_str(), PIN_LABEL_FONT_SIZE);
                if (w > m) m = w;
        }
        return m;
}

Vector2 getPartSize(const AppState& state, int id)
{
        float nameW = (float)MeasureText(state.labels.at(id).c_str(), 10);
        float leftMax = maxPinLabelWidth(state, id, true);
        float rightMax = maxPinLabelWidth(state, id, false);

        float w = BASE_PART_WIDTH;
        float wName = nameW + TEXT_PADDING * 2;
        if (wName > w) w = wName;
        if (leftMax > 0 || rightMax > 0)
        {
                float wPins = leftMax + rightMax + PIN_LABEL_INSET * 2 + PIN_LABEL_CENTER_GAP;
                if (wPins > w) w = wPins;
        }

        int inCount = state.inputCounts.at(id);
        int outCount = state.outputCounts.at(id);
        int maxPins = (inCount > outCount) ? inCount : outCount;
        float pinsH = (maxPins > 1) ? (float)(maxPins - 1) * PIN_SPACING + PIN_Y_OFFSET_BASE * 2 : PIN_Y_OFFSET_BASE * 2;
        float h = partTitleHeight(state, id) + pinsH;
        if (h < BASE_PART_HEIGHT) h = BASE_PART_HEIGHT;

        float ratioW = h * 0.35f;
        if (ratioW > 160.0f) ratioW = 160.0f;
        if (w < ratioW) w = ratioW;
        return {w, h};
}

Rectangle getBodyRect(const AppState& state, int id)
{
        Vector2 pos = {state.positions.at(id).first, state.positions.at(id).second};
        Vector2 size = getPartSize(state, id);
        return {pos.x - size.x/2, pos.y - size.y/2, size.x, size.y};
}

int getPinCount(const AppState& state, int id, bool isInput)
{
        if (isInput) return state.inputCounts.count(id) ? state.inputCounts.at(id) : 0;
        return state.outputCounts.count(id) ? state.outputCounts.at(id) : 0;
}

float getPinYOffset(const AppState& state, int id, bool isInput, int index)
{
        Vector2 size = getPartSize(state, id);
        int count = getPinCount(state, id, isInput);
        float top = -size.y/2 + partTitleHeight(state, id) + PIN_Y_OFFSET_BASE;
        float bot = size.y/2 - PIN_Y_OFFSET_BASE;
        if (count <= 1) return (top + bot) / 2.0f;
        float step = (bot - top) / (count - 1);
        return top + index * step;
}

Rectangle getPinRect(const AppState& state, int id, bool isInput, int index)
{
        Vector2 pos = {state.positions.at(id).first, state.positions.at(id).second};
        Vector2 size = getPartSize(state, id);
        float yOff = getPinYOffset(state, id, isInput, index);
        float x = isInput ? (pos.x - size.x/2 - PIN_SIZE) : (pos.x + size.x/2);
        return {x, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
}

Vector2 getPinPos(const AppState& state, int id, bool isInput, int index)
{
        Vector2 pos = {state.positions.at(id).first, state.positions.at(id).second};
        Vector2 size = getPartSize(state, id);
        float yOff = getPinYOffset(state, id, isInput, index);
        float x = isInput ? (pos.x - size.x/2 - PIN_SIZE) : (pos.x + size.x/2 + PIN_SIZE);
        return {x, pos.y + yOff};
}

void refreshLayouts(AppState& state)
{
        state.layoutFiles.clear();
        if (std::filesystem::exists("layouts"))
        {
                for (const auto& entry : std::filesystem::directory_iterator("layouts"))
                {
                        if (entry.path().extension() == ".json")
                        {
                                state.layoutFiles.push_back(entry.path().filename().string());
                        }
                }
        }
}

void refreshCompiledModules(AppState& state)
{
        state.compiledModules.clear();
        state.compiledInputs.clear();
        state.compiledOutputs.clear();
        state.compiledInputLabels.clear();
        state.compiledOutputLabels.clear();
        if (!std::filesystem::exists("parts")) return;

        for (const auto& entry : std::filesystem::directory_iterator("parts"))
        {
                if (!entry.is_directory()) continue;
                std::string modName = entry.path().filename().string();
                std::filesystem::path metaPath = entry.path() / (modName + ".json");
                if (!std::filesystem::exists(metaPath)) continue;

                std::ifstream file(metaPath);
                if (!file.is_open()) continue;
                std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                CompiledMeta meta{};
                if (glz::read_json(meta, json)) continue;
                state.compiledModules.push_back(modName);
                state.compiledInputs[modName] = meta.inputs;
                state.compiledOutputs[modName] = meta.outputs;
                state.compiledInputLabels[modName] = meta.inputLabels;
                state.compiledOutputLabels[modName] = meta.outputLabels;
        }
}

void initApp(AppState& state)
{
        setSourcePart(state.parts, state.rootSourceID);
        setSourcePart(state.parts, state.rootSinkID);
        refreshLayouts(state);
        refreshCompiledModules(state);
}

void tidyLayout(AppState& state)
{
        std::vector<int> ids;
        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                ids.push_back(it->first);
        if (ids.empty()) return;

        std::map<int, std::vector<int>> succ;
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                succ[it->second.first].push_back(it->first.first);

        std::map<int, int> color;
        for (size_t i = 0; i < ids.size(); ++i) color[ids[i]] = 0;
        std::set<std::pair<int, int>> back;
        for (size_t r = 0; r < ids.size(); ++r)
        {
                if (color[ids[r]] != 0) continue;
                std::vector<std::pair<int, size_t>> stk;
                stk.push_back(std::make_pair(ids[r], (size_t)0));
                color[ids[r]] = 1;
                size_t guard = 0, guardMax = ids.size() * 8 + 16;
                while (!stk.empty() && guard++ < guardMax)
                {
                        int node = stk.back().first;
                        bool advanced = false;
                        std::map<int, std::vector<int>>::iterator sit = succ.find(node);
                        if (sit != succ.end())
                        {
                                while (stk.back().second < sit->second.size())
                                {
                                        int w = sit->second[stk.back().second++];
                                        if (!color.count(w)) continue;
                                        if (color[w] == 1) back.insert(std::make_pair(node, w));
                                        else if (color[w] == 0) { color[w] = 1; stk.push_back(std::make_pair(w, (size_t)0)); advanced = true; break; }
                                }
                        }
                        if (!advanced) { color[node] = 2; stk.pop_back(); }
                }
        }

        std::map<int, int> depth;
        for (size_t i = 0; i < ids.size(); ++i) depth[ids[i]] = 0;
        std::vector<std::pair<int, int>> dag;
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                std::pair<int, int> e = std::make_pair(it->second.first, it->first.first);
                if (!back.count(e)) dag.push_back(e);
        }
        for (size_t pass = 0; pass < ids.size(); ++pass)
        {
                bool changed = false;
                for (size_t k = 0; k < dag.size(); ++k)
                        if (depth.count(dag[k].first) && depth.count(dag[k].second) && depth[dag[k].second] < depth[dag[k].first] + 1)
                        { depth[dag[k].second] = depth[dag[k].first] + 1; changed = true; }
                if (!changed) break;
        }

        int gmax = 0;
        for (size_t i = 0; i < ids.size(); ++i)
        {
                PartType t = state.partTypes[ids[i]];
                if (t != PART_TYPE_SOURCE && t != PART_TYPE_OUTPUT && depth[ids[i]] > gmax) gmax = depth[ids[i]];
        }
        std::map<int, std::vector<int>> cols;
        for (size_t i = 0; i < ids.size(); ++i)
        {
                PartType t = state.partTypes[ids[i]];
                int c = (t == PART_TYPE_SOURCE) ? 0 : (t == PART_TYPE_OUTPUT) ? (gmax + 2) : std::max(1, depth[ids[i]]);
                cols[c].push_back(ids[i]);
        }
        const float COLDX = 190.0f, ROWDY = 96.0f;
        for (std::map<int, std::vector<int>>::iterator it = cols.begin(); it != cols.end(); ++it)
        {
                std::vector<int>& g = it->second;
                std::sort(g.begin(), g.end(), [&](int a, int b) { return state.positions[a].second < state.positions[b].second; });
                for (size_t r = 0; r < g.size(); ++r)
                        state.positions[g[r]] = std::make_pair((float)it->first * COLDX, (float)r * ROWDY);
        }
}

static size_t circuitHash(const AppState& state)
{
        size_t h = 1469598103934665603ULL;
        auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ULL; };
        for (std::map<int, PartType>::const_iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                mix((size_t)it->first); mix((size_t)it->second);
                std::map<int, int>::const_iterator ic = state.inputCounts.find(it->first);
                std::map<int, int>::const_iterator oc = state.outputCounts.find(it->first);
                mix(ic != state.inputCounts.end() ? (size_t)ic->second : 0);
                mix(oc != state.outputCounts.end() ? (size_t)oc->second : 0);
                std::map<int, std::pair<float, float>>::const_iterator pp = state.positions.find(it->first);
                if (pp != state.positions.end()) { mix((size_t)(long)(pp->second.first + 0.5f)); mix((size_t)(long)(pp->second.second + 0.5f)); }
                std::map<int, std::string>::const_iterator lb = state.labels.find(it->first);
                if (lb != state.labels.end()) mix(std::hash<std::string>{}(lb->second));
        }
        for (std::map<PartPin, PartPin>::const_iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                mix((size_t)it->first.first); mix((size_t)it->first.second);
                mix((size_t)it->second.first); mix((size_t)it->second.second);
        }
        for (std::map<PartPin, std::vector<Vector2>>::const_iterator it = state.connectionWaypoints.begin(); it != state.connectionWaypoints.end(); ++it)
        {
                mix((size_t)it->first.first); mix((size_t)it->first.second);
                for (Vector2 wp : it->second) { mix((size_t)(long)wp.x); mix((size_t)(long)wp.y); }
        }
        return h;
}

CircuitSnapshot takeSnapshot(const AppState& s)
{
        CircuitSnapshot c;
        c.connectionWaypoints = s.connectionWaypoints;
        c.partTypes = s.partTypes; c.connections = s.connections; c.connColorIdx = s.connColorIdx; c.nextConnColor = s.nextConnColor;
        c.labels = s.labels; c.positions = s.positions; c.inputCounts = s.inputCounts; c.outputCounts = s.outputCounts;
        c.sourceValues = s.sourceValues; c.nextID = s.nextID;
        return c;
}

void applySnapshot(AppState& s, const CircuitSnapshot& c)
{
        s.connectionWaypoints = c.connectionWaypoints;
        s.partTypes = c.partTypes; s.connections = c.connections; s.connColorIdx = c.connColorIdx; s.nextConnColor = c.nextConnColor;
        s.labels = c.labels; s.positions = c.positions; s.inputCounts = c.inputCounts; s.outputCounts = c.outputCounts;
        s.sourceValues = c.sourceValues; s.nextID = c.nextID;
        s.simulation = nullptr;
        s.selectedConnection = {-1, -1};
        s.selectedParts.clear();
        s.wireStartPartID = -1;
}

void updateHistory(AppState& s)
{
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) return;
        size_t h = circuitHash(s);
        if (!s.histInit) { s.histLast = takeSnapshot(s); s.histHash = h; s.histInit = true; return; }
        if (h != s.histHash)
        {
                s.undoStack.push_back(s.histLast);
                if (s.undoStack.size() > 200) s.undoStack.erase(s.undoStack.begin());
                s.redoStack.clear();
                s.histLast = takeSnapshot(s);
                s.histHash = h;
        }
}

void doUndo(AppState& s)
{
        if (s.undoStack.empty()) return;
        s.redoStack.push_back(takeSnapshot(s));
        applySnapshot(s, s.undoStack.back());
        s.undoStack.pop_back();
        s.histLast = takeSnapshot(s);
        s.histHash = circuitHash(s);
}

void doRedo(AppState& s)
{
        if (s.redoStack.empty()) return;
        s.undoStack.push_back(takeSnapshot(s));
        applySnapshot(s, s.redoStack.back());
        s.redoStack.pop_back();
        s.histLast = takeSnapshot(s);
        s.histHash = circuitHash(s);
}

bool buildNativeSimulation(AppState& state)
{
        std::vector<int> srcs, outs;
        for (std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                if (it->second == PART_TYPE_SOURCE) srcs.push_back(it->first);
                if (it->second == PART_TYPE_OUTPUT || it->second == PART_TYPE_DISPLAY) outs.push_back(it->first);
        }
        if (outs.empty()) return false;

        std::map<int, int> gIn; int gi = 0;
        for (std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it) { gIn[it->first] = gi; gi += (int)it->second.size(); }
        std::map<int, int> gOut; int go = 0;
        for (std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
                if (it->second == PART_TYPE_OUTPUT || it->second == PART_TYPE_DISPLAY) { gOut[it->first] = go; go += state.inputCounts[it->first]; }
        int totalIn = gi, totalOut = go;

        auto posLess = [&](int a, int b) -> bool {
                std::pair<float, float> pa = state.positions[a], pb = state.positions[b];
                if (std::fabs(pa.second - pb.second) > 0.1f) return pa.second < pb.second;
                if (std::fabs(pa.first - pb.first) > 0.1f) return pa.first < pb.first;
                return a < b;
        };
        std::vector<int> sp = srcs, op = outs;
        std::sort(sp.begin(), sp.end(), posLess);
        std::sort(op.begin(), op.end(), posLess);
        std::map<int, int> nIn; int ni = 0; for (size_t i = 0; i < sp.size(); ++i) { nIn[sp[i]] = ni; ni += state.outputCounts[sp[i]]; }
        std::map<int, int> nOut; int no = 0; for (size_t i = 0; i < op.size(); ++i) { nOut[op[i]] = no; no += state.inputCounts[op[i]]; }

        std::vector<int> inRemap(totalIn, 0), outRemap(totalOut, 0);
        for (size_t k = 0; k < srcs.size(); ++k) { int s = srcs[k], c = state.outputCounts[s]; for (int i = 0; i < c; ++i) if (nIn[s] + i < totalIn) inRemap[nIn[s] + i] = gIn[s] + i; }
        for (size_t k = 0; k < outs.size(); ++k) { int o = outs[k], c = state.inputCounts[o]; for (int i = 0; i < c; ++i) if (gOut[o] + i < totalOut) outRemap[gOut[o] + i] = nOut[o] + i; }

        size_t h = circuitHash(state);
        bool soReady = (h == state.nativeHash) && !sullaFindDynamic("__live__").empty();
        if (!soReady)
        {
                std::string cpp = transpileToCpp(state, false);
                if (!compilePartLibrary(cpp, "__live__", false, true)) return false;
                state.nativeHash = h;
        }
        Part native = loadCompiledPart("__live__", totalOut);
        if (!native) return false;

        state.simulation = [native, inRemap, outRemap, totalIn, totalOut](std::vector<State> runtimeInput) -> std::vector<State> {
                std::vector<State> soIn(totalIn, STATE_LOW);
                for (int j = 0; j < totalIn; ++j) { int g = inRemap[j]; if (g >= 0 && g < (int)runtimeInput.size()) soIn[j] = runtimeInput[g]; }
                std::vector<State> soOut = native(soIn);
                std::vector<State> guiOut(totalOut, STATE_LOW);
                for (int j = 0; j < totalOut; ++j) { int n = outRemap[j]; if (n >= 0 && n < (int)soOut.size()) guiOut[j] = soOut[n]; }
                return guiOut;
        };
        state.nativeActive = true;
        return true;
}

void recompileSimulation(AppState& state)
{
        std::map<int, Part> simulationParts = state.parts;
        std::map<PartPin, PartPin> simConnections = state.connections;
        state.runtimeInput.clear();

        int globalInputIdx = 0;
        for (std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it)
        {
                int partID = it->first;
                std::vector<State>& vals = it->second;
                int startIdx = globalInputIdx;
                int count = (int)vals.size();

                simulationParts[partID] = [startIdx, count](std::vector<State> runtimeInput) -> std::vector<State> {
                        std::vector<State> result;
                        for (int i = 0; i < count; ++i) {
                                if ((size_t)(startIdx + i) < runtimeInput.size()) {
                                        result.push_back(runtimeInput[startIdx + i]);
                                } else {
                                        result.push_back(STATE_UNDEFINED);
                                }
                        }
                        return result;
                };

                for (size_t i = 0; i < vals.size(); ++i)
                {
                        state.runtimeInput.push_back(vals[i]);
                        globalInputIdx++;
                }
        }

        int outIdx = 0;
        for(std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                if (it->second == PART_TYPE_OUTPUT || it->second == PART_TYPE_DISPLAY)
                {
                        int inC = state.inputCounts[it->first];
                        for (int p = 0; p < inC; ++p)
                        {
                                simConnections[{state.rootSinkID, outIdx++}] = {it->first, p};
                        }
                }
        }

        if (state.isSimulating && !state.visualizeSignals && buildNativeSimulation(state)) return;
        state.nativeActive = false;
        state.simulation = assemblePart(simulationParts, simConnections, state.rootSinkID, &state.captureNets, &state.netStates);
}

void updateSimulation(AppState& state)
{
        if (state.benchmarkPending)
        {
                if (state.benchmark.running)
                {
                        runBenchmark(state);
                        state.benchmarkPending = false;
                        state.benchmark.running = false;
                }
                else
                {
                        state.benchmark.running = true;
                }
        }

        if (!state.simulation) recompileSimulation(state);
        state.runtimeInput.clear();
        for (std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it)
        {
                for (State val : it->second)
                {
                        state.runtimeInput.push_back(val);
                }
        }

        if (state.isSimulating && state.simulation)
        {
                state.simTimer += GetFrameTime();
                float stepTime = 1.0f / state.targetHZ;
                if (stepTime < 1e-9f) stepTime = 1e-9f;
                double stepStart = GetTime();
                long steps = 0;
                bool capped = false;
                while (state.simTimer >= stepTime)
                {
                        state.captureNets = state.visualizeSignals;
                        state.lastOutputStates = state.simulation(state.runtimeInput);
                        state.stepCount++;
                        state.simTimer -= stepTime;
                        ++steps;
                        if ((steps & 31) == 0 && GetTime() - stepStart > SIM_STEP_BUDGET_SEC) { capped = true; break; }
                }
                state.captureNets = false;
                if (capped) state.simTimer = 0.0f;
        }
        else if (!state.isSimulating && state.simulation && IsKeyPressed(KEY_RIGHT))
        {
                state.captureNets = state.visualizeSignals;
                state.lastOutputStates = state.simulation(state.runtimeInput);
                state.captureNets = false;
                state.stepCount++;
        }

        state.hzSampleTimer += GetFrameTime();
        if (state.hzSampleTimer >= 0.4f)
        {
                state.actualHz = (float)(state.stepCount - state.hzSampleBase) / state.hzSampleTimer;
                state.hzSampleBase = state.stepCount;
                state.hzSampleTimer = 0.0f;
                state.simSaturated = state.isSimulating && state.actualHz > 0.0f && state.actualHz < state.targetHZ * 0.85f;
        }
        if (!state.isSimulating) state.simSaturated = false;
}
