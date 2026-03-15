#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <set>
#include <filesystem>

#include <glaze/glaze.hpp>

#include "part.h"
#include "primitives.h"
#include "utils.h"
#include "appstate.h"
#include "config.h"
#include "compiler/compiler.h"

typedef struct SerializablePart
{
        int id;
        PartType type;
        std::string label;
        float x;
        float y;
        int numInputs;
        int numOutputs;
} SPart;

typedef struct SerializableConnectionPin
{
        int id;
        int pin;
} SCPin;

typedef struct SerializableConnection
{
        SCPin from;
        SCPin to;
} SConn;

struct LayoutData
{
        std::vector<SPart> parts;
        std::vector<SConn> connections;
};

void saveLayout(const std::map<int, PartType>& partTypes,
                const std::map<PartPin, PartPin>& connections,
                const std::map<int, std::string>& labels,
                const std::map<int, std::pair<float, float>>& positions,
                const std::map<int, int>& inputCounts,
                const std::map<int, int>& outputCounts,
                const std::string& filename)
{
        LayoutData layoutData;
        for (std::map<int, PartType>::const_iterator partIt = partTypes.begin(); partIt != partTypes.end(); ++partIt)
        {
                SPart part;
                part.id = partIt->first;
                part.type = partIt->second;
                part.label = "";
                part.x = 0.0f;
                part.y = 0.0f;
                part.numInputs = 2;
                part.numOutputs = 1;

                if (labels.find(part.id) != labels.end()) part.label = labels.at(part.id);
                if (positions.find(part.id) != positions.end())
                {
                        part.x = positions.at(part.id).first;
                        part.y = positions.at(part.id).second;
                }
                if (inputCounts.find(part.id) != inputCounts.end()) part.numInputs = inputCounts.at(part.id);
                if (outputCounts.find(part.id) != outputCounts.end()) part.numOutputs = outputCounts.at(part.id);

                layoutData.parts.push_back(part);
        }
        for (std::map<PartPin, PartPin>::const_iterator connIt = connections.begin(); connIt != connections.end(); ++connIt)
        {
                SConn conn;
                SCPin from = {connIt->second.first, connIt->second.second};
                SCPin to = {connIt->first.first, connIt->first.second};
                conn = {from, to};
                layoutData.connections.push_back(conn);
        }

        std::string json;
        if (glz::write<glz::opts{.prettify = true}>(layoutData, json)) return;

        std::ofstream file(filename);
        if (file.is_open())
        {
                file << json;
                file.close();
        }
}

int loadLayout(AppState& state, const std::string& filename)
{
        std::ifstream file(filename);
        if (!file.is_open()) return 0;

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        LayoutData layoutData{};
        if (glz::read_json(layoutData, json)) return 0;

        state.parts.clear();

        setSourcePart(state.parts, state.rootSourceID);
        setOutputPart(state.parts, state.rootSinkID);

        state.partTypes.clear();
        state.connections.clear();
        state.labels.clear();
        state.positions.clear();
        state.inputCounts.clear();
        state.outputCounts.clear();
        state.sourceValues.clear();
        state.selectedParts.clear();
        state.simulation = nullptr;
        state.stepCount = 0;

        int maxID = 0;
        std::set<int> validIDs;

        for (size_t i = 0; i < layoutData.parts.size(); ++i)
        {
                const SPart& part = layoutData.parts[i];
                validIDs.insert(part.id);
                if (part.id > maxID) maxID = part.id;
                switch (part.type)
                {
                case PART_TYPE_SOURCE: setSourcePart(state.parts, part.id); break;
                case PART_TYPE_OUTPUT: setOutputPart(state.parts, part.id); break;
                case PART_TYPE_CUSTOM:
                {
                        int dummyIn, dummyOut;
                        std::string layoutPath = "layouts/" + part.label + ".json";

                        if (std::filesystem::exists(layoutPath)) 
                        {
                                Part layoutPart = loadLayoutAsPart(layoutPath, dummyIn, dummyOut);
                                if (layoutPart) setPart(state.parts, part.id, layoutPart); 
                        }
                        else
                        {
                                Part compiledPart = loadCompiledPart(part.label, part.numOutputs);
                                if (compiledPart)
                                {
                                        setPart(state.parts, part.id, compiledPart);
                                }
                                else
                                {
                                        std::cerr << "Warning: Could not load custom part '" << part.label << "'" << std::endl;
                                }
                        }
                }
                break;
                default: setPart(state.parts, part.id, getPartFromType(part.type)); break;
                }
                state.partTypes[part.id] = part.type;
                state.labels[part.id] = part.label;
                state.positions[part.id] = {part.x, part.y};
                state.inputCounts[part.id] = part.numInputs;
                state.outputCounts[part.id] = (part.type == PART_TYPE_OUTPUT) ? 0 : ((part.numOutputs > 0) ? part.numOutputs : 1);

                if (part.type == PART_TYPE_SOURCE)
                {
                        state.sourceValues[part.id].resize(state.outputCounts[part.id], STATE_LOW);
                }
        }

        for (size_t i = 0; i < layoutData.connections.size(); ++i)
        {
                const SConn& conn = layoutData.connections[i];
                if (validIDs.count(conn.to.id) && validIDs.count(conn.from.id))
                {
                        state.connections[{conn.to.id, conn.to.pin}] = {conn.from.id, conn.from.pin};
                }
        }

        return maxID;
}

