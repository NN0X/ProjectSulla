#include "part.h"

#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>

#include "primitives.h"

void setSourcePart(std::map<int, Part>& parts, int partID)
{
        parts[partID] = [](std::vector<State> input) -> std::vector<State>
        {
                return input;
        };
}

void setOutputPart(std::map<int, Part>& parts, int partID)
{
        parts[partID] = [](std::vector<State> input) -> std::vector<State>
        {
                return input;
        };
}

void connectParts(std::map<PartPin, PartPin>& connections, PartPin from, PartPin to)
{
        connections[to] = from;
}

void setPart(std::map<int, Part>& parts, int partID, Part part)
{
        parts[partID] = part;
}

Part assemblePart(std::map<int, Part> parts, const std::map<PartPin, PartPin>& connections, int partID)
{
        std::map<int, std::vector<State>> lastOutputs;

        return [parts, connections, partID, lastOutputs](std::vector<State> runtimeInput) mutable -> std::vector<State>
        {
                std::map<int, std::vector<State>> cache;
                std::vector<int> recursionStack;
                std::function<std::vector<State>(int)> resolve;

                resolve = [&](int currentID) -> std::vector<State>
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
                                        return {STATE_UNDEFINED};
                                }
                        }

                        if (parts.find(currentID) == parts.end())
                        {
                                return {STATE_UNDEFINED};
                        }

                        recursionStack.push_back(currentID);

                        std::vector<State> collectedInputs;
                        std::map<PartPin, PartPin>::const_iterator it = connections.lower_bound(std::make_pair(currentID, -1));
                        bool hasConnections = (it != connections.end() && it->first.first == currentID);

                        while (it != connections.end() && it->first.first == currentID)
                        {
                                int inputIdx = it->first.second;
                                int sourceID = it->second.first;
                                int sourceOutputIdx = it->second.second;
                                std::vector<State> sourceResult = resolve(sourceID);

                                if (collectedInputs.size() <= (size_t)inputIdx)
                                {
                                        collectedInputs.resize(inputIdx + 1, STATE_UNDEFINED);
                                }

                                if ((size_t)sourceOutputIdx < sourceResult.size())
                                {
                                        collectedInputs[inputIdx] = sourceResult[sourceOutputIdx];
                                }
                                it++;
                        }

                        std::vector<State> result;
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
                        return result;
                };

                std::vector<State> finalResult = resolve(partID);
                for (std::map<int, std::vector<State>>::iterator it = cache.begin(); it != cache.end(); ++it)
                {
                        lastOutputs[it->first] = it->second;
                }
                return finalResult;
        };
}

Part getPartFromType(PartType type)
{
        switch (type)
        {
        case PART_TYPE_AND:
                return andPart;
        case PART_TYPE_OR:
                return orPart;
        case PART_TYPE_NOT:
                return notPart;
        case PART_TYPE_NAND:
                return nandPart;
        case PART_TYPE_NOR:
                return norPart;
        case PART_TYPE_XOR:
                return xorPart;
        case PART_TYPE_XNOR:
                return xnorPart;
        default:
                std::cerr << "Error: Unknown part type " << PART_TYPE_NAMES[type] << std::endl;
                std::exit(1);
        }
}
