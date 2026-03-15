#ifndef COMPILER_H
#define COMPILER_H

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

std::string transpileToCpp(const AppState& state);
bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName);
Part loadCompiledPart(const std::string& moduleName, int outCount);
void unloadCompiledPart(const std::string& moduleName);

#endif
