#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>

void setSourcePart(std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                   int partID, std::vector<bool> data)
{
        parts[partID] = [data](std::vector<bool> input) -> std::vector<bool>
        {
                return data;
        };
}

void setOutputPart(std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                   int partID)
{
        parts[partID] = [](std::vector<bool> input) -> std::vector<bool>
        {
                for (size_t i = 0; i < input.size(); ++i)
                {
                        std::cout << input[i] << " ";
                }
                std::cout << std::endl;
                return {};
        };
}

void connectParts(std::map<std::pair<int, int>, std::pair<int, int>>& connections,
                  int fromPartID, int fromOutputID,
                  int toPartID, int toInputID)
{
        connections[std::make_pair(toPartID, toInputID)] = std::make_pair(fromPartID, fromOutputID);
}

void setPartFunction(std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                     int partID,
                     std::function<std::vector<bool>(std::vector<bool>)> func)
{
        parts[partID] = func;
}

std::vector<bool> andFunction(std::vector<bool> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {input[0] && input[1]};
}

std::vector<bool> compilePart(const std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                              const std::map<std::pair<int, int>, std::pair<int, int>>& connections,
                              int partID)
{
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>::const_iterator partIt = parts.find(partID);
        if (partIt == parts.end())
        {
                return {};
        }

        std::vector<bool> input;
        std::map<std::pair<int, int>, std::pair<int, int>>::const_iterator connIt = connections.lower_bound(std::make_pair(partID, -1));

        while (connIt != connections.end() && connIt->first.first == partID)
        {
                int inputIndex = connIt->first.second;

                if (input.size() <= (size_t)inputIndex)
                {
                        input.resize(inputIndex + 1, false);
                }

                std::vector<bool> fromOutput = compilePart(parts, connections, connIt->second.first);

                if ((size_t)connIt->second.second < fromOutput.size())
                {
                        input[inputIndex] = fromOutput[connIt->second.second];
                }
                else
                {
                        input[inputIndex] = false;
                }

                connIt++;
        }

        return partIt->second(input);
}

int main()
{
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>> parts;
        std::map<std::pair<int, int>, std::pair<int, int>> connections;

        setSourcePart(parts, 0, {true, false});

        setPartFunction(parts, 1, andFunction);
        connectParts(connections, 0, 0, 1, 0);
        connectParts(connections, 0, 1, 1, 1);

        setOutputPart(parts, 2);
        connectParts(connections, 1, 0, 2, 0);

        compilePart(parts, connections, 2);

        return 0;
}
