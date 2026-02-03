#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>
#include <fstream>

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

void saveLayout(const std::map<int, Part>& parts, const std::map<PartPin, PartPin>& connections,
              const std::map<int, std::string>& labels, const std::string& filename)
{
        std::ofstream file(filename);
        if (!file.is_open())
        {
                std::cerr << "Error: Could not open file " << filename << " for writing.\n";
                return;
        }

        // TODO: json serialization

        file.close();
}

void loadLayout(std::map<int, Part>& parts, std::map<PartPin, PartPin>& connections,
              std::map<int, std::string>& labels, const std::string& filename)
{
        std::ifstream file(filename);
        if (!file.is_open())
        {
                std::cerr << "Error: Could not open file " << filename << " for reading.\n";
                return;
        }

        parts.clear();
        connections.clear();
        labels.clear();

        // TODO: json deserialization

        file.close();
}
