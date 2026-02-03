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
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<int, std::string> labels;

        setSourcePart(parts, 0);
        partTypes[0] = PART_TYPE_SOURCE;
        labels[0] = "Inputs (S, R)";

        setPart(parts, 1, nandPart);
        partTypes[1] = PART_TYPE_NAND;
        labels[1] = "NAND 1";

        setPart(parts, 2, nandPart);
        partTypes[2] = PART_TYPE_NAND;
        labels[2] = "NAND 2";

        setOutputPart(parts, 3);
        partTypes[3] = PART_TYPE_OUTPUT;
        labels[3] = "Output";

        connectParts(connections, {0, 0}, {1, 0}); 
        connectParts(connections, {0, 1}, {2, 1}); 

        connectParts(connections, {1, 0}, {2, 0});
        connectParts(connections, {2, 0}, {1, 1});

        connectParts(connections, {1, 0}, {3, 0});

        visualizePath(parts, connections, labels);
        saveLayout(parts, partTypes, connections, labels, "layouts/sr_latch_layout.json");

        Part srLatch = assemblePart(parts, connections, 3);

        srLatch({STATE_LOW, STATE_HIGH});
        srLatch({STATE_HIGH, STATE_HIGH});
        srLatch({STATE_HIGH, STATE_LOW});
        srLatch({STATE_HIGH, STATE_HIGH});

        return 0;
}
