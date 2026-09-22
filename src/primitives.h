#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <cstdint>
#include <vector>
#include <string>
#include "part.h"

std::vector<State> displayPart(std::vector<State> input);

Part getClockPart();

bool parseRamLabel(const std::string& label, bool& sync, int& addrBits, int& dataBits);

Part makeMemoryPart(bool sync, int addrBits, int dataBits);
Part makeRomPart(const std::vector<uint32_t>& contents, int addrBits, int dataBits);
std::vector<uint32_t> parseHexDump(const std::string& text, int dataBits);

bool parseArithLabel(const std::string& label, bool& isMul, int& width);

Part makeArithPart(bool isMul, int width);

#endif
