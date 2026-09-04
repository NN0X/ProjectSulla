#ifndef GATES_H
#define GATES_H

#include "part.h"

enum GateEval
{
        EVAL_FOLD,
        EVAL_SOURCE,
        EVAL_SINK,
        EVAL_CLOCK,
        EVAL_CUSTOM
};

struct GateSpec
{
        const char* name;
        GateEval eval;
        char foldOp;
        bool invert;
        bool stateful;
        int fixedInputs;
};

const GateSpec& gateSpec(PartType type);
const char* partTypeName(PartType type);

Part makeFoldPart(char foldOp, bool invert);

#endif
