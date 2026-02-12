#ifndef APPSTATE_H
#define APPSTATE_H

#include <map>
#include <vector>
#include <set>
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
        std::map<int, int> outputCounts; 

        std::map<int, std::vector<State>> sourceValues;

        Part simulation;
        bool isSimulating = false;
        float targetHZ = 1.0f;
        float simTimer = 0.0f;
        unsigned long long stepCount = 0;

        std::vector<State> runtimeInput;
        std::vector<State> lastOutputStates;

        std::set<int> selectedParts;

        int dragPartID = -1; 
        Vector2 dragStartMousePos = {0, 0};
        std::map<int, Vector2> dragStartPositions; 
        bool isDragging = false;
        bool isBoxSelecting = false;
        Vector2 boxSelectStart = {0, 0};

        int wireStartPartID = -1;
        int wireStartPin = -1;
        PartPin selectedConnection = {-1, -1};

        int nextID = 100;
        int rootSourceID = 0;
        int rootSinkID = 1;

        bool showSaveDialog = false;
        bool showLoadDialog = false;
        bool showDeleteConfirm = false;
        bool showOverwriteConfirm = false;
        bool showSideMenu = true;
        bool showHelp = false;
        bool darkMode = true;

        char fileNameBuffer[64] = "circuit";
        std::string layoutToDelete = "";
        std::string pendingSaveFilename = "";

        int draggingNewPartType = -1;
        std::string draggingLayoutFile = "";

        ContextMenu contextMenu;
        std::vector<std::string> layoutFiles;

        Camera2D camera = { {0,0}, {0,0}, 0.0f, 1.0f };

        float hzKeyTimer = 0.0f;
};

#endif
