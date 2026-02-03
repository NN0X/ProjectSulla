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
        int fromID;
        int fromPin;
        int toID;
        int toPin;
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
        for (const auto& partIt : partTypes)
        {
                SerializablePart sPart;
                sPart.id = partIt.first;
                sPart.type = partIt.second;
                sPart.label = "";
                if (labels.find(sPart.id) != labels.end())
                {
                        sPart.label = labels.at(sPart.id);
                }
                layoutData.parts.push_back(sPart);
        }
        for (const auto& connIt : connections)
        {
                SerializableConnection sConn;
                sConn.fromID = connIt.second.first;
                sConn.fromPin = connIt.second.second;
                sConn.toID = connIt.first.first;
                sConn.toPin = connIt.first.second;
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

int loadLayout(std::map<int, Part>& parts, std::map<int, PartType>& partTypes,
              std::map<PartPin, PartPin>& connections,
              std::map<int, std::string>& labels, const std::string& filename)
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

        int outputID = -1;
        for (const auto& sPart : layoutData.parts)
        {
                switch (sPart.type)
                {
                        case PART_TYPE_SOURCE:
                                setSourcePart(parts, sPart.id);
                                break;
                        case PART_TYPE_OUTPUT:
                                setOutputPart(parts, sPart.id);
                                if (outputID != -1)
                                {
                                        std::cerr << "Warning: Multiple output parts found in layout:" << outputID << " and " << sPart.id << std::endl;
                                        std::exit(1);
                                }
                                outputID = sPart.id;
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
                connections[{sConn.toID, sConn.toPin}] = {sConn.fromID, sConn.fromPin};
        }

        return outputID;
}
