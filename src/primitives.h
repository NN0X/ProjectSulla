#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <vector>
#include "part.h"

std::vector<State> andPart(std::vector<State> input);
std::vector<State> orPart(std::vector<State> input);
std::vector<State> notPart(std::vector<State> input);
std::vector<State> nandPart(std::vector<State> input);
std::vector<State> norPart(std::vector<State> input);
std::vector<State> xorPart(std::vector<State> input);
std::vector<State> xnorPart(std::vector<State> input);
std::vector<State> displayPart(std::vector<State> input);

Part getClockPart();

#endif
