#include "gui.h"
#include "common.h"
#include "../gates.h"
#include <filesystem>
#include <algorithm>
#include <format>
#include <fstream>
#include <set>
#include <raylib/raylib.h>
#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"
#include "../compiler/compiler.h"
#include <glaze/glaze.hpp>


void cleanupInputPinConnections(AppState& state, int partID, int removedIdx)
{
        std::vector<PartPin> toRemove;
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                if (it->first.first == partID && it->first.second == removedIdx)
                {
                        toRemove.push_back(it->first);
                }
        }
        for (size_t i = 0; i < toRemove.size(); ++i)
        {
                state.connections.erase(toRemove[i]);
        }
}

void cleanupOutputPinConnections(AppState& state, int partID, int removedIdx)
{
        std::vector<PartPin> toRemove;
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                if (it->second.first == partID && it->second.second == removedIdx)
                {
                        toRemove.push_back(it->first);
                }
        }
        for (size_t i = 0; i < toRemove.size(); ++i)
        {
                state.connections.erase(toRemove[i]);
        }
}

void doCompile(AppState& state, const std::string& modName)
{
        if (!state.compileStatic && !state.compileDynamic) return; // nothing selected
        std::string cpp = transpileToCpp(state, state.linkCustomParts);
        if (!compilePartLibrary(cpp, modName, state.compileStatic, state.compileDynamic))
        {
                state.errorMessage = "Compilation failed. Ensure a C++ compiler (clang++ or g++) is installed.";
                state.showError = true;
        }
        else
        {
                int inC = 0, outC = 0;
                std::vector<std::string> inLabels;
                std::vector<std::string> outLabels;

                std::vector<int> sortedSources;
                std::vector<int> sortedOutputs;
                for (std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
                {
                        if (it->second == PART_TYPE_SOURCE) sortedSources.push_back(it->first);
                        if (it->second == PART_TYPE_OUTPUT) sortedOutputs.push_back(it->first);
                }
                auto pinLess = [&](int a, int b) -> bool {
                        auto pa = state.positions.find(a);
                        auto pb = state.positions.find(b);
                        float xa = pa != state.positions.end() ? pa->second.first : 0.0f;
                        float ya = pa != state.positions.end() ? pa->second.second : 0.0f;
                        float xb = pb != state.positions.end() ? pb->second.first : 0.0f;
                        float yb = pb != state.positions.end() ? pb->second.second : 0.0f;
                        if (std::fabs(ya - yb) > 0.1f) return ya < yb;
                        if (std::fabs(xa - xb) > 0.1f) return xa < xb;
                        return a < b;
                };
                std::sort(sortedSources.begin(), sortedSources.end(), pinLess);
                std::sort(sortedOutputs.begin(), sortedOutputs.end(), pinLess);

                for (size_t s = 0; s < sortedSources.size(); ++s)
                {
                        int sid = sortedSources[s];
                        int pins = state.outputCounts[sid];
                        std::string lbl = state.labels[sid];
                        for (int p = 0; p < pins; ++p)
                        {
                                if (pins == 1) inLabels.push_back(lbl);
                                else inLabels.push_back(lbl + "[" + std::to_string(p) + "]");
                        }
                        inC += pins;
                }
                for (size_t s = 0; s < sortedOutputs.size(); ++s)
                {
                        int oid = sortedOutputs[s];
                        int pins = state.inputCounts[oid];
                        std::string lbl = state.labels[oid];
                        for (int p = 0; p < pins; ++p)
                        {
                                if (pins == 1) outLabels.push_back(lbl);
                                else outLabels.push_back(lbl + "[" + std::to_string(p) + "]");
                        }
                        outC += pins;
                }

                if (std::find(state.compiledModules.begin(), state.compiledModules.end(), modName) == state.compiledModules.end())
                {
                        state.compiledModules.push_back(modName);
                }
                state.compiledInputs[modName] = inC;
                state.compiledOutputs[modName] = outC;
                state.compiledInputLabels[modName] = inLabels;
                state.compiledOutputLabels[modName] = outLabels;

                std::string dir = sullaPartDir(modName);
                std::filesystem::create_directories(dir);
                CompiledMeta meta{inC, outC, inLabels, outLabels};
                std::string json;
                if (!glz::write<glz::opts{.prettify = true}>(meta, json))
                {
                        std::ofstream file(dir + "/" + modName + ".json");
                        if (file.is_open())
                        {
                                file << json;
                                file.close();
                        }
                }
                refreshCompiledModules(state);
        }
}

void dropPart(AppState& state, int type, Vector2 pos)
{
        int id = state.parts.empty() ? 100 : state.parts.rbegin()->first + 1;
        if (type == PART_TYPE_SOURCE) setSourcePart(state.parts, id);
        else if (type == PART_TYPE_OUTPUT) setOutputPart(state.parts, id);
        else setPart(state.parts, id, getPartFromType((PartType)type));

        state.partTypes[id] = (PartType)type;
        state.positions[id] = {pos.x, pos.y};
        state.labels[id] = partTypeName((PartType)type);
        state.simulation = nullptr;

        if (type == PART_TYPE_NOT)
        {
                state.inputCounts[id] = 1;
                state.outputCounts[id] = 1;
        }
        else if (type == PART_TYPE_SOURCE) 
        { 
                state.inputCounts[id] = 0; 
                state.outputCounts[id] = 1; 
                state.sourceValues[id] = {STATE_LOW};
        }
        else if (type == PART_TYPE_OUTPUT)
        {
                state.inputCounts[id] = 1;
                state.outputCounts[id] = 0;
        }
        else if (type == PART_TYPE_CLOCK)
        {
                state.inputCounts[id] = 0;
                state.outputCounts[id] = 1;
        }
        else if (type == PART_TYPE_DISPLAY)
        {
                state.inputCounts[id] = 8;
                state.outputCounts[id] = 0;
        }
        else
        {
                state.inputCounts[id] = 2;
                state.outputCounts[id] = 1;
        }
}

void deleteParts(AppState& state)
{
        if (state.selectedParts.empty()) return;

        for (std::set<int>::iterator it = state.selectedParts.begin(); it != state.selectedParts.end(); ++it)
        {
                int id = *it;
                state.parts.erase(id);
                state.partTypes.erase(id);
                state.positions.erase(id);
                state.inputCounts.erase(id);
                state.outputCounts.erase(id);
                state.sourceValues.erase(id);
                state.labels.erase(id);
                state.inputPinLabels.erase(id);
                state.outputPinLabels.erase(id);

                std::vector<PartPin> toRemove;
                for(std::map<PartPin, PartPin>::iterator connIt = state.connections.begin(); connIt != state.connections.end(); ++connIt)
                {
                        if (connIt->first.first == id || connIt->second.first == id)
                        {
                                toRemove.push_back(connIt->first);
                        }
                }
                for(size_t i = 0; i < toRemove.size(); ++i)
                {
                        state.connections.erase(toRemove[i]);
                        state.connectionWaypoints.erase(toRemove[i]);
                }
        }
        state.selectedParts.clear();
        state.simulation = nullptr;
}

void copySelection(AppState& state)
{
        state.clipboard.parts.clear();
        state.clipboard.connections.clear();
        for (int id : state.selectedParts)
        {
                ClipboardPart cp;
                cp.id = id;
                cp.type = state.partTypes[id];
                cp.label = state.labels[id];
                cp.x = state.positions[id].first;
                cp.y = state.positions[id].second;
                cp.inputs = state.inputCounts[id];
                cp.outputs = state.outputCounts[id];
                std::map<int, std::vector<State>>::iterator sv = state.sourceValues.find(id);
                if (sv != state.sourceValues.end()) cp.sourceValues = sv->second;
                state.clipboard.parts.push_back(cp);
        }
        for (std::map<PartPin, PartPin>::iterator c = state.connections.begin(); c != state.connections.end(); ++c)
        {
                if (state.selectedParts.count(c->first.first) && state.selectedParts.count(c->second.first))
                        state.clipboard.connections.push_back({c->second, c->first});
        }
}

void pasteClipboard(AppState& state, Vector2 offset)
{
        std::map<int, int> idMap;
        state.selectedParts.clear();
        for (ClipboardPart cp : state.clipboard.parts)
        {
                int id = state.parts.empty() ? 100 : state.parts.rbegin()->first + 1;
                if (cp.type == PART_TYPE_SOURCE) setSourcePart(state.parts, id);
                else if (cp.type == PART_TYPE_OUTPUT) setOutputPart(state.parts, id);
                else setPart(state.parts, id, getPartFromType(cp.type));
                state.partTypes[id] = cp.type;
                state.positions[id] = {cp.x + offset.x, cp.y + offset.y};
                state.labels[id] = cp.label;
                state.inputCounts[id] = cp.inputs;
                state.outputCounts[id] = cp.outputs;
                if (!cp.sourceValues.empty()) state.sourceValues[id] = cp.sourceValues;
                idMap[cp.id] = id;
                state.selectedParts.insert(id);
        }
        for (ClipboardConn cc : state.clipboard.connections)
        {
                if (idMap.count(cc.from.first) && idMap.count(cc.to.first))
                        state.connections[{idMap[cc.to.first], cc.to.second}] = {idMap[cc.from.first], cc.from.second};
        }
        state.simulation = nullptr;
}

void duplicateSelection(AppState& state)
{
        copySelection(state);
        pasteClipboard(state, {GRID_SIZE * 2.0f, GRID_SIZE * 2.0f});
}

void selectAllParts(AppState& state)
{
        state.selectedParts.clear();
        for (std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
                state.selectedParts.insert(it->first);
}

void fitView(AppState& state)
{
        if (state.positions.empty()) return;
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
        {
                Rectangle r = getBodyRect(state, it->first);
                if (r.x < minX) minX = r.x;
                if (r.y < minY) minY = r.y;
                if (r.x + r.width > maxX) maxX = r.x + r.width;
                if (r.y + r.height > maxY) maxY = r.y + r.height;
        }
        const float MARGIN = 80.0f;
        const float MAX_FIT_ZOOM = 2.0f;
        float boxW = maxX - minX + MARGIN * 2.0f;
        float boxH = maxY - minY + MARGIN * 2.0f;
        float zoomX = (float)GetScreenWidth() / boxW;
        float zoomY = (float)GetScreenHeight() / boxH;
        float zoom = zoomX < zoomY ? zoomX : zoomY;
        if (zoom > MAX_FIT_ZOOM) zoom = MAX_FIT_ZOOM;
        state.camera.zoom = zoom;
        state.camera.target = {(minX + maxX) / 2.0f, (minY + maxY) / 2.0f};
        state.camera.offset = {(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
}

void nudgeSelection(AppState& state, float dx, float dy)
{
        for (int id : state.selectedParts)
        {
                state.positions[id].first += dx;
                state.positions[id].second += dy;
        }
}

void alignSelection(AppState& state, int edge)
{
        if (state.selectedParts.empty()) return;
        bool wantMax = (edge == 1 || edge == 3);
        float target = wantMax ? -1e30f : 1e30f;
        for (int id : state.selectedParts)
        {
                Rectangle r = getBodyRect(state, id);
                float v = (edge == 0) ? r.x : (edge == 1) ? r.x + r.width : (edge == 2) ? r.y : r.y + r.height;
                if (wantMax) { if (v > target) target = v; }
                else { if (v < target) target = v; }
        }
        for (int id : state.selectedParts)
        {
                Vector2 sz = getPartSize(state, id);
                if (edge == 0) state.positions[id].first = target + sz.x / 2.0f;
                else if (edge == 1) state.positions[id].first = target - sz.x / 2.0f;
                else if (edge == 2) state.positions[id].second = target + sz.y / 2.0f;
                else state.positions[id].second = target - sz.y / 2.0f;
        }
}

void distributeSelection(AppState& state, bool horizontal)
{
        if (state.selectedParts.size() < 3) return;
        std::vector<int> ids(state.selectedParts.begin(), state.selectedParts.end());
        std::sort(ids.begin(), ids.end(), [&](int a, int b) {
                return horizontal ? state.positions[a].first < state.positions[b].first
                                  : state.positions[a].second < state.positions[b].second;
        });
        float first = horizontal ? state.positions[ids.front()].first : state.positions[ids.front()].second;
        float last = horizontal ? state.positions[ids.back()].first : state.positions[ids.back()].second;
        float step = (last - first) / (float)(ids.size() - 1);
        for (size_t i = 0; i < ids.size(); ++i)
        {
                float v = first + (float)i * step;
                if (horizontal) state.positions[ids[i]].first = v;
                else state.positions[ids[i]].second = v;
        }
}

void loadExternalLayout(AppState& state, const std::string& path)
{
        if (path.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories("layouts");
        std::filesystem::path src(path);
        std::filesystem::path dest = std::filesystem::path("layouts") / src.filename();
        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
                state.errorMessage = "Could not copy the layout into layouts/: " + ec.message();
                state.showError = true;
                return;
        }
        loadLayout(state, dest.string());
        recompileSimulation(state);
        refreshLayouts(state);
}
