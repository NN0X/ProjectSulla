#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>

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
                std::cout << "\n";
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

void visualizePath(const std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                      const std::map<std::pair<int, int>, std::pair<int, int>>& connections,
                      const std::map<int, std::string>& labels)
{
        std::cout << "graph LR;\n";
        for (std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>::const_iterator partIt = parts.begin(); partIt != parts.end(); ++partIt)
        {
                int id = partIt->first;
                std::string name = "Unknown";
                if (labels.find(id) != labels.end())
                {
                        name = labels.find(id)->second;
                }
                std::cout << "    " << id << "[\"" << name << " (" << id << ")\"];\n";
        }

        for (std::map<std::pair<int, int>, std::pair<int, int>>::const_iterator connIt = connections.begin(); connIt != connections.end(); ++connIt)
        {
                int fromID = connIt->second.first;
                int fromPin = connIt->second.second;
                int toID = connIt->first.first;
                int toPin = connIt->first.second;

                std::cout << "    " << fromID << " -- " << "\"Out:" << fromPin << " In:" << toPin << "\" --> " << toID << ";\n";
        }
        std::cout << "\n";
}

std::function<std::vector<bool>(std::vector<bool>)> compilePart(
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>> parts,
        const std::map<std::pair<int, int>, std::pair<int, int>>& connections,
        int partID)
{
        std::map<int, std::vector<bool>> lastOutputs;

        return [parts, connections, partID, lastOutputs](std::vector<bool> runtimeInput) mutable -> std::vector<bool>
        {
                std::map<int, std::vector<bool>> cache;
                std::vector<int> recursionStack;
                std::function<std::vector<bool>(int)> resolve;

                resolve = [&](int currentID) -> std::vector<bool>
                {
                        if (cache.find(currentID) != cache.end())
                        {
                                return cache[currentID];
                        }

                        for (size_t i = 0; i < recursionStack.size(); ++i)
                        {
                                if (recursionStack[i] == currentID)
                                {
                                        if (lastOutputs.find(currentID) != lastOutputs.end())
                                        {
                                                return lastOutputs[currentID];
                                        }
                                        return {false};
                                }
                        }

                        recursionStack.push_back(currentID);

                        std::vector<bool> collectedInputs;
                        int maxInputIndex = -1;
                        auto it = connections.lower_bound(std::make_pair(currentID, -1));
                        bool hasConnections = (it != connections.end() && it->first.first == currentID);

                        while (it != connections.end() && it->first.first == currentID)
                        {
                                if (it->first.second > maxInputIndex)
                                {
                                        maxInputIndex = it->first.second;
                                }
                                int inputIdx = it->first.second;
                                int sourceID = it->second.first;
                                int sourceOutputIdx = it->second.second;
                                std::vector<bool> sourceResult = resolve(sourceID);

                                if (collectedInputs.size() <= (size_t)inputIdx)
                                {
                                        collectedInputs.resize(inputIdx + 1, false);
                                }

                                if ((size_t)sourceOutputIdx < sourceResult.size())
                                {
                                        collectedInputs[inputIdx] = sourceResult[sourceOutputIdx];
                                }
                                it++;
                        }

                        std::vector<bool> result;
                        if (!hasConnections)
                        {
                                result = parts.at(currentID)(runtimeInput);
                        }
                        else
                        {
                                result = parts.at(currentID)(collectedInputs);
                        }

                        recursionStack.pop_back();

                        cache[currentID] = result;
                        lastOutputs[currentID] = result;
                        return result;
                };

                return resolve(partID);
        };
}

int main()
{
        std::map<int, std::function<std::vector<bool>(std::vector<bool>)>> parts;
        std::map<std::pair<int, int>, std::pair<int, int>> connections;
        std::map<int, std::string> labels;

        setSourcePart(parts, 0);
        labels[0] = "Source";

        setPartFunction(parts, 1, andFunction);
        labels[1] = "AND Gate";

        setOutputPart(parts, 2);
        labels[2] = "Output";

        connectParts(connections, 0, 0, 1, 0);
        connectParts(connections, 0, 1, 1, 1);
        connectParts(connections, 1, 0, 2, 0);

        visualizePath(parts, connections, labels);

        std::function<std::vector<bool>(std::vector<bool>)> circuitA = compilePart(parts, connections, 2);
        circuitA({true, false});
        circuitA({true, true});

        return 0;
}
