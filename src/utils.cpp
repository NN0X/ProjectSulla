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

void visualizePath(const std::map<int, Part>& parts, const std::map<PartPin, PartPin>& connections,
                   const std::map<int, std::string>& labels)
{
        std::cout << "graph LR;\n";
        for (auto partIt = parts.begin(); partIt != parts.end(); ++partIt)
        {
                int id = partIt->first;
                std::string name = "Unknown";
                if (labels.find(id) != labels.end())
                {
                        name = labels.find(id)->second;
                }
                std::cout << "    " << id << "[\"" << name << " (" << id << ")\"];\n";
        }

        for (auto connIt = connections.begin(); connIt != connections.end(); ++connIt)
        {
                int fromID = connIt->second.first;
                int fromPin = connIt->second.second;
                int toID = connIt->first.first;
                int toPin = connIt->first.second;

                std::cout << "    " << fromID << " -- " << "\"Out:" << fromPin << " In:" << toPin << "\" --> " << toID << ";\n";
        }
        std::cout << "\n";
}

struct SerializablePart
{
        int id;
        PartType type;
        std::string label;
};

struct SerializableConnection
{
        PartPin from;
        PartPin to;
};

struct LayoutData
{
        std::vector<SerializablePart> parts;
        std::vector<SerializableConnection> connections;
};

void saveLayout(const std::map<int, Part>& parts, const std::map<int, PartType>& partTypes,
              const std::map<PartPin, PartPin>& connections,
              const std::map<int, std::string>& labels, const std::string& filename)
{
        LayoutData layoutData;
        for (const auto& partIt : parts)
        {
                SerializablePart sPart;
                sPart.id = partIt.first;
                sPart.type = partTypes.at(partIt.first);
                sPart.label = labels.at(partIt.first);
                layoutData.parts.push_back(sPart);
        }
        for (const auto& connIt : connections)
        {
                SerializableConnection sConn;
                sConn.from = connIt.second;
                sConn.to = connIt.first;
                layoutData.connections.push_back(sConn);
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

void loadLayout(std::map<int, Part>& parts, std::map<int, PartType>& partTypes,
              std::map<PartPin, PartPin>& connections,
              std::map<int, std::string>& labels, const std::string& filename)
{
        std::ifstream file(filename);
        if (!file.is_open())
        {
                std::cerr << "Error: Could not open file " << filename << " for reading.\n";
                return;
        }

        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        LayoutData layoutData{};
        auto result = glz::read_json(layoutData, json);
        if (result)
        {
                std::cerr << "Error: Could not deserialize layout data from JSON.\n";
                return;
        }

        for (const auto& sPart : layoutData.parts)
        {
                parts[sPart.id] = getPartFromType(sPart.type);
                partTypes[sPart.id] = sPart.type;
                labels[sPart.id] = sPart.label;
        }
        for (const auto& sConn : layoutData.connections)
        {
                connections[sConn.to] = sConn.from;
        }

        parts.clear();
        partTypes.clear();
        connections.clear();
        labels.clear();

        for (const auto& sPart : layoutData.parts)
        {
                switch (sPart.type)
                {
                        case PART_TYPE_SOURCE:
                                setSourcePart(parts, sPart.id);
                                break;
                        case PART_TYPE_OUTPUT:
                                setOutputPart(parts, sPart.id);
                                break;
                        default:
                                setPart(parts, sPart.id, getPartFromType(sPart.type));
                                break;
                }
                partTypes[sPart.id] = sPart.type;
                labels[sPart.id] = sPart.label;
        }

        for (const auto& sConn : layoutData.connections)
        {
                connections[sConn.to] = sConn.from;
        }
}
