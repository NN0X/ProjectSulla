#ifndef APPSTATE_H
#define APPSTATE_H

#include <map>
#include <vector>
#include <string>
#include <raylib/raylib.h>
#include "part.h"

struct ContextMenu
{
        bool active = false;
        int targetPartID = -1;
        Vector2 position = {0, 0};
};

struct AppState
{
        std::map<int, Part> parts;
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<int, std::string> labels;
        std::map<int, std::pair<float, float>> positions;
        std::map<int, int> inputCounts;

        Part simulation;
        bool isSimulating = false;
        float targetHZ = 1.0f;
        float simTimer = 0.0f;
        
        std::vector<State> runtimeInput;
        std::vector<State> lastOutputStates;

        int selectedPartID = -1;
        
        int dragPartID = -1;
        Vector2 dragStartMousePos = {0, 0};
        Vector2 dragPartStartPos = {0, 0};
        bool isDragging = false;

        int wireStartPartID = -1;
        int wireStartPin = -1;
        PartPin selectedConnection = {-1, -1};

        int nextID = 100;
        int rootSourceID = 0;
        int rootSinkID = 1;

        bool showSaveDialog = false;
        bool showSideMenu = true;
        bool showHelp = true;
        char saveFileNameBuffer[64] = "circuit";
        
        int draggingNewPartType = -1;

        ContextMenu contextMenu;
        std::vector<std::string> layoutFiles;
};

#endif
