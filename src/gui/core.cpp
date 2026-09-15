#include "gui.h"
#include "common.h"

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

        int stepsToDo = 0;
        if (state.isSimulating)
        {
                state.simTimer += GetFrameTime();
                float stepTime = 1.0f / state.targetHZ;
                if (stepTime < 0.000001f) stepTime = 0.000001f;
                while (state.simTimer >= stepTime && stepsToDo < 10000)
                {
                        stepsToDo++;
                        state.simTimer -= stepTime;
                }
                if (state.simTimer >= stepTime) state.simTimer = fmod(state.simTimer, stepTime);
        }
        else
        {
                if (IsKeyPressed(KEY_RIGHT)) stepsToDo = 1;
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

        if (state.simulation)
        {
                for (int i = 0; i < stepsToDo; ++i)
                {
                        state.captureNets = state.visualizeSignals && (i == stepsToDo - 1);
                        state.lastOutputStates = state.simulation(state.runtimeInput);
                        state.stepCount++;
                }
                state.captureNets = false;
        }
}
