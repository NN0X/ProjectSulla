#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <format>

#include "raylib/raylib.h"
#include "raylib/raymath.h"

#include "part.h"
#include "primitives.h"
#include "utils.h"

#define GRID_SIZE 32.0f
#define PART_RADIUS 20.0f
#define PIN_RADIUS 5.0f

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
        bool showHelp = true;
        float targetHZ = 1.0f;
        float simTimer = 0.0f;

        std::vector<State> runtimeInput;
        std::vector<State> lastOutputStates;

        int selectedPartID = -1;
        int dragPartID = -1;
        int wireStartPartID = -1;
        int wireStartPin = -1;

        int nextID = 100;
        int rootSourceID = 0;
        int rootSinkID = 1;
};

void initApp(AppState& state)
{
        setSourcePart(state.parts, state.rootSourceID);
        setSourcePart(state.parts, state.rootSinkID);
}

void recompileSimulation(AppState& state)
{
        std::map<PartPin, PartPin> simConnections = state.connections;

        int srcIdx = 0;
        int outIdx = 0;
        for(std::map<int, PartType>::iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                if (it->second == PART_TYPE_SOURCE)
                {
                        simConnections[{it->first, 0}] = {state.rootSourceID, srcIdx++};
                }
                if (it->second == PART_TYPE_OUTPUT)
                {
                        simConnections[{state.rootSinkID, outIdx++}] = {it->first, 0};
                }
        }
        state.simulation = assemblePart(state.parts, simConnections, state.rootSinkID);
}

