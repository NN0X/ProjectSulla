#include "primitives.h"

#include <vector>

std::vector<State> displayPart(std::vector<State> input)
{
        return input;
}

Part getClockPart()
{
        return [state = STATE_LOW](std::vector<State> input) mutable -> std::vector<State> {
                state = (state == STATE_HIGH) ? STATE_LOW : STATE_HIGH;
                return {state};
        };
}
