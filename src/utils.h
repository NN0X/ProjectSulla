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

void visualizePath(const std::map<int, Part>& parts, const std::map<PartPin, PartPin>& connections,
                   const std::map<int, std::string>& labels);

void saveLayout(const std::map<int, Part>& parts, const std::map<PartPin, PartPin>& connections,
              const std::map<int, std::string>& labels, const std::string& filename);

void loadLayout(std::map<int, Part>& parts, std::map<PartPin, PartPin>& connections,
              std::map<int, std::string>& labels, const std::string& filename);

#endif // UTILS_H
