#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>

void setSourcePart(std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                   int partID)
{
        parts[partID] = [](std::vector<bool> input) -> std::vector<bool>
        {
                return input;
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

std::function<std::vector<bool>(std::vector<bool>)> compilePart(
        const std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
        const std::map<std::pair<int, int>, std::pair<int, int>>& connections,
        int partID)
{
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>::const_iterator partIt = parts.find(partID);
        if (partIt == parts.end())
        {
                return [](std::vector<bool>) { return std::vector<bool>{}; };
        }

        std::function<std::vector<bool>(std::vector<bool>)> currentLogic = partIt->second;
        std::vector<std::pair<int, std::function<std::vector<bool>(std::vector<bool>)>>> inputProviders;
        std::vector<int> outputPinIndices;

        std::map<std::pair<int, int>, std::pair<int, int>>::const_iterator connIt = connections.lower_bound(std::make_pair(partID, -1));

        while (connIt != connections.end() && connIt->first.first == partID)
        {
                int inputIdx = connIt->first.second;
                int sourcePartID = connIt->second.first;
                int sourceOutputIdx = connIt->second.second;

                std::function<std::vector<bool>(std::vector<bool>)> dependency = compilePart(parts, connections, sourcePartID);

                inputProviders.push_back(std::make_pair(inputIdx, dependency));
                outputPinIndices.push_back(sourceOutputIdx);

                connIt++;
        }

        if (inputProviders.empty())
        {
                return currentLogic;
        }

        return [currentLogic, inputProviders, outputPinIndices](std::vector<bool> runtimeInput) -> std::vector<bool>
        {
                std::vector<bool> collectedInputs;
                int maxInputIndex = 0;

                for (size_t i = 0; i < inputProviders.size(); ++i)
                {
                        if (inputProviders[i].first > maxInputIndex)
                        {
                                maxInputIndex = inputProviders[i].first;
                        }
                }

                if (collectedInputs.size() <= (size_t)maxInputIndex)
                {
                        collectedInputs.resize(maxInputIndex + 1, false);
                }

                for (size_t i = 0; i < inputProviders.size(); ++i)
                {
                        std::vector<bool> output = inputProviders[i].second(runtimeInput);
                        int pin = outputPinIndices[i];

                        if ((size_t)pin < output.size())
                        {
                                collectedInputs[inputProviders[i].first] = output[pin];
                        }
                }

                return currentLogic(collectedInputs);
        };
}

int main()
{
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>> parts;
        std::map<std::pair<int, int>, std::pair<int, int>> connections;

        setSourcePart(parts, 0);
        setPartFunction(parts, 1, andFunction);
        setOutputPart(parts, 2);

        connectParts(connections, 0, 0, 1, 0);
        connectParts(connections, 0, 1, 1, 1);
        connectParts(connections, 1, 0, 2, 0);

        visualizeCircuit(parts, connections);

        std::function<std::vector<bool>(std::vector<bool>)> circuit = compilePart(parts, connections, 2);

        circuit({true, false});

        circuit({true, true});

        return 0;
}
