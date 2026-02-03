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

const std::string STATE_STRINGS[] = {"LOW", "HIGH", "UNDEFINED"};

std::vector<State> andPart(std::vector<State> input);
std::vector<State> orPart(std::vector<State> input);
std::vector<State> notPart(std::vector<State> input);
std::vector<State> nandPart(std::vector<State> input);
std::vector<State> norPart(std::vector<State> input);
std::vector<State> xorPart(std::vector<State> input);
std::vector<State> xnorPart(std::vector<State> input);

#endif // PRIMITIVES_H
