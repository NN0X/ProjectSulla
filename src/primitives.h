#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <vector>
#include <string>

typedef enum State
{
    STATE_LOW = 0,
    STATE_HIGH = 1,
    STATE_UNDEFINED = 2
} State;

typedef enum PartType
{
    PART_TYPE_SOURCE,
    PART_TYPE_OUTPUT,
    PART_TYPE_AND,
    PART_TYPE_OR,
    PART_TYPE_NOT,
    PART_TYPE_NAND,
    PART_TYPE_NOR,
    PART_TYPE_XOR,
    PART_TYPE_XNOR
} PartType;

const std::string STATE_STRINGS[] = {
        [STATE_LOW] = "LOW",
        [STATE_HIGH] = "HIGH",
        [STATE_UNDEFINED] = "UNDEFINED"
};

const std::string PART_TYPE_STRINGS[] = {
        [PART_TYPE_SOURCE] = "SOURCE",
        [PART_TYPE_OUTPUT] = "OUTPUT",
        [PART_TYPE_AND] = "AND",
        [PART_TYPE_OR] = "OR",
        [PART_TYPE_NOT] = "NOT",
        [PART_TYPE_NAND] = "NAND",
        [PART_TYPE_NOR] = "NOR",
        [PART_TYPE_XOR] = "XOR",
        [PART_TYPE_XNOR] = "XNOR"
};

std::vector<State> andPart(std::vector<State> input);
std::vector<State> orPart(std::vector<State> input);
std::vector<State> notPart(std::vector<State> input);
std::vector<State> nandPart(std::vector<State> input);
std::vector<State> norPart(std::vector<State> input);
std::vector<State> xorPart(std::vector<State> input);
std::vector<State> xnorPart(std::vector<State> input);

#endif // PRIMITIVES_H
