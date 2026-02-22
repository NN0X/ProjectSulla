#include "gui.h"
#include "common.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>

#include <raylib/raylib.h>
#include <raylib/raymath.h>
#include <glaze/glaze.hpp>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"

struct CompiledMeta
{
        int inputs;
        int outputs;
};

Color getThemeColor(const AppState& state, Color light, Color dark)
{
        return state.darkMode ? dark : light;
}

Vector2 getPartSize(const AppState& state, int id)
{
        float txtW = (float)MeasureText(state.labels.at(id).c_str(), 10);
        float w = BASE_PART_WIDTH;
        if (txtW + TEXT_PADDING * 2 > w)
        {
                w = txtW + TEXT_PADDING * 2;
        }
        int inCount = state.inputCounts.at(id);
        int outCount = state.outputCounts.at(id);
        int maxPins = (inCount > outCount) ? inCount : outCount;
        float h = BASE_PART_HEIGHT;
        float pinsH = (float)(maxPins - 1) * PIN_SPACING + PIN_Y_OFFSET_BASE * 2;
        if (maxPins > 1 && pinsH > h)
        {
                h = pinsH;
        }
        return {w, h};
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
        if (std::filesystem::exists("parts"))
        {
                for (const auto& entry : std::filesystem::directory_iterator("parts"))
                {
                        if (entry.path().extension() == ".json")
                        {
                                std::string modName = entry.path().stem().string();
                                std::ifstream file(entry.path());
                                if (file.is_open())
                                {
                                        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                                        file.close();
                                        CompiledMeta meta{};
                                        if (!glz::read_json(meta, json))
                                        {
                                                state.compiledModules.push_back(modName);
                                                state.compiledInputs[modName] = meta.inputs;
                                                state.compiledOutputs[modName] = meta.outputs;
                                        }
                                }
                        }
                }
        }
}

void initApp(AppState& state)
{
        setSourcePart(state.parts, state.rootSourceID);
        setSourcePart(state.parts, state.rootSinkID);
        refreshLayouts(state);
        refreshCompiledModules(state);
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
                if (it->second == PART_TYPE_OUTPUT)
                {
                        simConnections[{state.rootSinkID, outIdx++}] = {it->first, 0};
                }
        }

        state.simulation = assemblePart(simulationParts, simConnections, state.rootSinkID);
}

void updateSimulation(AppState& state)
{
        if (!state.simulation) recompileSimulation(state);
        state.runtimeInput.clear();
        for (std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it)
        {
                for (State val : it->second)
                {
                        state.runtimeInput.push_back(val);
                }
        }
        bool step = false;
        if (state.isSimulating)
        {
                state.simTimer += GetFrameTime();
                float stepTime = 1.0f / state.targetHZ;
                if (state.simTimer >= stepTime)
                {
                        step = true;
                        state.simTimer = 0.0f;
                }
        }
        else if (IsKeyPressed(KEY_RIGHT))
        {
                step = true;
        }
        if (step && state.simulation)
        {
                state.lastOutputStates = state.simulation(state.runtimeInput);
                state.stepCount++;
        }
}
