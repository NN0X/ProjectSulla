#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include "../appstate.h"
#include "../part.h"

std::string transpileToCpp(const AppState& state);
bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName);
Part loadCompiledPart(const std::string& moduleName, int outCount);

#endif
