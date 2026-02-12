#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>
#include <fstream>

#include <glaze/glaze.hpp>

#include "part.h"
#include "primitives.h"
#include "utils.h"

typedef struct SerializablePart
{
        int id;
        PartType type;
        std::string label;
        float x;
        float y;
        int numInputs;
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

void saveLayout(const std::map<int, Part>& parts, const std::map<int, PartType>& partTypes,
                const std::map<PartPin, PartPin>& connections,
                const std::map<int, std::string>& labels,
                const std::map<int, std::pair<float, float>>& positions,
                const std::map<int, int>& inputCounts,
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

                if (labels.find(part.id) != labels.end())
                {
                        part.label = labels.at(part.id);
                }
                if (positions.find(part.id) != positions.end())
                {
                        part.x = positions.at(part.id).first;
                        part.y = positions.at(part.id).second;
                }
                if (inputCounts.find(part.id) != inputCounts.end())
                {
                        part.numInputs = inputCounts.at(part.id);
                }

                layoutData.parts.push_back(part);
        }
        for (std::map<PartPin, PartPin>::const_iterator connIt = connections.begin(); connIt != connections.end(); ++connIt)
        {
                SConn conn;
                SCPin from;
                SCPin to;
                from = {connIt->second.first, connIt->second.second};
                to = {connIt->first.first, connIt->first.second};
                conn = {from, to};
                layoutData.connections.push_back(conn);
        }

        std::string json;
        auto result = glz::write<glz::opts{.prettify = true}>(layoutData, json);
        if (result)
        {
                std::cerr << "Error: Could not serialize layout data to JSON.\n";
                return;
        }

        std::ofstream file(filename);
        if (!file.is_open())
        {
                std::cerr << "Error: Could not open file " << filename << " for writing.\n";
                return;
        }

        file << json;

        file.close();
}

int loadLayout(std::map<int, Part>& parts, std::map<int, PartType>& partTypes,
               std::map<PartPin, PartPin>& connections,
               std::map<int, std::string>& labels,
               std::map<int, std::pair<float, float>>& positions,
               std::map<int, int>& inputCounts,
               const std::string& filename)
{
        std::ifstream file(filename);
        if (!file.is_open())
        {
                std::cerr << "Error: Could not open file " << filename << " for reading.\n";
                std::exit(1);
        }

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        LayoutData layoutData{};
        auto result = glz::read_json(layoutData, json);
        if (result)
        {
                std::cerr << "Error: Could not deserialize layout data from JSON.\n";
                std::exit(1);
        }

        parts.clear();
        partTypes.clear();
        connections.clear();
        labels.clear();
        positions.clear();
        inputCounts.clear();

        int outputID = -1;
        for (const SPart& part : layoutData.parts)
        {
                switch (part.type)
                {
                case PART_TYPE_SOURCE:
                        setSourcePart(parts, part.id);
                        break;
                case PART_TYPE_OUTPUT:
                        setOutputPart(parts, part.id);
                        outputID = part.id;
                        break;
                default:
                        setPart(parts, part.id, getPartFromType(part.type));
                        break;
                }
                partTypes[part.id] = part.type;
                labels[part.id] = part.label;
                positions[part.id] = {part.x, part.y};
                inputCounts[part.id] = part.numInputs;
        }

        for (const SConn& conn : layoutData.connections)
        {
                SCPin from = conn.from;
                SCPin to = conn.to;
                connections[{to.id, to.pin}] = {from.id, from.pin};
        }

        return outputID;
}
