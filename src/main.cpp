#include <iostream>
#include <map>
#include <vector>
#include <functional>

#include "part.h"
#include "primitives.h"
#include "utils.h"

int main()
{
        std::map<int, Part> parts;
        std::map<PartPin, PartPin> connections;
        std::map<int, std::string> labels;

        setSourcePart(parts, 0);
        labels[0] = "Inputs (S, R)";

        setPart(parts, 1, nandPart);
        labels[1] = "NAND 1";
        setPart(parts, 2, nandPart);
        labels[2] = "NAND 2";

        setOutputPart(parts, 3);
        labels[3] = "Output";

        connectParts(connections, {0, 0}, {1, 0}); 
        connectParts(connections, {0, 1}, {2, 1}); 

        connectParts(connections, {1, 0}, {2, 0});
        connectParts(connections, {2, 0}, {1, 1});

        connectParts(connections, {1, 0}, {3, 0});

        visualizePath(parts, connections, labels);

        Part flipFlop = assemblePart(parts, connections, 3);

        flipFlop({STATE_LOW, STATE_HIGH});
        flipFlop({STATE_HIGH, STATE_HIGH});
        flipFlop({STATE_HIGH, STATE_LOW});
        flipFlop({STATE_HIGH, STATE_HIGH});

        return 0;
}
