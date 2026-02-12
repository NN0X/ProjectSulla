#ifndef PART_H
#define PART_H

#include <vector>
#include <map>
#include <functional>
#include <string>

enum State
{
        STATE_LOW,
        STATE_HIGH,
        STATE_UNDEFINED
};

enum PartType
{
        PART_TYPE_AND,
        PART_TYPE_OR,
        PART_TYPE_NOT,
        PART_TYPE_NAND,
        PART_TYPE_NOR,
        PART_TYPE_XOR,
        PART_TYPE_XNOR,
        PART_TYPE_SOURCE,
        PART_TYPE_OUTPUT
};

inline const char* PART_TYPE_NAMES[] = {
        "AND",
        "OR",
        "NOT",
        "NAND",
        "NOR",
        "XOR",
        "XNOR",
        "SOURCE",
        "OUTPUT"
};

typedef std::vector<State> Input;
typedef std::function<std::vector<State>(Input)> Part;
typedef std::pair<int, int> PartPin;

void setSourcePart(std::map<int, Part>& parts, int partID);
void setOutputPart(std::map<int, Part>& parts, int partID);
void setPart(std::map<int, Part>& parts, int partID, Part part);
void connectParts(std::map<PartPin, PartPin>& connections, PartPin from, PartPin to);
Part assemblePart(std::map<int, Part> parts, const std::map<PartPin, PartPin>& connections, int partID);
Part getPartFromType(PartType type);

#endif
