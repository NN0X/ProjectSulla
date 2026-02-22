#ifndef COMPILER_H
#define COMPILER_H

#include <string>

#include "../appstate.h"
#include "../part.h"

std::string transpileToC(const AppState& state);
std::string getExportSignature();
bool checkCompilerAvailability();
bool compileSharedLibrary(const std::string& cCode, const std::string& moduleName);
Part loadCompiledPart(const std::string& moduleName, int outCount);

#endif
