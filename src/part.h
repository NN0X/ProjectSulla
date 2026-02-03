#ifndef PART_H
#define PART_H

#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>

#include "primitives.h"

typedef std::function<std::vector<State>(std::vector<State>)> Part;
typedef std::pair<int, int> PartPin;

void setSourcePart(std::map<int, Part>& parts, int partID);

void setOutputPart(std::map<int, Part>& parts, int partID);

void connectParts(std::map<PartPin, PartPin>& connections, PartPin from, PartPin to);

void setPart(std::map<int, Part>& parts, int partID, Part part);

Part assemblePart(std::map<int, Part> parts, const std::map<PartPin, PartPin>& connections, int partID);

Part getPartFromType(PartType type);

#endif // PART_H
