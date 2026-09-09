#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <vector>
#include <string>
#include "part.h"

std::vector<State> displayPart(std::vector<State> input);

Part getClockPart();

bool parseRamLabel(const std::string& label, bool& sync, int& addrBits, int& dataBits);

Part makeMemoryPart(bool sync, int addrBits, int dataBits);

#endif
