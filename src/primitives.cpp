#include "primitives.h"

#include <iostream>
#include <vector>

std::vector<State> andPart(std::vector<State> input)
{
        if (input.empty())
        {
                return {STATE_LOW};
        }
        for (size_t i = 0; i < input.size(); ++i)
        {
                if (input[i] != STATE_HIGH)
                {
                        return {STATE_LOW};
                }
        }
        return {STATE_HIGH};
}

std::vector<State> orPart(std::vector<State> input)
{
        if (input.empty())
        {
                return {STATE_LOW};
        }
        for (size_t i = 0; i < input.size(); ++i)
        {
                if (input[i] == STATE_HIGH)
                {
                        return {STATE_HIGH};
                }
        }
        return {STATE_LOW};
}

std::vector<State> notPart(std::vector<State> input)
{
        if (input.empty())
        {
                return {STATE_LOW};
        }
        return {(input[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

std::vector<State> nandPart(std::vector<State> input)
{
        std::vector<State> res = andPart(input);
        return {(res[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

std::vector<State> norPart(std::vector<State> input)
{
        std::vector<State> res = orPart(input);
        return {(res[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

std::vector<State> xorPart(std::vector<State> input)
{
        if (input.empty())
        {
                return {STATE_LOW};
        }
        int highCount = 0;
        for (size_t i = 0; i < input.size(); ++i)
        {
                if (input[i] == STATE_HIGH)
                {
                        highCount++;
                }
        }
        return {(highCount % 2 != 0) ? STATE_HIGH : STATE_LOW};
}

std::vector<State> xnorPart(std::vector<State> input)
{
        std::vector<State> res = xorPart(input);
        return {(res[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

Part getClockPart()
{
        return [state = STATE_LOW](std::vector<State> input) mutable -> std::vector<State> {
                state = (state == STATE_HIGH) ? STATE_LOW : STATE_HIGH;
                return {state};
        };
}
