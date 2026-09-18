#ifndef APPSTATE_H
#define APPSTATE_H

#include "theme.h"
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

struct BenchmarkResult
{
        bool valid = false;
        bool running = false;
        double interpNs = 0.0;
        double inlineNs = 0.0;
        double linkNs = 0.0;
        bool inlineOk = false;
        bool linkOk = false;
};

struct CircuitSnapshot
{
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<PartPin, std::vector<Vector2>> connectionWaypoints;
        std::map<PartPin, int> connColorIdx;
        int nextConnColor = 0;
        std::map<int, std::string> labels;
        std::map<int, std::pair<float, float>> positions;
        std::map<int, int> inputCounts;
        std::map<int, int> outputCounts;
        std::map<int, std::vector<State>> sourceValues;
        int nextID = 100;
};

struct AppState
{
        std::map<int, Part> parts;
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<PartPin, std::vector<Vector2>> connectionWaypoints;
        std::map<PartPin, int> connColorIdx;
        int nextConnColor = 0;
        PartPin hoveredNet = {-1, -1};
        std::map<PartPin, std::vector<Vector2>> wirePaths;
        PartPin dragWpConn = {-1, -1};
        int dragWpIdx = -1;
        double lastWireClickTime = 0.0;
        Vector2 lastWireClickPos = {0, 0};
        Vector2 wireDragStartPos = {0, 0};
        std::map<int, std::string> labels;
        std::map<int, std::pair<float, float>> positions;
        std::map<int, int> inputCounts;
        std::map<int, int> outputCounts; 

        std::map<int, std::vector<State>> sourceValues;

        std::map<int, std::vector<std::string>> inputPinLabels;
        std::map<int, std::vector<std::string>> outputPinLabels;

        Part simulation;
        bool isSimulating = false;
        float targetHZ = 1.0f;
        float simTimer = 0.0f;
        unsigned long long stepCount = 0;
        float actualHz = 0.0f;
        float hzSampleTimer = 0.0f;
        unsigned long long hzSampleBase = 0;
        bool simSaturated = false;

        std::vector<State> runtimeInput;
        std::vector<State> lastOutputStates;
        bool visualizeSignals = false;
        bool nativeActive = false;
        size_t nativeHash = 0;
        bool captureNets = false;
        std::map<int, std::vector<State>> netStates;

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
        std::vector<CircuitSnapshot> undoStack;
        std::vector<CircuitSnapshot> redoStack;
        CircuitSnapshot histLast;
        size_t histHash = 0;
        bool histInit = false;
        int rootSourceID = 0;
        int rootSinkID = 1;

        bool showSaveDialog = false;
        bool showLoadDialog = false;
        bool showCompileDialog = false;
        bool showDeleteConfirm = false;
        bool showTidyConfirm = false;
        bool showOverwriteConfirm = false;
        bool showQuitConfirm = false;
        bool showRenameDialog = false;
        bool showSideMenu = true;
        float sidebarWidth = 200.0f;
        bool sidebarResizing = false;
        bool sidebarCollapsed[3] = {false, false, false};
        float sidebarScroll = 0.0f;
        float sidebarMaxScroll = 0.0f;
        bool showHelp = false;
        bool showBenchmark = false;
        bool benchmarkPending = false;
        BenchmarkResult benchmark;
        bool darkMode = true;
        Theme theme;
        bool shouldQuit = false;
        bool linkCustomParts = false;
        bool compileStatic = true;
        bool compileDynamic = true;

        char fileNameBuffer[64] = "circuit";
        int renamePartID = -1;
        std::string layoutToDelete = "";
        std::string partToDelete = "";
        std::string pendingSaveFilename = "";

        int draggingNewPartType = -1;
        std::string draggingLayoutFile = "";
        std::string draggingCompiledFile = "";

        ContextMenu contextMenu;
        std::vector<std::string> layoutFiles;
        std::vector<std::string> compiledModules;
        std::map<std::string, int> compiledInputs;
        std::map<std::string, int> compiledOutputs;
        std::map<std::string, std::vector<std::string>> compiledInputLabels;
        std::map<std::string, std::vector<std::string>> compiledOutputLabels;

        Camera2D camera = { {0,0}, {0,0}, 0.0f, 1.0f };

        float hzKeyTimer = 0.0f;
        float cursorTimer = 0.0f;
};

#endif