Part loadLayoutAsPart(const std::string& filename, int& nInputs, int& nOutputs)
{
        std::ifstream file(filename);
        if (!file.is_open()) return nullptr;

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        LayoutData layoutData{};
        if (glz::read_json(layoutData, json)) return nullptr;

        std::map<int, Part> subParts;
        std::map<int, PartType> subTypes;
        std::map<PartPin, PartPin> subConnections;
        std::vector<int> internalSources;
        std::vector<int> internalOutputs;
        std::set<int> validIDs;

        for (size_t i = 0; i < layoutData.parts.size(); ++i)
        {
                const SPart& part = layoutData.parts[i];
                validIDs.insert(part.id);
                subTypes[part.id] = part.type;
                switch (part.type)
                {
                case PART_TYPE_SOURCE:
                        setSourcePart(subParts, part.id); 
                        internalSources.push_back(part.id);
                        break;
                case PART_TYPE_OUTPUT:
                        setOutputPart(subParts, part.id); 
                        internalOutputs.push_back(part.id);
                        break;
                case PART_TYPE_CUSTOM:
                {
                        int dummyIn, dummyOut;
                        std::string layoutPath = "layouts/" + part.label + ".json";

                        if (std::filesystem::exists(layoutPath)) 
                        {
                                Part layoutPart = loadLayoutAsPart(layoutPath, dummyIn, dummyOut);
                                if (layoutPart) setPart(subParts, part.id, layoutPart); 
                        }
                        else 
                        {
                                Part compiledPart = loadCompiledPart(part.label, part.numOutputs);
                                if (compiledPart) 
                                {
                                        setPart(subParts, part.id, compiledPart);
                                }
                                else
                                {
                                        std::cerr << "Warning: Could not load custom part '" << part.label << "'" << std::endl;
                                }
                        }
                }
                break;
                default: setPart(subParts, part.id, getPartFromType(part.type)); break;
                }
        }

        std::function<bool(int, int)> sortPos = [&](int a, int b) -> bool {
                float ya = 0;
                float yb = 0;
                float xa = 0;
                float xb = 0;
                for(size_t i = 0; i < layoutData.parts.size(); ++i)
                {
                        if(layoutData.parts[i].id == a) { ya = layoutData.parts[i].y; xa = layoutData.parts[i].x; }
                        if(layoutData.parts[i].id == b) { yb = layoutData.parts[i].y; xb = layoutData.parts[i].x; }
                }
                if (fabs(ya - yb) > 0.1f) return ya < yb;
                return xa < xb;
        };
        std::sort(internalSources.begin(), internalSources.end(), sortPos);
        std::sort(internalOutputs.begin(), internalOutputs.end(), sortPos);

        nInputs = 0;
        nOutputs = 0;

        for(size_t i = 0; i < internalSources.size(); ++i)
        {
             int id = internalSources[i];
             int count = 1;
             for(size_t k = 0; k < layoutData.parts.size(); ++k)
             {
                 if(layoutData.parts[k].id == id) count = (layoutData.parts[k].numOutputs > 0 ? layoutData.parts[k].numOutputs : 1);
             }
             nInputs += count;
        }

        nOutputs = (int)internalOutputs.size(); 

        for (size_t i = 0; i < layoutData.connections.size(); ++i)
        {
                const SConn& conn = layoutData.connections[i];
                if (validIDs.count(conn.to.id) && validIDs.count(conn.from.id))
                {
                        subConnections[{conn.to.id, conn.to.pin}] = {conn.from.id, conn.from.pin};
                }
        }

        int virtualSourceID = -9999;
        int virtualSinkID = -8888;

        int inputIdx = 0;
        for(size_t i = 0; i < internalSources.size(); ++i)
        {
                int srcID = internalSources[i];
                int pins = 1;
                for(size_t k = 0; k < layoutData.parts.size(); ++k)
                {
                    if(layoutData.parts[k].id == srcID) pins = (layoutData.parts[k].numOutputs > 0 ? layoutData.parts[k].numOutputs : 1);
                }

                for(int j=0; j<pins; ++j)
                {
                        subConnections[{srcID, j}] = {virtualSourceID, inputIdx++};
                }
        }

        int outputIdx = 0;
        for(size_t i = 0; i < internalOutputs.size(); ++i)
        {
                 int outID = internalOutputs[i];
                 subConnections[{virtualSinkID, outputIdx++}] = {outID, 0};
        }

        setSourcePart(subParts, virtualSourceID);
        setOutputPart(subParts, virtualSinkID);

        Part subSim = assemblePart(subParts, subConnections, virtualSinkID);

        return [subSim](std::vector<State> inputs) -> std::vector<State>
        {
                return subSim(inputs);
        };
}

