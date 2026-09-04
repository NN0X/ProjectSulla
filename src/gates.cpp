#include "gates.h"

static const GateSpec GATES[] = {
        { "AND",     EVAL_FOLD,   '&', false, false, -1 },
        { "OR",      EVAL_FOLD,   '|', false, false, -1 },
        { "NOT",     EVAL_FOLD,    0,  true,  false,  1 },
        { "NAND",    EVAL_FOLD,   '&', true,  false, -1 },
        { "NOR",     EVAL_FOLD,   '|', true,  false, -1 },
        { "XOR",     EVAL_FOLD,   '^', false, false, -1 },
        { "XNOR",    EVAL_FOLD,   '^', true,  false, -1 },
        { "SOURCE",  EVAL_SOURCE,  0,  false, false,  0 },
        { "OUTPUT",  EVAL_SINK,    0,  false, false,  1 },
        { "CUSTOM",  EVAL_CUSTOM,  0,  false, false, -1 },
        { "CLOCK",   EVAL_CLOCK,   0,  false, true,   0 },
        { "DISPLAY", EVAL_SINK,    0,  false, false, -1 },
};

const GateSpec& gateSpec(PartType type) { return GATES[type]; }
const char* partTypeName(PartType type) { return GATES[type].name; }

Part makeFoldPart(char foldOp, bool invert)
{
        return [foldOp, invert](std::vector<State> input) -> std::vector<State> {
                int acc = 0;
                if (!input.empty())
                {
                        acc = (input[0] == STATE_HIGH) ? 1 : 0;
                        for (size_t i = 1; i < input.size(); ++i)
                        {
                                int b = (input[i] == STATE_HIGH) ? 1 : 0;
                                if (foldOp == '&') acc &= b;
                                else if (foldOp == '|') acc |= b;
                                else if (foldOp == '^') acc ^= b;
                        }
                }
                if (invert) acc = !acc;
                return { acc ? STATE_HIGH : STATE_LOW };
        };
}