void handleHotkeys(AppState& state)
{
        if (IsKeyPressed(KEY_H)) state.showHelp = !state.showHelp;
        if (IsKeyPressed(KEY_SPACE)) state.isSimulating = !state.isSimulating;
        if (IsKeyPressed(KEY_UP)) state.targetHZ *= 2.0f;
        if (IsKeyPressed(KEY_DOWN)) state.targetHZ *= 0.5f;

        if (IsKeyPressed(KEY_S)) 
        {
                saveLayout(state.parts, state.partTypes, state.connections, state.labels, 
                           state.positions, state.inputCounts, "layout.json");
        }
        if (IsKeyPressed(KEY_L)) 
        {
                state.nextID = loadLayout(state.parts, state.partTypes, state.connections, state.labels, 
                                          state.positions, state.inputCounts, "layout.json") + 100;
                recompileSimulation(state);
        }

        int dropType = -1;
        if (IsKeyPressed(KEY_ONE)) dropType = PART_TYPE_AND;
        if (IsKeyPressed(KEY_TWO)) dropType = PART_TYPE_OR;
        if (IsKeyPressed(KEY_THREE)) dropType = PART_TYPE_NOT;
        if (IsKeyPressed(KEY_FOUR)) dropType = PART_TYPE_NAND;
        if (IsKeyPressed(KEY_FIVE)) dropType = PART_TYPE_SOURCE;
        if (IsKeyPressed(KEY_SIX)) dropType = PART_TYPE_OUTPUT;

        if (dropType != -1)
        {
                int id = state.nextID++;
                if (dropType == PART_TYPE_SOURCE) setSourcePart(state.parts, id);
                else if (dropType == PART_TYPE_OUTPUT) setOutputPart(state.parts, id);
                else setPart(state.parts, id, getPartFromType((PartType)dropType));

                state.partTypes[id] = (PartType)dropType;
                state.positions[id] = {GetMousePosition().x, GetMousePosition().y};
                state.inputCounts[id] = 2;
                state.labels[id] = PART_TYPE_STRINGS[dropType];
                state.simulation = nullptr;
        }

        if (IsKeyPressed(KEY_DELETE) && state.selectedPartID != -1)
        {
                state.parts.erase(state.selectedPartID);
                state.partTypes.erase(state.selectedPartID);
                state.positions.erase(state.selectedPartID);
                state.inputCounts.erase(state.selectedPartID);

                std::vector<PartPin> toRemove;
                for(std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                {
                        if (it->first.first == state.selectedPartID || it->second.first == state.selectedPartID)
                        {
                                toRemove.push_back(it->first);
                        }
                }
                for(size_t i=0; i<toRemove.size(); ++i) state.connections.erase(toRemove[i]);

                state.selectedPartID = -1;
                state.simulation = nullptr;
        }
}

void handleMouseInput(AppState& state)
{
        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
                bool hitSomething = false;
                for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                {
                        int id = it->first;
                        Vector2 pos = {it->second.first, it->second.second};

                        int inCount = state.inputCounts.count(id) ? state.inputCounts[id] : 2;
                        if (state.partTypes[id] == PART_TYPE_SOURCE) inCount = 0;

                        for (int i = 0; i < inCount; ++i)
                        {
                                Vector2 pinPos = {pos.x - PART_RADIUS, pos.y - (inCount - 1) * 10.0f + i * 20.0f};
                                if (CheckCollisionPointCircle(mousePos, pinPos, PIN_RADIUS * 1.5f))
                                {
                                        hitSomething = true;
                                        if (state.wireStartPartID != -1)
                                        {
                                                if (state.connections.count({id, i}) == 0)
                                                {
                                                        state.connections[{id, i}] = {state.wireStartPartID, state.wireStartPin};
                                                        state.simulation = nullptr; 
                                                }
                                                state.wireStartPartID = -1;
                                                state.wireStartPin = -1;
                                        }
                                        break;
                                }
                        }
                        if (hitSomething) break;

                        int outCount = 1;
                        if (state.partTypes[id] == PART_TYPE_OUTPUT) outCount = 0;

                        for (int i = 0; i < outCount; ++i)
                        {
                                Vector2 pinPos = {pos.x + PART_RADIUS, pos.y};
                                if (CheckCollisionPointCircle(mousePos, pinPos, PIN_RADIUS * 1.5f))
                                {
                                        hitSomething = true;
                                        state.wireStartPartID = id;
                                        state.wireStartPin = i;
                                        break;
                                }
                        }
                        if (hitSomething) break;

                        if (CheckCollisionPointCircle(mousePos, pos, PART_RADIUS))
                        {
                                hitSomething = true;
                                if (state.partTypes[id] == PART_TYPE_SOURCE)
                                {
                                        size_t index = 0;
                                        for(std::map<int, PartType>::iterator srcIt = state.partTypes.begin(); srcIt != state.partTypes.end(); ++srcIt)
                                        {
                                                if (srcIt->second == PART_TYPE_SOURCE)
                                                {
                                                        if (srcIt->first == id) break;
                                                        index++;
                                                }
                                        }
                                        if (state.runtimeInput.size() <= index) state.runtimeInput.resize(index + 1, STATE_LOW);
                                        state.runtimeInput[index] = (state.runtimeInput[index] == STATE_LOW) ? STATE_HIGH : STATE_LOW;
                                }
                                else
                                {
                                        state.dragPartID = id;
                                        state.selectedPartID = id;
                                }
                        }
                }

                if (!hitSomething && state.dragPartID == -1)
                {
                        state.wireStartPartID = -1;
                        state.selectedPartID = -1;
                }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
                state.dragPartID = -1;
        }

        if (state.dragPartID != -1)
        {
                float gx = round(mousePos.x / GRID_SIZE) * GRID_SIZE;
                float gy = round(mousePos.y / GRID_SIZE) * GRID_SIZE;
                state.positions[state.dragPartID] = {gx, gy};
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && state.selectedPartID != -1)
        {
                Vector2 pos = {state.positions[state.selectedPartID].first, state.positions[state.selectedPartID].second};
                if (mousePos.x < pos.x)
                {
                        state.inputCounts[state.selectedPartID]++;
                        state.simulation = nullptr;
                }
                else if (mousePos.x > pos.x && state.inputCounts[state.selectedPartID] > 0)
                {
                        state.inputCounts[state.selectedPartID]--;
                        state.simulation = nullptr;
                }
        }
}

