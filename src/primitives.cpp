#include "primitives.h"

#include <vector>

std::vector<State> andPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] == STATE_HIGH && input[1] == STATE_HIGH) ? STATE_HIGH : STATE_LOW};
}

std::vector<State> orPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] == STATE_HIGH || input[1] == STATE_HIGH) ? STATE_HIGH : STATE_LOW};
}

std::vector<State> notPart(std::vector<State> input)
{
        if (input.size() < 1)
        {
                return {};
        }
        return {(input[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

std::vector<State> nandPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] == STATE_HIGH && input[1] == STATE_HIGH) ? STATE_LOW : STATE_HIGH};
}

std::vector<State> norPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] == STATE_LOW && input[1] == STATE_LOW) ? STATE_HIGH : STATE_LOW};
}

std::vector<State> xorPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] != input[1]) ? STATE_HIGH : STATE_LOW};
}

std::vector<State> xnorPart(std::vector<State> input)
{
        if (input.size() < 2)
        {
                return {};
        }
        return {(input[0] == input[1]) ? STATE_HIGH : STATE_LOW};
}