std::set<int> importLayout(AppState& state, const std::string& filename, float mouseX, float mouseY)
{
        std::set<int> newIDs;
        std::ifstream file(filename);
        if (!file.is_open()) return newIDs;

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        LayoutData layoutData{};
        if (glz::read_json(layoutData, json)) return newIDs;

        int nextID = state.parts.empty() ? 100 : state.parts.rbegin()->first + 1;
        std::map<int, int> idMap;

        float minX = 9999999.0f, maxX = -9999999.0f, minY = 9999999.0f, maxY = -9999999.0f;
        for (size_t i = 0; i < layoutData.parts.size(); ++i) {
            float px = layoutData.parts[i].x;
            float py = layoutData.parts[i].y;
            if (px < minX) minX = px;
            if (px > maxX) maxX = px;
            if (py < minY) minY = py;
            if (py > maxY) maxY = py;
        }

        float centerX = 0.0f, centerY = 0.0f;
        if (layoutData.parts.size() > 0) {
            centerX = (minX + maxX) / 2.0f;
            centerY = (minY + maxY) / 2.0f;
        }

        for (size_t i = 0; i < layoutData.parts.size(); ++i)
        {
                const SPart& part = layoutData.parts[i];
                int newID = nextID++;
                idMap[part.id] = newID;
                newIDs.insert(newID);

                switch (part.type)
                {
                case PART_TYPE_SOURCE: setSourcePart(state.parts, newID); break;
                case PART_TYPE_OUTPUT: setOutputPart(state.parts, newID); break;
                case PART_TYPE_CUSTOM:
                {
                        int dummyIn, dummyOut;
                        std::string layoutPath = "layouts/" + part.label + ".json";
                        if (std::filesystem::exists(layoutPath)) 
                        {
                                Part layoutPart = loadLayoutAsPart(layoutPath, dummyIn, dummyOut);
                                if (layoutPart) setPart(state.parts, newID, layoutPart); 
                        }
                        else 
                        {
                                Part compiledPart = loadCompiledPart(part.label, part.numOutputs);
                                if (compiledPart) 
                                {
                                        setPart(state.parts, newID, compiledPart);
                                }
                                else
                                {
                                        std::cerr << "Warning: Could not load custom part '" << part.label << "'" << std::endl;
                                }
                        }
                }
                break;
                default: setPart(state.parts, newID, getPartFromType(part.type)); break;
                }

                state.partTypes[newID] = part.type;
                state.labels[newID] = part.label;

                float dx = part.x - centerX;
                float dy = part.y - centerY;
                float gx = round((mouseX + dx) / GRID_SIZE) * GRID_SIZE;
                float gy = round((mouseY + dy) / GRID_SIZE) * GRID_SIZE;

                state.positions[newID] = {gx, gy};
                state.inputCounts[newID] = part.numInputs;
                state.outputCounts[newID] = (part.type == PART_TYPE_OUTPUT) ? 0 : ((part.numOutputs > 0) ? part.numOutputs : 1);

                if (part.type == PART_TYPE_SOURCE)
                {
                        state.sourceValues[newID].resize(state.outputCounts[newID], STATE_LOW);
                }
        }

        for (size_t i = 0; i < layoutData.connections.size(); ++i)
        {
                const SConn& conn = layoutData.connections[i];
                if (idMap.count(conn.to.id) && idMap.count(conn.from.id))
                {
                        state.connections[{idMap[conn.to.id], conn.to.pin}] = {idMap[conn.from.id], conn.from.pin};
                }
        }

        state.simulation = nullptr;
        return newIDs;
}
