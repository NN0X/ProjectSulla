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

static float distToPolyline(Vector2 p, const std::vector<Vector2>& path)
{
        float best = 1e30f;
        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
                Vector2 a = path[i], b = path[i + 1];
                Vector2 ab = {b.x - a.x, b.y - a.y};
                float len2 = ab.x * ab.x + ab.y * ab.y;
                float t = (len2 > 0.0001f) ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2 : 0.0f;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float dx = p.x - (a.x + t * ab.x), dy = p.y - (a.y + t * ab.y);
                float d = sqrtf(dx * dx + dy * dy);
                if (d < best) best = d;
        }
        return best;
}

static void makeConnection(AppState& state, int fromPart, int fromPin, int toPart, int toPin, bool bus)
{
        if (bus)
        {
                int count = state.outputCounts[fromPart] - fromPin;
                int inRoom = state.inputCounts[toPart] - toPin;
                if (inRoom < count) count = inRoom;
                for (int j = 0; j < count; ++j)
                        state.connections[{toPart, toPin + j}] = {fromPart, fromPin + j};
        }
        else
        {
                state.connections[{toPart, toPin}] = {fromPart, fromPin};
        }
        state.simulation = nullptr;
}

static Vector2 snapWaypoint(AppState& state, PartPin connKey, Vector2 pos)
{
        const float SNAP_PIXELS = 10.0f;
        float snap = SNAP_PIXELS / (state.camera.zoom > 0.01f ? state.camera.zoom : 1.0f);
        Vector2 result = pos;
        Vector2 destP = getPinPos(state, connKey.first, true, connKey.second);
        if (fabsf(pos.y - destP.y) < snap) result.y = destP.y;
        if (fabsf(pos.x - destP.x) < snap) result.x = destP.x;
        std::map<PartPin, PartPin>::iterator c = state.connections.find(connKey);
        if (c != state.connections.end())
        {
                Vector2 srcP = getPinPos(state, c->second.first, false, c->second.second);
                if (fabsf(pos.y - srcP.y) < snap) result.y = srcP.y;
                if (fabsf(pos.x - srcP.x) < snap) result.x = srcP.x;
        }
        return result;
}

static int coarseAddStep(int current)
{
        return (current / 8 + 1) * 8 - current;
}

static int coarseRemoveStep(int current)
{
        if (current <= 0) return 0;
        return current - (current - 1) / 8 * 8;
}

