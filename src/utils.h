#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <set>

#include "part.h"
#include "appstate.h"

void saveLayout(const std::map<int, PartType>& partTypes,
                const std::map<PartPin, PartPin>& connections,
                const std::map<int, std::string>& labels,
                const std::map<int, std::pair<float, float>>& positions,
                const std::map<int, int>& inputCounts,
                const std::map<int, int>& outputCounts,
                const std::string& filename);

int loadLayout(AppState& state, const std::string& filename);

Part loadLayoutAsPart(const std::string& filename, int& nInputs, int& nOutputs);

std::set<int> importLayout(AppState& state, const std::string& filename, float mouseX, float mouseY);

#endif
