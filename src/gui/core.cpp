#include "gui.h"
#include "common.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>

#include <raylib/raylib.h>
#include <raylib/raymath.h>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"

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

void initApp(AppState& state)
{
        setSourcePart(state.parts, state.rootSourceID);
        setSourcePart(state.parts, state.rootSinkID);
        refreshLayouts(state);
        SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_BILINEAR);
}

void recompileSimulation(AppState& state)
{
        std::map<PartPin, PartPin> simConnections = state.connections;
        state.runtimeInput.clear();
        int globalInputIdx = 0;

        for(std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it)
        {
                int partID = it->first;
                std::vector<State>& vals = it->second;
                for (size_t i = 0; i < vals.size(); ++i)
                {
                        simConnections[{partID, (int)i}] = {state.rootSourceID, globalInputIdx};
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
        state.simulation = assemblePart(state.parts, simConnections, state.rootSinkID);
}

void updateSimulation(AppState& state)
{
        if (!state.simulation) recompileSimulation(state);

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
                state.runtimeInput.clear();
                for(std::map<int, std::vector<State>>::iterator it = state.sourceValues.begin(); it != state.sourceValues.end(); ++it)
                {
                        for (size_t i = 0; i < it->second.size(); ++i)
                        {
                                state.runtimeInput.push_back(it->second[i]);
                        }
                }

                state.lastOutputStates = state.simulation(state.runtimeInput);
                state.stepCount++;
        }
}