void handleInput(AppState& state)
{
        float sideMenuWidth = state.showSideMenu ? state.sidebarWidth : 0;
        Vector2 mousePos = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mousePos, state.camera);

        bool isDialogActive = state.showSaveDialog || state.showLoadDialog || state.showRenameDialog || state.showCompileDialog || state.showDeleteConfirm || state.showOverwriteConfirm || state.showQuitConfirm || state.showTidyConfirm || state.showError;
        bool mouseOverUI = (mousePos.x < sideMenuWidth) || (mousePos.y < TOOLBAR_HEIGHT) || isDialogActive;

        Rectangle searchBox = {SIDEMENU_PADDING_X, TOOLBAR_HEIGHT + 5.0f, sideMenuWidth - SIDEMENU_PADDING_X * 2.0f, 24.0f};
        if (state.showSideMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isDialogActive)
                state.sidebarSearchFocused = CheckCollisionPointRec(mousePos, searchBox);
        if (state.sidebarSearchFocused)
        {
                int ch = GetCharPressed();
                while (ch > 0)
                {
                        if (ch >= 32 && ch < 127) state.sidebarSearch += (char)ch;
                        ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !state.sidebarSearch.empty()) state.sidebarSearch.pop_back();
                if (IsKeyPressed(KEY_ESCAPE)) { state.sidebarSearch.clear(); state.sidebarSearchFocused = false; }
        }
        bool keyInputBlocked = isDialogActive || state.sidebarSearchFocused;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        bool ctrlHeld = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (!keyInputBlocked && !ctrlHeld && IsKeyPressed(KEY_V)) { state.visualizeSignals = !state.visualizeSignals; state.simulation = nullptr; }
        if (!keyInputBlocked && IsKeyPressed(KEY_T)) state.showTidyConfirm = true;
        if (!keyInputBlocked && !ctrlHeld && IsKeyPressed(KEY_F)) fitView(state);
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_Z)) { if (shiftHeld) doRedo(state); else doUndo(state); }
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_Y)) doRedo(state);
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_C)) copySelection(state);
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_V)) pasteClipboard(state, {GRID_SIZE * 2.0f, GRID_SIZE * 2.0f});
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_D)) duplicateSelection(state);
        if (ctrlHeld && !keyInputBlocked && IsKeyPressed(KEY_A)) selectAllParts(state);
        if (!keyInputBlocked && !state.selectedParts.empty())
        {
                if (shiftHeld)
                {
                        if (IsKeyPressed(KEY_LEFT)) alignSelection(state, 0);
                        if (IsKeyPressed(KEY_RIGHT)) alignSelection(state, 1);
                        if (IsKeyPressed(KEY_UP)) alignSelection(state, 2);
                        if (IsKeyPressed(KEY_DOWN)) alignSelection(state, 3);
                }
                else if (ctrlHeld)
                {
                        if (IsKeyPressed(KEY_H)) distributeSelection(state, true);
                        if (IsKeyPressed(KEY_J)) distributeSelection(state, false);
                }
                else
                {
                        if (IsKeyPressed(KEY_LEFT)) nudgeSelection(state, -GRID_SIZE, 0.0f);
                        if (IsKeyPressed(KEY_RIGHT)) nudgeSelection(state, GRID_SIZE, 0.0f);
                        if (IsKeyPressed(KEY_UP)) nudgeSelection(state, 0.0f, -GRID_SIZE);
                        if (IsKeyPressed(KEY_DOWN)) nudgeSelection(state, 0.0f, GRID_SIZE);
                }
        }

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
                float bestDist = 8.0f / (state.camera.zoom > 0.01f ? state.camera.zoom : 1.0f);
                for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                {
                        std::map<PartPin, std::vector<Vector2>>::iterator pit = state.wirePaths.find(it->first);
                        if (pit == state.wirePaths.end() || pit->second.size() < 2) continue;
                        float d = distToPolyline(worldMouse, pit->second);
                        if (d < bestDist) { bestDist = d; state.hoveredNet = it->second; }
                }
        }

        if (!mouseOverUI && state.wireStartPartID == -1)
        {
                const float WAYPOINT_GRAB = 8.0f / (state.camera.zoom > 0.01f ? state.camera.zoom : 1.0f);
                const double DOUBLE_CLICK_SEC = 0.35;
                if (state.dragWpIdx >= 0)
                {
                        std::map<PartPin, std::vector<Vector2>>::iterator w = state.connectionWaypoints.find(state.dragWpConn);
                        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && w != state.connectionWaypoints.end() && state.dragWpIdx < (int)w->second.size())
                                w->second[state.dragWpIdx] = snapWaypoint(state, state.dragWpConn, worldMouse);
                        else
                                state.dragWpIdx = -1;
                }
                else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
                {
                        bool onHandle = false;
                        for (std::map<PartPin, std::vector<Vector2>>::iterator w = state.connectionWaypoints.begin(); w != state.connectionWaypoints.end() && !onHandle; ++w)
                        {
                                for (size_t k = 0; k < w->second.size(); ++k)
                                {
                                        if (Vector2Distance(worldMouse, w->second[k]) < WAYPOINT_GRAB)
                                        {
                                                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
                                                {
                                                        w->second.erase(w->second.begin() + (long)k);
                                                        if (w->second.empty()) state.connectionWaypoints.erase(w);
                                                }
                                                else
                                                {
                                                        state.dragWpConn = w->first;
                                                        state.dragWpIdx = (int)k;
                                                }
                                                onHandle = true;
                                                break;
                                        }
                                }
                        }
                        if (!onHandle && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                                PartPin nearConn = {-1, -1};
                                int nearSeg = -1;
                                float best = WAYPOINT_GRAB;
                                for (std::map<PartPin, std::vector<Vector2>>::iterator p = state.wirePaths.begin(); p != state.wirePaths.end(); ++p)
                                {
                                        for (size_t k = 0; k + 1 < p->second.size(); ++k)
                                        {
                                                Vector2 a = p->second[k], b = p->second[k + 1];
                                                Vector2 ab = { b.x - a.x, b.y - a.y };
                                                float len2 = ab.x * ab.x + ab.y * ab.y;
                                                float t = (len2 > 0.0001f) ? ((worldMouse.x - a.x) * ab.x + (worldMouse.y - a.y) * ab.y) / len2 : 0.0f;
                                                if (t < 0.0f) t = 0.0f;
                                                if (t > 1.0f) t = 1.0f;
                                                float dx = worldMouse.x - (a.x + t * ab.x), dy = worldMouse.y - (a.y + t * ab.y);
                                                float d = sqrtf(dx * dx + dy * dy);
                                                if (d < best) { best = d; nearConn = p->first; nearSeg = (int)k; }
                                        }
                                }
                                double now = GetTime();
                                bool doubleClick = (now - state.lastWireClickTime < DOUBLE_CLICK_SEC) && (Vector2Distance(mousePos, state.lastWireClickPos) < 6.0f);
                                if (nearConn.first != -1 && doubleClick)
                                {
                                        std::vector<Vector2>& wl = state.connectionWaypoints[nearConn];
                                        int idx = nearSeg - 1;
                                        if (idx < 0) idx = 0;
                                        if (idx > (int)wl.size()) idx = (int)wl.size();
                                        Vector2 snapped = snapWaypoint(state, nearConn, worldMouse);
                                        wl.insert(wl.begin() + (long)idx, snapped);
                                        state.dragWpConn = nearConn;
                                        state.dragWpIdx = idx;
                                }
                                state.lastWireClickTime = now;
                                state.lastWireClickPos = mousePos;
                        }
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
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseAddStep(state.inputCounts[tid]) : 1;
                                        state.inputCounts[tid] += step;
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rRemIn))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseRemoveStep(state.inputCounts[tid]) : 1;
                                        for (int k = 0; k < step && state.inputCounts[tid] > 0; ++k)
                                        {
                                                cleanupInputPinConnections(state, tid, state.inputCounts[tid] - 1);
                                                state.inputCounts[tid]--;
                                        }
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rAddOut))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseAddStep(state.outputCounts[tid]) : 1;
                                        state.outputCounts[tid] += step;
                                        state.simulation = nullptr;
                                }
                                else if (CheckCollisionPointRec(mousePos, rRemOut))
                                {
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseRemoveStep(state.outputCounts[tid]) : 1;
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
                                        int current = (type == PART_TYPE_SOURCE || type == PART_TYPE_CLOCK) ? state.outputCounts[tid] : state.inputCounts[tid];
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseAddStep(current) : 1;
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
                                        int current = (type == PART_TYPE_SOURCE || type == PART_TYPE_CLOCK) ? state.outputCounts[tid] : state.inputCounts[tid];
                                        int step = IsKeyDown(KEY_LEFT_SHIFT) ? coarseRemoveStep(current) : 1;
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
                                                        makeConnection(state, state.wireStartPartID, state.wireStartPin, id, i, ctrlHeld);
                                                        state.wireStartPartID = -1;
                                                }
                                                else
                                                {
                                                        std::map<PartPin, PartPin>::iterator existing = state.connections.find({id, i});
                                                        if (existing != state.connections.end())
                                                        {
                                                                state.wireStartPartID = existing->second.first;
                                                                state.wireStartPin = existing->second.second;
                                                                state.wireDragStartPos = mousePos;
                                                                state.connections.erase(existing);
                                                                state.simulation = nullptr;
                                                        }
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
                        float best = 8.0f / (state.camera.zoom > 0.01f ? state.camera.zoom : 1.0f);
                        PartPin sel = {-1, -1};
                        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
                        {
                                std::map<PartPin, std::vector<Vector2>>::iterator pit = state.wirePaths.find(it->first);
                                if (pit == state.wirePaths.end() || pit->second.size() < 2) continue;
                                float d = distToPolyline(worldMouse, pit->second);
                                if (d < best) { best = d; sel = it->first; }
                        }
                        if (sel.first != -1)
                        {
                                state.selectedConnection = sel;
                                hitSomething = true;
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
                                                makeConnection(state, state.wireStartPartID, state.wireStartPin, id, i, ctrlHeld);
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