void updateSimulation(AppState& state)
{
        if (!state.simulation)
        {
                recompileSimulation(state);
        }

        bool step = false;
        if (state.isSimulating)
        {
                state.simTimer += GetFrameTime();
                float stepTime = 1.0f / state.targetHZ;
                if (state.simTimer >= stepTime)
                {
                        step = true;
                        state.simTimer = 0.0f;
                }
        }
        else if (IsKeyPressed(KEY_RIGHT))
        {
                step = true;
        }

        if (step && state.simulation)
        {
                state.lastOutputStates = state.simulation(state.runtimeInput);
        }
}

void drawGrid()
{
        for (float x = 0; x < GetScreenWidth(); x += GRID_SIZE) DrawLine(x, 0, x, GetScreenHeight(), LIGHTGRAY);
        for (float y = 0; y < GetScreenHeight(); y += GRID_SIZE) DrawLine(0, y, GetScreenWidth(), y, LIGHTGRAY);
}

void drawWires(AppState& state)
{
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                Vector2 start = {state.positions[it->second.first].first + PART_RADIUS, state.positions[it->second.first].second};

                int inCount = state.inputCounts[it->first.first];
                float yOff = -(inCount - 1) * 10.0f + it->first.second * 20.0f;
                Vector2 end = {state.positions[it->first.first].first - PART_RADIUS, state.positions[it->first.first].second + yOff};

                float midX = (start.x + end.x) / 2.0f;
                DrawLineEx(start, {midX, start.y}, 2.0f, BLACK);
                DrawLineEx({midX, start.y}, {midX, end.y}, 2.0f, BLACK);
                DrawLineEx({midX, end.y}, end, 2.0f, BLACK);
        }

        if (state.wireStartPartID != -1)
        {
                Vector2 start = {state.positions[state.wireStartPartID].first + PART_RADIUS, state.positions[state.wireStartPartID].second};
                DrawLineV(start, GetMousePosition(), BLACK);
        }
}

void drawParts(AppState& state)
{
        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
        {
                int id = it->first;
                Vector2 pos = {it->second.first, it->second.second};
                PartType type = state.partTypes[id];

                Color color = LIGHTGRAY;
                if (type == PART_TYPE_AND) color = GREEN;
                else if (type == PART_TYPE_NAND) color = RED;
                else if (type == PART_TYPE_OR) color = BLUE;
                else if (type == PART_TYPE_SOURCE)
                {
                        size_t index = 0;
                        for(std::map<int, PartType>::iterator srcIt = state.partTypes.begin(); srcIt != state.partTypes.end(); ++srcIt)
                        {
                                if (srcIt->second == PART_TYPE_SOURCE)
                                {
                                        if (srcIt->first == id) break;
                                        index++;
                                }
                        }
                        State s = (index < state.runtimeInput.size()) ? state.runtimeInput[index] : STATE_LOW;
                        color = (s == STATE_HIGH) ? GREEN : DARKGREEN;
                }
                else if (type == PART_TYPE_OUTPUT)
                {
                        size_t index = 0;
                        for(std::map<int, PartType>::iterator outIt = state.partTypes.begin(); outIt != state.partTypes.end(); ++outIt)
                        {
                                if (outIt->second == PART_TYPE_OUTPUT)
                                {
                                        if (outIt->first == id) break;
                                        index++;
                                }
                        }
                        State s = (index < state.lastOutputStates.size()) ? state.lastOutputStates[index] : STATE_LOW;
                        color = (s == STATE_HIGH) ? RED : DARKGRAY;
                }

                if (id == state.selectedPartID) DrawCircleV(pos, PART_RADIUS + 2.0f, YELLOW);
                DrawCircleV(pos, PART_RADIUS, color);
                DrawText(state.labels[id].c_str(), pos.x - 10, pos.y - 10, 10, BLACK);

                int inCount = state.inputCounts.count(id) ? state.inputCounts[id] : 2;
                if (type != PART_TYPE_SOURCE)
                {
                        for (int i = 0; i < inCount; ++i)
                        {
                                Vector2 pinPos = {pos.x - PART_RADIUS, pos.y - (inCount - 1) * 10.0f + i * 20.0f};
                                DrawCircleV(pinPos, PIN_RADIUS, DARKGRAY);
                        }
                }

                if (type != PART_TYPE_OUTPUT)
                {
                        Vector2 pinPos = {pos.x + PART_RADIUS, pos.y};
                        DrawCircleV(pinPos, PIN_RADIUS, DARKGRAY);
                }
        }
}

