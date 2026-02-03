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

std::function<std::vector<bool>(std::vector<bool>)> createLatch()
{
        std::vector<bool> state = {false};

        return [state](std::vector<bool> input) mutable -> std::vector<bool>
        {
                for (size_t i = 0; i < input.size(); ++i)
                {
                        state[0] = state[0] || input[i];
                }
                return state;
        };
}

std::function<std::vector<bool>(std::vector<bool>)> createFlipFlop()
{
        std::vector<bool> state = {false};

        return [state](std::vector<bool> input) mutable -> std::vector<bool>
        {
                for (size_t i = 0; i < input.size(); ++i)
                {
                        state[0] = !state[0];
                }
                return state;
        };
}

void visualizePath(const std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>& parts,
                   const std::map<std::pair<int, int>, std::pair<int, int>>& connections,
                   const std::map<int, std::string>& labels)
{
        for (std::map<int, std::function<std::vector<bool>(std::vector<bool>)>>::const_iterator partIt = parts.begin(); partIt != parts.end(); ++partIt)
        {
                int sourceID = partIt->first;
                std::string sourceName = "Unknown";
                if (labels.find(sourceID) != labels.end())
                {
                        sourceName = labels.find(sourceID)->second;
                }

                for (std::map<std::pair<int, int>, std::pair<int, int>>::const_iterator connIt = connections.begin(); connIt != connections.end(); ++connIt)
                {
                        if (connIt->second.first == sourceID)
                        {
                                int destID = connIt->first.first;
                                int destPin = connIt->first.second;
                                int sourcePin = connIt->second.second;
                                std::string destName = "Unknown";

                                if (labels.find(destID) != labels.end())
                                {
                                        destName = labels.find(destID)->second;
                                }

                                std::cout << "[" << sourceID << "] " << sourceName << " (Pin " << sourcePin << ")";
                                std::cout << " --------> ";
                                std::cout << "[" << destID << "] " << destName << " (Pin " << destPin << ")\n";
                        }
                }
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

        setPartFunction(parts, 3, createLatch());
        labels[3] = "Latch";

        connectParts(connections, 0, 0, 1, 0);
        connectParts(connections, 0, 1, 1, 1);
        connectParts(connections, 1, 0, 3, 0);
        connectParts(connections, 3, 0, 2, 0);

        visualizePath(parts, connections, labels);

        std::function<std::vector<bool>(std::vector<bool>)> circuitA = compilePart(parts, connections, 2);
        circuitA({true, false});
        circuitA({true, true});

        parts.clear();
        connections.clear();
        labels.clear();

        setSourcePart(parts, 0);
        labels[0] = "Source";
        setOutputPart(parts, 1);
        labels[1] = "Output";
        setPartFunction(parts, 2, createLatch());
        labels[2] = "Latch";

        connectParts(connections, 0, 0, 2, 0);
        connectParts(connections, 2, 0, 1, 0);
        visualizePath(parts, connections, labels);

        std::function<std::vector<bool>(std::vector<bool>)> circuitB = compilePart(parts, connections, 1);
        circuitB({true});
        circuitB({false});

        parts.clear();
        connections.clear();
        labels.clear();

        setSourcePart(parts, 0);
        labels[0] = "Source";
        setOutputPart(parts, 1);
        labels[1] = "Output";
        setPartFunction(parts, 2, createFlipFlop());
        labels[2] = "Flip-Flop";

        connectParts(connections, 0, 0, 2, 0);
        connectParts(connections, 2, 0, 1, 0);
        visualizePath(parts, connections, labels);

        std::function<std::vector<bool>(std::vector<bool>)> circuitC = compilePart(parts, connections, 1);
        circuitC({true});
        circuitC({true});
        circuitC({true});

        return 0;
}
