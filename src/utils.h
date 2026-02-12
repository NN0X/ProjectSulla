#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <utility>
#include <string>

#include "part.h"
#include "primitives.h"

void saveLayout(const std::map<int, Part>& parts, const std::map<int, PartType>& partTypes,
                const std::map<PartPin, PartPin>& connections,
                const std::map<int, std::string>& labels,
                const std::map<int, std::pair<float, float>>& positions,
                const std::map<int, int>& inputCounts,
                const std::string& filename);

int loadLayout(std::map<int, Part>& parts, std::map<int, PartType>& partTypes,
               std::map<PartPin, PartPin>& connections,
               std::map<int, std::string>& labels,
               std::map<int, std::pair<float, float>>& positions,
               std::map<int, int>& inputCounts,
               const std::string& filename);

#endif // UTILS_H
