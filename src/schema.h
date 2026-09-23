#ifndef SULLA_SCHEMA_H
#define SULLA_SCHEMA_H

#include <vector>
#include <string>
#include <cstdint>

#include "part.h"

#define SULLA_SCHEMA_VERSION 3

typedef struct SerializablePart
{
        int id;
        PartType type;
        std::string label;
        float x;
        float y;
        int numInputs;
        int numOutputs;
        std::vector<uint32_t> romData;
} SPart;

typedef struct SerializableConnectionPin
{
        int id;
        int pin;
} SCPin;

typedef struct SerializablePoint
{
        float x;
        float y;
} SPoint;

typedef struct SerializableConnection
{
        SCPin from;
        SCPin to;
        std::vector<SPoint> waypoints;
} SConn;

struct LayoutData
{
        int version = 0;
        std::vector<SPart> parts;
        std::vector<SConn> connections;
};

#endif