void drawUI(const AppState& state)
{
        DrawText(std::format("HZ: {:.1f}", state.targetHZ).c_str(), 10, 10, 20, BLACK);
        if (state.simTimer > (1.0f/state.targetHZ) * 2.0f && state.isSimulating) 
        {
                DrawText("LAGGING", 10, 40, 20, RED);
        }

        if (state.showHelp)
        {
                float helpW = 300;
                float helpH = 350;
                float helpX = (GetScreenWidth() - helpW) / 2.0f;
                float helpY = (GetScreenHeight() - helpH) / 2.0f;

                DrawRectangle(helpX - 5, helpY - 5, helpW + 10, helpH + 10, DARKGRAY);
                DrawRectangle(helpX, helpY, helpW, helpH, RAYWHITE);

                int y = helpY + 10;
                int x = helpX + 10;
                DrawText("HELP (Press H to toggle)", x, y, 20, BLACK); y += 30;
                DrawText("---------------------------", x, y, 20, GRAY); y += 20;
                DrawText("Add Parts:", x, y, 20, DARKBLUE); y += 25;
                DrawText("  1: AND    4: NAND", x, y, 10, BLACK); y += 15;
                DrawText("  2: OR     5: SOURCE (Switch)", x, y, 10, BLACK); y += 15;
                DrawText("  3: NOT    6: OUTPUT (Light)", x, y, 10, BLACK); y += 25;

                DrawText("Editing:", x, y, 20, DARKBLUE); y += 25;
                DrawText("  Drag Body: Move Part", x, y, 10, BLACK); y += 15;
                DrawText("  Drag Pin to Pin: Wire", x, y, 10, BLACK); y += 15;
                DrawText("  Right Click Body: +/- Inputs", x, y, 10, BLACK); y += 15;
                DrawText("  DEL: Delete Selected", x, y, 10, BLACK); y += 25;

                DrawText("Simulation:", x, y, 20, DARKBLUE); y += 25;
                DrawText("  SPACE: Run/Pause", x, y, 10, BLACK); y += 15;
                DrawText("  Right Arrow: Step", x, y, 10, BLACK); y += 15;
                DrawText("  UP/DOWN: Speed (Hz)", x, y, 10, BLACK); y += 15;
                DrawText("  S / L: Save / Load", x, y, 10, BLACK); y += 15;
        }
        else
        {
                 DrawText("Press H for Help", GetScreenWidth() - 150, 10, 20, GRAY);
        }
}

int main()
{
        InitWindow(1280, 720, "Sulla Logic Sim");
        SetTargetFPS(60);

        AppState state;
        initApp(state);

        while (!WindowShouldClose())
        {
                handleHotkeys(state);
                handleMouseInput(state);
                updateSimulation(state);

                BeginDrawing();
                ClearBackground(RAYWHITE);

                drawGrid();
                drawWires(state);
                drawParts(state);
                drawUI(state);

                EndDrawing();
        }

        CloseWindow();
        return 0;
}
