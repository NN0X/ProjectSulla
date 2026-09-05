#ifndef COMPILER_H
#define COMPILER_H

#include <cstdint>
#include <string>
#include <vector>
#include "../appstate.h"
#include "../part.h"

struct CompiledMeta
{
        int inputs;
        int outputs;
        std::vector<std::string> inputLabels;
        std::vector<std::string> outputLabels;
};

std::string transpileToCpp(const AppState& state, bool linkCustomParts = false);
bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName);
Part loadCompiledPart(const std::string& moduleName, int outCount);
void unloadCompiledPart(const std::string& moduleName);

typedef void (*RawTickFn)(const uint8_t* in, uint8_t* out);

RawTickFn loadRawTick(const std::string& moduleName);

std::string sullaPartDir(const std::string& label);
std::string sullaPartSymbol(const std::string& label);
std::string sullaFindStatic(const std::string& label);
std::string sullaFindDynamic(const std::string& label);
std::string sullaFindMeta(const std::string& label);

bool compilePartLibrary(const std::string& cppCode, const std::string& label,
                        bool buildStatic, bool buildDynamic);

#endif
