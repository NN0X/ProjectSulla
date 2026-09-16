#include "gui.h"
#include "common.h"
#include "../gates.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <cstdio>
#include <fstream>

#include <raylib/raylib.h>
#include <raylib/raymath.h>
#include <glaze/glaze.hpp>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"
#include "../compiler/compiler.h"

void handleInput(AppState& state)
{
        float sideMenuWidth = state.showSideMenu ? state.sidebarWidth : 0;
        Vector2 mousePos = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mousePos, state.camera);

        bool isDialogActive = state.showSaveDialog || state.showLoadDialog || state.showRenameDialog || state.showCompileDialog || state.showDeleteConfirm || state.showOverwriteConfirm || state.showQuitConfirm;
        bool mouseOverUI = (mousePos.x < sideMenuWidth) || (mousePos.y < TOOLBAR_HEIGHT) || isDialogActive;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (!isDialogActive && IsKeyPressed(KEY_V)) { state.visualizeSignals = !state.visualizeSignals; state.simulation = nullptr; }
        if (!isDialogActive && IsKeyPressed(KEY_T)) tidyLayout(state);
        bool ctrlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrlHeld && !isDialogActive && IsKeyPressed(KEY_Z)) { if (shiftHeld) doRedo(state); else doUndo(state); }
        if (ctrlHeld && !isDialogActive && IsKeyPressed(KEY_Y)) doRedo(state);

        if (state.showSideMenu && mousePos.x < sideMenuWidth && mousePos.y >= TOOLBAR_HEIGHT && !isDialogActive)
        {
                float sw = GetMouseWheelMove();
                if (sw != 0.0f)
                {
                        state.sidebarScroll -= sw * 34.0f;
                        if (state.sidebarScroll < 0.0f) state.sidebarScroll = 0.0f;
                        if (state.sidebarScroll > state.sidebarMaxScroll) state.sidebarScroll = state.sidebarMaxScroll;
                }
        }

        state.hoveredNet = {-1, -1};
        if (!mouseOverUI && state.wireStartPartID == -1)
        {
                float bestDist = 10.0f / (state.camera.zoom > 0.01f ? state.camera.zoom : 1.0f);
                for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                {
                        Vector2 a = getPinPos(state, it->second.first, false, it->second.second);
                        Vector2 b = getPinPos(state, it->first.first, true, it->first.second);
                        Vector2 ab = {b.x - a.x, b.y - a.y};
                        float len2 = ab.x * ab.x + ab.y * ab.y;
                        float t = (len2 > 0.0001f) ? ((worldMouse.x - a.x) * ab.x + (worldMouse.y - a.y) * ab.y) / len2 : 0.0f;
                        if (t < 0.0f) t = 0.0f;
                        if (t > 1.0f) t = 1.0f;
                        float dx = worldMouse.x - (a.x + t * ab.x);
                        float dy = worldMouse.y - (a.y + t * ab.y);
                        float d = sqrtf(dx * dx + dy * dy);
                        if (d < bestDist) { bestDist = d; state.hoveredNet = it->second; }
                }
        }

        if (handleDialogs(state)) return;

        if (!isDialogActive)
        {
                if (mousePos.y < TOOLBAR_HEIGHT && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float x = TOOLBAR_PADDING;
                        for (int i = 0; i < 11; ++i)
                        {
                                Rectangle btn = {x, TOOLBAR_PADDING, TOOLBAR_BTN_WIDTH, TOOLBAR_BTN_HEIGHT};
                                if (CheckCollisionPointRec(mousePos, btn))
                                {
                                        if (i == 0) state.showSaveDialog = true;
                                        if (i == 1) state.showLoadDialog = true;
                                        if (i == 2)
                                        {
                                                state.selectedParts.clear();
                                                for(auto const& [id, pos] : state.positions)
                                                {
                                                        state.selectedParts.insert(id);
                                                }
                                                deleteParts(state);
                                        }
                                        if (i == 3) state.showHelp = !state.showHelp;
                                        if (i == 4) state.darkMode = !state.darkMode;
                                        if (i == 5) { state.isSimulating = !state.isSimulating; state.simulation = nullptr; }
                                        if (i == 6)
                                        {
                                                state.isSimulating = false;
                                                if (!state.simulation) recompileSimulation(state);
                                                if(state.simulation)
                                                {
                                                        state.lastOutputStates = state.simulation(state.runtimeInput);
                                                        state.stepCount++;
                                                }
                                        }
                                        if (i == 7) state.stepCount = 0;
                                        if (i == 8) state.targetHZ *= 2.0f;
                                        if (i == 9) state.targetHZ *= 0.5f;
                                        if (i == 10) state.showCompileDialog = true;
                                }
                                x += TOOLBAR_BTN_WIDTH + TOOLBAR_BTN_SPACING;
                        }
                }

                float wheel = GetMouseWheelMove();
                if (wheel != 0 && !mouseOverUI)
                {
                        Vector2 mouseWorldBefore = GetScreenToWorld2D(mousePos, state.camera);
                        state.camera.zoom += (wheel * ZOOM_SPEED);
                        if (state.camera.zoom < ZOOM_MIN) state.camera.zoom = ZOOM_MIN;
                        if (state.camera.zoom > ZOOM_MAX) state.camera.zoom = ZOOM_MAX;
                        Vector2 mouseWorldAfter = GetScreenToWorld2D(mousePos, state.camera);
                        state.camera.target = Vector2Add(state.camera.target, Vector2Subtract(mouseWorldBefore, mouseWorldAfter));
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
                {
                        Vector2 delta = GetMouseDelta();
                        delta = Vector2Scale(delta, -1.0f / state.camera.zoom);
                        state.camera.target = Vector2Add(state.camera.target, delta);
                }

                if (IsKeyPressed(KEY_TAB)) state.showSideMenu = !state.showSideMenu;
                if (IsKeyPressed(KEY_H)) state.showHelp = !state.showHelp;
                if (IsKeyPressed(KEY_D)) state.darkMode = !state.darkMode;
                if (IsKeyPressed(KEY_B))
                {
                        if (state.showBenchmark)
                        {
                                state.showBenchmark = false;
                        }
                        else
                        {
                                state.showBenchmark = true;
                                state.benchmark.valid = false;
                                state.benchmark.running = false;
                                state.benchmarkPending = true;
                        }
                }
                if (IsKeyPressed(KEY_ESCAPE))
                {
                        if (state.showBenchmark) state.showBenchmark = false;
                        else state.showQuitConfirm = true;
                }
        }

        if (state.contextMenu.active)
        {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        Vector2 p = state.contextMenu.position;
                        int tid = state.contextMenu.targetPartID;
                        PartType type = state.partTypes[tid];
                        bool isOutput = (type == PART_TYPE_OUTPUT);

                        if (isOutput)
                        {
                                Rectangle rLabel = {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rAddIn = {p.x, p.y + CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rRemIn = {p.x, p.y + CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rAddOut = {p.x, p.y + CM_ROW_HEIGHT*3, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rRemOut = {p.x, p.y + CM_ROW_HEIGHT*4, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rDel = {p.x, p.y + CM_ROW_HEIGHT*5, CM_WIDTH, CM_ROW_HEIGHT};

                                if (CheckCollisionPointRec(mousePos, rLabel))
                                {
                                        state.showRenameDialog = true;
                                        state.renamePartID = tid;
                                        std::string current = state.labels[tid];
                                        snprintf(state.fileNameBuffer, sizeof(state.fileNameBuffer), "%s", current.c_str());
                                        state.contextMenu.active = false;
                                }
                                else if (CheckCollisionPointRec(mousePos, rAddIn))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        state.inputCounts[tid] += step;
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rRemIn))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        for (int k = 0; k < step && state.inputCounts[tid] > 0; ++k)
                                        {
                                                cleanupInputPinConnections(state, tid, state.inputCounts[tid] - 1);
                                                state.inputCounts[tid]--;
                                        }
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rAddOut))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        state.outputCounts[tid] += step;
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rRemOut))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        for (int k = 0; k < step && state.outputCounts[tid] > 0; ++k)
                                        {
                                                cleanupOutputPinConnections(state, tid, state.outputCounts[tid] - 1);
                                                state.outputCounts[tid]--;
                                        }
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rDel))
                                {
                                        state.selectedParts.clear();
                                        state.selectedParts.insert(tid);
                                        deleteParts(state);
                                        state.contextMenu.active = false;
                                }
                                else
                                {
                                        state.contextMenu.active = false;
                                }
                        }
                        else
                        {
                                Rectangle rLabel = {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rAdd = {p.x, p.y + CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rRem = {p.x, p.y + CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT};
                                Rectangle rDel = {p.x, p.y + CM_ROW_HEIGHT*3, CM_WIDTH, CM_ROW_HEIGHT};
                                bool canModPins = (type != PART_TYPE_CUSTOM);

                                if (CheckCollisionPointRec(mousePos, rLabel))
                                {
                                        state.showRenameDialog = true;
                                        state.renamePartID = tid;
                                        std::string current = state.labels[tid];
                                        snprintf(state.fileNameBuffer, sizeof(state.fileNameBuffer), "%s", current.c_str());
                                        state.contextMenu.active = false;
                                }
                                else if (canModPins && CheckCollisionPointRec(mousePos, rAdd))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        for (int k = 0; k < step; ++k)
                                        {
                                                if (type == PART_TYPE_SOURCE)
                                                {
                                                        state.outputCounts[tid]++;
                                                        state.sourceValues[tid].push_back(STATE_LOW);
                                                }
                                                else if (type == PART_TYPE_CLOCK)
                                                {
                                                        state.outputCounts[tid]++;
                                                }
                                                else
                                                {
                                                        state.inputCounts[tid]++;
                                                }
                                        }
                                        state.simulation = nullptr;
                                }
                                else if (canModPins && CheckCollisionPointRec(mousePos, rRem))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? 8 : 1;
                                        for (int k = 0; k < step; ++k)
                                        {
                                                if (type == PART_TYPE_SOURCE)
                                                {
                                                        if (state.outputCounts[tid] > 1)
                                                        {
                                                                cleanupOutputPinConnections(state, tid, state.outputCounts[tid] - 1);
                                                                state.outputCounts[tid]--;
                                                                state.sourceValues[tid].pop_back();
                                                        }
                                                }
                                                else if (type == PART_TYPE_CLOCK)
                                                {
                                                        if (state.outputCounts[tid] > 1)
                                                        {
                                                                cleanupOutputPinConnections(state, tid, state.outputCounts[tid] - 1);
                                                                state.outputCounts[tid]--;
                                                        }
                                                }
                                                else
                                                {
                                                        if (state.inputCounts[tid] > 0)
                                                        {
                                                                cleanupInputPinConnections(state, tid, state.inputCounts[tid] - 1);
                                                                state.inputCounts[tid]--;
                                                        }
                                                }
                                        }
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rDel))
                                {
                                        state.selectedParts.clear();
                                        state.selectedParts.insert(tid);
                                        deleteParts(state);
                                        state.contextMenu.active = false;
                                }
                                else
                                {
                                        state.contextMenu.active = false;
                                }
                        }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) state.contextMenu.active = false;
                return;
        }

        if (IsKeyPressed(KEY_SPACE) && !isDialogActive) { state.isSimulating = !state.isSimulating; state.simulation = nullptr; }

        bool up = IsKeyDown(KEY_UP);
        bool down = IsKeyDown(KEY_DOWN);
        if ((up || down) && !isDialogActive)
        {
                state.hzKeyTimer += GetFrameTime();
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || state.hzKeyTimer > INPUT_REPEAT_DELAY)
                {
                        if (state.hzKeyTimer > INPUT_REPEAT_DELAY) state.hzKeyTimer -= INPUT_REPEAT_RATE;
                        if (up) state.targetHZ *= 2.0f;
                        else state.targetHZ *= 0.5f;
                }
        }
        else state.hzKeyTimer = 0.0f;

        if (IsKeyPressed(KEY_S) && !isDialogActive) state.showSaveDialog = true;
        if (IsKeyPressed(KEY_L) && !isDialogActive) state.showLoadDialog = true;

        if (IsKeyPressed(KEY_DELETE) && !isDialogActive)
        {
                 if (state.selectedConnection.first != -1)
                 {
                         state.connections.erase(state.selectedConnection);
                         state.selectedConnection = {-1, -1};
                         state.simulation = nullptr;
                 }
                 else
                 {
                         deleteParts(state);
                 }
        }

        if (mouseOverUI && state.draggingNewPartType == -1 && state.draggingLayoutFile == "" && state.draggingCompiledFile == "") return;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
                bool hitSomething = false;
                state.selectedConnection = {-1, -1};

                for (std::map<int, std::pair<float, float>>::reverse_iterator it = state.positions.rbegin(); it != state.positions.rend(); ++it)
                {
                        int id = it->first;
                        Vector2 pos = {it->second.first, it->second.second};
                        Vector2 size = getPartSize(state, id);
                        Rectangle body = {pos.x - size.x/2, pos.y - size.y/2, size.x, size.y};

                        int inCount = state.inputCounts[id];
                        int outCount = state.outputCounts[id];

                        if (state.partTypes[id] == PART_TYPE_SOURCE)
                        {
                                for (int i = 0; i < outCount; ++i)
                                {
                                        float yOff = getPinYOffset(state, id, false, i);
                                        Rectangle toggleRect = {pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE};
                                        if (CheckCollisionPointRec(worldMouse, toggleRect))
                                        {
                                                state.sourceValues[id][i] = (state.sourceValues[id][i] == STATE_LOW) ? STATE_HIGH : STATE_LOW;
                                                hitSomething = true;
                                                break;
                                        }
                                        Rectangle pinRect = getPinRect(state, id, false, i);
                                        if (CheckCollisionPointRec(worldMouse, pinRect))
                                        {
                                                state.wireStartPartID = id;
                                                state.wireStartPin = i;
                                                state.wireDragStartPos = mousePos;
                                                hitSomething = true;
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                for (int i = 0; i < inCount; ++i)
                                {
                                        Rectangle pinRect = getPinRect(state, id, true, i);
                                        if (CheckCollisionPointRec(worldMouse, pinRect))
                                        {
                                                if (state.wireStartPartID != -1)
                                                {
                                                        state.connections[{id, i}] = {state.wireStartPartID, state.wireStartPin};
                                                        state.simulation = nullptr;
                                                        state.wireStartPartID = -1;
                                                }
                                                hitSomething = true;
                                                break;
                                        }
                                }
                                for (int i = 0; i < outCount; ++i)
                                {
                                        Rectangle pinRect = getPinRect(state, id, false, i);
                                        if (CheckCollisionPointRec(worldMouse, pinRect))
                                        {
                                                state.wireStartPartID = id;
                                                state.wireStartPin = i;
                                                state.wireDragStartPos = mousePos;
                                                hitSomething = true;
                                                break;
                                        }
                                }
                        }
                        if (hitSomething) break;

                        if (CheckCollisionPointRec(worldMouse, body))
                        {
                                state.dragPartID = id;
                                state.dragStartMousePos = worldMouse;
                                state.isDragging = false;

                                if (IsKeyDown(KEY_LEFT_SHIFT))
                                {
                                        if (state.selectedParts.count(id)) state.selectedParts.erase(id);
                                        else state.selectedParts.insert(id);
                                }
                                else if (state.selectedParts.find(id) == state.selectedParts.end())
                                {
                                        state.selectedParts.clear();
                                        state.selectedParts.insert(id);
                                }

                                state.dragStartPositions.clear();
                                for (std::set<int>::iterator sit = state.selectedParts.begin(); sit != state.selectedParts.end(); ++sit)
                                {
                                        state.dragStartPositions[*sit] = {state.positions[*sit].first, state.positions[*sit].second};
                                }

                                hitSomething = true;
                                break;
                        }
                }

                if (!hitSomething)
                {
                        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                        {
                                int fromID = it->second.first;
                                int toID = it->first.first;
                                Vector2 startPos = {state.positions[fromID].first, state.positions[fromID].second};
                                Vector2 fromSize = getPartSize(state, fromID);

                                int fromOutCount = state.outputCounts[fromID];
                                float pinYStepFrom = (fromOutCount > 1) ? (fromSize.y - PIN_Y_OFFSET_BASE*2) / (fromOutCount - 1) : 0;
                                float yOffStart = -fromSize.y/2 + PIN_Y_OFFSET_BASE + it->second.second * pinYStepFrom;
                                if (fromOutCount <= 1) yOffStart = 0;
                                Vector2 start = {startPos.x + fromSize.x/2, startPos.y + yOffStart};

                                Vector2 endPos = {state.positions[toID].first, state.positions[toID].second};
                                Vector2 toSize = getPartSize(state, toID);
                                int inCount = state.inputCounts[toID];
                                float pinYStep = (inCount > 1) ? (toSize.y - PIN_Y_OFFSET_BASE*2) / (inCount - 1) : 0;
                                float yOff = -toSize.y/2 + PIN_Y_OFFSET_BASE + it->first.second * pinYStep;
                                if (inCount <= 1) yOff = 0;
                                Vector2 end = {endPos.x - toSize.x/2, endPos.y + yOff};

                                float midX = (start.x + end.x) / 2.0f;
                                Vector2 p1 = {midX, start.y};
                                Vector2 p2 = {midX, end.y};

                                bool hit = false;
                                if (CheckCollisionPointRec(worldMouse, {fminf(start.x, p1.x)-WIRE_HITBOX_PADDING, start.y-WIRE_HITBOX_PADDING, fabsf(start.x-p1.x)+WIRE_HITBOX_SIZE, WIRE_HITBOX_SIZE})) hit = true;
                                else if (CheckCollisionPointRec(worldMouse, {p1.x-WIRE_HITBOX_PADDING, fminf(p1.y, p2.y)-WIRE_HITBOX_PADDING, WIRE_HITBOX_SIZE, fabsf(p1.y-p2.y)+WIRE_HITBOX_SIZE})) hit = true;
                                else if (CheckCollisionPointRec(worldMouse, {fminf(p2.x, end.x)-WIRE_HITBOX_PADDING, end.y-WIRE_HITBOX_PADDING, fabsf(p2.x-end.x)+WIRE_HITBOX_SIZE, WIRE_HITBOX_SIZE})) hit = true;

                                if (hit)
                                {
                                        state.selectedConnection = it->first;
                                        hitSomething = true;
                                        break;
                                }
                        }
                }

                if (!hitSomething)
                {
                        state.wireStartPartID = -1;
                        if (!IsKeyDown(KEY_LEFT_SHIFT)) state.selectedParts.clear();
                        state.isBoxSelecting = true;
                        state.boxSelectStart = worldMouse;
                }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
                if (state.dragPartID != -1)
                {
                        if (Vector2Distance(state.dragStartMousePos, worldMouse) > DRAG_THRESHOLD) state.isDragging = true;
                        if (state.isDragging)
                        {
                                Vector2 diff = Vector2Subtract(worldMouse, state.dragStartMousePos);
                                for (std::map<int, Vector2>::iterator it = state.dragStartPositions.begin(); it != state.dragStartPositions.end(); ++it)
                                {
                                        int id = it->first;
                                        Vector2 startPos = it->second;
                                        float gx = round((startPos.x + diff.x) / GRID_SIZE) * GRID_SIZE;
                                        float gy = round((startPos.y + diff.y) / GRID_SIZE) * GRID_SIZE;
                                        state.positions[id] = {gx, gy};
                                }
                        }
                }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
                if (state.wireStartPartID != -1 && Vector2Distance(mousePos, state.wireDragStartPos) > 6.0f)
                {
                        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                        {
                                int id = it->first;
                                if (id == state.wireStartPartID) continue;
                                bool done = false;
                                int inC = state.inputCounts[id];
                                for (int i = 0; i < inC; ++i)
                                {
                                        if (CheckCollisionPointRec(worldMouse, getPinRect(state, id, true, i)))
                                        {
                                                state.connections[{id, i}] = {state.wireStartPartID, state.wireStartPin};
                                                state.simulation = nullptr;
                                                done = true;
                                                break;
                                        }
                                }
                                if (done) break;
                        }
                        state.wireStartPartID = -1;
                }
                if (state.isBoxSelecting)
                {
                        float minX = std::min(state.boxSelectStart.x, worldMouse.x);
                        float maxX = std::max(state.boxSelectStart.x, worldMouse.x);
                        float minY = std::min(state.boxSelectStart.y, worldMouse.y);
                        float maxY = std::max(state.boxSelectStart.y, worldMouse.y);
                        Rectangle box = {minX, minY, maxX - minX, maxY - minY};

                        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                        {
                                Vector2 pos = {it->second.first, it->second.second};
                                if (CheckCollisionPointRec(pos, box)) state.selectedParts.insert(it->first);
                        }
                }
                if (state.draggingNewPartType != -1 && !mouseOverUI)
                {
                        float gx = round(worldMouse.x / GRID_SIZE) * GRID_SIZE;
                        float gy = round(worldMouse.y / GRID_SIZE) * GRID_SIZE;
                        dropPart(state, state.draggingNewPartType, {gx, gy});
                }
                else if (state.draggingLayoutFile != "" && !mouseOverUI)
                {
                        if (!IsKeyDown(KEY_LEFT_SHIFT)) state.selectedParts.clear();
                        std::set<int> importedIDs = importLayout(state, "layouts/" + state.draggingLayoutFile, worldMouse.x, worldMouse.y);
                        for (int id : importedIDs)
                        {
                                state.selectedParts.insert(id);
                        }
                }
                else if (state.draggingCompiledFile != "" && !mouseOverUI)
                {
                        float gx = round(worldMouse.x / GRID_SIZE) * GRID_SIZE;
                        float gy = round(worldMouse.y / GRID_SIZE) * GRID_SIZE;
                        int nIn = state.compiledInputs[state.draggingCompiledFile];
                        int nOut = state.compiledOutputs[state.draggingCompiledFile];
                        Part p = loadCompiledPart(state.draggingCompiledFile, nOut);
                        if (p)
                        {
                                int id = state.parts.empty() ? 100 : state.parts.rbegin()->first + 1;
                                setPart(state.parts, id, p);
                                state.partTypes[id] = PART_TYPE_CUSTOM;
                                state.positions[id] = {gx, gy};
                                state.inputCounts[id] = nIn;
                                state.outputCounts[id] = nOut;
                                state.labels[id] = state.draggingCompiledFile;
                                state.simulation = nullptr;
                                if (state.compiledInputLabels.count(state.draggingCompiledFile))
                                        state.inputPinLabels[id] = state.compiledInputLabels[state.draggingCompiledFile];
                                if (state.compiledOutputLabels.count(state.draggingCompiledFile))
                                        state.outputPinLabels[id] = state.compiledOutputLabels[state.draggingCompiledFile];
                        }
                }
                state.dragPartID = -1;
                state.isDragging = false;
                state.isBoxSelecting = false;
                state.draggingNewPartType = -1;
                state.draggingLayoutFile = "";
                state.draggingCompiledFile = "";
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
                state.wireStartPartID = -1;
                for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                {
                        int id = it->first;
                        Rectangle body = getBodyRect(state, id);
                        if (CheckCollisionPointRec(worldMouse, body))
                        {
                                state.contextMenu.active = true;
                                state.contextMenu.targetPartID = id;
                                state.contextMenu.position = mousePos;
                                break;
                        }
                }
        }

        updateHistory(state);
}
