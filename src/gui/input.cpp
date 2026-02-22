#include "gui.h"
#include "common.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>
#include <set>

#include <raylib/raylib.h>
#include <raylib/raymath.h>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"

void dropPart(AppState& state, int type, Vector2 pos)
{
        int id = state.nextID++;
        if (type == PART_TYPE_SOURCE) setSourcePart(state.parts, id);
        else if (type == PART_TYPE_OUTPUT) setOutputPart(state.parts, id);
        else setPart(state.parts, id, getPartFromType((PartType)type));

        state.partTypes[id] = (PartType)type;
        state.positions[id] = {pos.x, pos.y};
        state.labels[id] = PART_TYPE_NAMES[type];
        state.simulation = nullptr;

        if (type == PART_TYPE_NOT)
        {
                state.inputCounts[id] = 1;
                state.outputCounts[id] = 1;
        }
        else if (type == PART_TYPE_SOURCE) 
        { 
                state.inputCounts[id] = 0; 
                state.outputCounts[id] = 1; 
                state.sourceValues[id] = {STATE_LOW};
        }
        else if (type == PART_TYPE_OUTPUT)
        {
                state.inputCounts[id] = 1;
                state.outputCounts[id] = 0;
        }
        else
        {
                state.inputCounts[id] = 2;
                state.outputCounts[id] = 1;
        }
}

void deleteParts(AppState& state)
{
        if (state.selectedParts.empty()) return;

        for (std::set<int>::iterator it = state.selectedParts.begin(); it != state.selectedParts.end(); ++it)
        {
                int id = *it;
                state.parts.erase(id);
                state.partTypes.erase(id);
                state.positions.erase(id);
                state.inputCounts.erase(id);
                state.outputCounts.erase(id);
                state.sourceValues.erase(id);
                state.labels.erase(id);

                std::vector<PartPin> toRemove;
                for(std::map<PartPin, PartPin>::iterator connIt = state.connections.begin(); connIt != state.connections.end(); ++connIt)
                {
                        if (connIt->first.first == id || connIt->second.first == id)
                        {
                                toRemove.push_back(connIt->first);
                        }
                }
                for(size_t i = 0; i < toRemove.size(); ++i)
                {
                        state.connections.erase(toRemove[i]);
                }
        }
        state.selectedParts.clear();
        state.simulation = nullptr;
}

void handleInput(AppState& state)
{
        float sideMenuWidth = state.showSideMenu ? DEFAULT_SIDEMENU_WIDTH : 0;
        Vector2 mousePos = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mousePos, state.camera);

        bool isDialogActive = state.showSaveDialog || state.showLoadDialog || state.showDeleteConfirm || state.showOverwriteConfirm || state.showQuitConfirm;
        bool mouseOverUI = (mousePos.x < sideMenuWidth) || (mousePos.y < TOOLBAR_HEIGHT) || isDialogActive;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (state.showQuitConfirm)
        {
                bool confirm = false;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                        float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;
                        Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        if (CheckCollisionPointRec(mousePos, cancelBtn)) state.showQuitConfirm = false;
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }

                if (IsKeyPressed(KEY_ENTER) || confirm) state.shouldQuit = true;
                if (IsKeyPressed(KEY_ESCAPE)) state.showQuitConfirm = false;
                
                return;
        }

        if (state.showSaveDialog || state.showLoadDialog)
        {
                int key = GetCharPressed();
                while (key > 0)
                {
                        if ((key >= 32) && (key <= 125))
                        {
                                int len = std::string(state.fileNameBuffer).length();
                                if (len < SAVE_FILENAME_MAX_LEN)
                                {
                                        state.fileNameBuffer[len] = (char)key;
                                        state.fileNameBuffer[len + 1] = '\0';
                                }
                        }
                        key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE))
                {
                        int len = std::string(state.fileNameBuffer).length();
                        if (len > 0) state.fileNameBuffer[len - 1] = '\0';
                }

                bool confirm = false;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                        float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;
                        Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        if (CheckCollisionPointRec(mousePos, cancelBtn))
                        {
                                state.showSaveDialog = false;
                                state.showLoadDialog = false;
                        }
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }

                if (IsKeyPressed(KEY_ENTER) || confirm)
                {
                        std::string fname = "layouts/" + std::string(state.fileNameBuffer) + ".json";
                        if (state.showSaveDialog)
                        {
                                if (std::filesystem::exists(fname))
                                {
                                        state.pendingSaveFilename = fname;
                                        state.showOverwriteConfirm = true;
                                        state.showSaveDialog = false;
                                }
                                else
                                {
                                        if (!std::filesystem::exists("layouts")) std::filesystem::create_directory("layouts");
                                        saveLayout(state.partTypes, state.connections, state.labels, 
                                                   state.positions, state.inputCounts, state.outputCounts, fname);
                                        state.showSaveDialog = false;
                                        refreshLayouts(state);
                                }
                        }
                        else if (state.showLoadDialog)
                        {
                                loadLayout(state, fname);
                                recompileSimulation(state);
                                state.showLoadDialog = false;
                                refreshLayouts(state);
                        }
                }
                if (IsKeyPressed(KEY_ESCAPE))
                {
                        state.showSaveDialog = false;
                        state.showLoadDialog = false;
                }
                return;
        }

        if (state.showOverwriteConfirm)
        {
                bool confirm = false;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                        float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;
                        Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        if (CheckCollisionPointRec(mousePos, cancelBtn))
                        {
                                state.showOverwriteConfirm = false;
                                state.showSaveDialog = true;
                        }
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }
                if (IsKeyPressed(KEY_ENTER) || confirm)
                {
                        saveLayout(state.partTypes, state.connections, state.labels, 
                                   state.positions, state.inputCounts, state.outputCounts, state.pendingSaveFilename);
                        state.showOverwriteConfirm = false;
                        refreshLayouts(state);
                }
                if (IsKeyPressed(KEY_ESCAPE))
                {
                        state.showOverwriteConfirm = false;
                        state.showSaveDialog = true;
                }
                return;
        }

        if (state.showDeleteConfirm)
        {
                bool confirm = false;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                        float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;
                        Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                        if (CheckCollisionPointRec(mousePos, cancelBtn))
                        {
                                state.showDeleteConfirm = false;
                        }
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }
                if (IsKeyPressed(KEY_ENTER) || confirm)
                {
                        std::filesystem::remove("layouts/" + state.layoutToDelete);
                        refreshLayouts(state);
                        state.showDeleteConfirm = false;
                }
                if (IsKeyPressed(KEY_ESCAPE)) state.showDeleteConfirm = false;
                return;
        }

        if (!isDialogActive)
        {
                if (mousePos.y < TOOLBAR_HEIGHT && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        float x = TOOLBAR_PADDING;
                        for (int i = 0; i < 9; ++i)
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
                                        if (i == 5) state.isSimulating = !state.isSimulating;
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
                                        if (i == 7) state.targetHZ *= 2.0f;
                                        if (i == 8) state.targetHZ *= 0.5f;
                                }
                                x += TOOLBAR_BTN_WIDTH + TOOLBAR_BTN_SPACING;
                        }
                }

                float wheel = GetMouseWheelMove();
                if (wheel != 0)
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
                if (IsKeyPressed(KEY_ESCAPE)) state.showQuitConfirm = true;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
                if (state.draggingNewPartType != -1 && !mouseOverUI)
                {
                        float gx = round(worldMouse.x / GRID_SIZE) * GRID_SIZE;
                        float gy = round(worldMouse.y / GRID_SIZE) * GRID_SIZE;
                        dropPart(state, state.draggingNewPartType, {gx, gy});
                }
                else if (state.draggingLayoutFile != "" && !mouseOverUI)
                {
                        float gx = round(worldMouse.x / GRID_SIZE) * GRID_SIZE;
                        float gy = round(worldMouse.y / GRID_SIZE) * GRID_SIZE;
                        int nIn;
                        int nOut;
                        Part p = loadLayoutAsPart("layouts/" + state.draggingLayoutFile, nIn, nOut);
                        if (p)
                        {
                                int id = state.nextID++;
                                setPart(state.parts, id, p);
                                state.partTypes[id] = PART_TYPE_CUSTOM;
                                state.positions[id] = {gx, gy};
                                state.inputCounts[id] = nIn;
                                state.outputCounts[id] = nOut;
                                std::filesystem::path path(state.draggingLayoutFile);
                                state.labels[id] = path.stem().string();
                                state.simulation = nullptr;
                        }
                }
                state.draggingNewPartType = -1;
                state.draggingLayoutFile = "";
        }

        if (state.contextMenu.active)
        {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        Vector2 p = state.contextMenu.position;
                        Rectangle rAdd = {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT};
                        Rectangle rRem = {p.x, p.y + CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT};
                        Rectangle rDel = {p.x, p.y + CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT};

                        if (CheckCollisionPointRec(mousePos, rAdd))
                        {
                                if (state.partTypes[state.contextMenu.targetPartID] == PART_TYPE_SOURCE)
                                {
                                        state.outputCounts[state.contextMenu.targetPartID]++;
                                        state.sourceValues[state.contextMenu.targetPartID].push_back(STATE_LOW);
                                }
                                else
                                {
                                        state.inputCounts[state.contextMenu.targetPartID]++;
                                }
                                state.simulation = nullptr;
                                state.contextMenu.active = false;
                        }
                        else if (CheckCollisionPointRec(mousePos, rRem))
                        {
                                if (state.partTypes[state.contextMenu.targetPartID] == PART_TYPE_SOURCE)
                                {
                                        if (state.outputCounts[state.contextMenu.targetPartID] > 1)
                                        {
                                                state.outputCounts[state.contextMenu.targetPartID]--;
                                                state.sourceValues[state.contextMenu.targetPartID].pop_back();
                                        }
                                }
                                else
                                {
                                        if (state.inputCounts[state.contextMenu.targetPartID] > 0)
                                        {
                                                state.inputCounts[state.contextMenu.targetPartID]--;
                                        }
                                }
                                state.simulation = nullptr;
                                state.contextMenu.active = false;
                        }
                        else if (CheckCollisionPointRec(mousePos, rDel))
                        {
                                state.selectedParts.clear();
                                state.selectedParts.insert(state.contextMenu.targetPartID);
                                deleteParts(state);
                                state.contextMenu.active = false;
                        }
                        else
                        {
                                state.contextMenu.active = false;
                        }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) state.contextMenu.active = false;
                return;
        }

        if (IsKeyPressed(KEY_SPACE) && !isDialogActive) state.isSimulating = !state.isSimulating;

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

        if (mouseOverUI && state.draggingNewPartType == -1 && state.draggingLayoutFile == "") return;

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
                                float pinYStep = (outCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (outCount - 1) : 0;
                                for (int i = 0; i < outCount; ++i)
                                {
                                        float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                        if (outCount <= 1) yOff = 0;

                                        Rectangle toggleRect = {pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE};
                                        if (CheckCollisionPointRec(worldMouse, toggleRect))
                                        {
                                                state.sourceValues[id][i] = (state.sourceValues[id][i] == STATE_LOW) ? STATE_HIGH : STATE_LOW;
                                                hitSomething = true;
                                                break;
                                        }
                                        Rectangle pinRect = {pos.x + size.x/2, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                        if (CheckCollisionPointRec(worldMouse, pinRect))
                                        {
                                                state.wireStartPartID = id;
                                                state.wireStartPin = i;
                                                hitSomething = true;
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                float pinYStepIn = (inCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (inCount - 1) : 0;
                                for (int i = 0; i < inCount; ++i)
                                {
                                        float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStepIn;
                                        if (inCount <= 1) yOff = 0;
                                        Rectangle pinRect = {pos.x - size.x/2 - PIN_SIZE, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
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
                                float pinYStepOut = (outCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (outCount - 1) : 0;
                                for (int i = 0; i < outCount; ++i)
                                {
                                        float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStepOut;
                                        if (outCount <= 1) yOff = 0;
                                        Rectangle pinRect = {pos.x + size.x/2, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                        if (CheckCollisionPointRec(worldMouse, pinRect))
                                        {
                                                state.wireStartPartID = id;
                                                state.wireStartPin = i;
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
                state.dragPartID = -1;
                state.isDragging = false;
                state.isBoxSelecting = false;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
                state.wireStartPartID = -1;
                for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
                {
                        int id = it->first;
                        Vector2 size = getPartSize(state, id);
                        Rectangle body = {it->second.first - size.x/2, it->second.second - size.y/2, size.x, size.y};
                        if (CheckCollisionPointRec(worldMouse, body))
                        {
                                state.contextMenu.active = true;
                                state.contextMenu.targetPartID = id;
                                state.contextMenu.position = mousePos;
                                break;
                        }
                }
        }
}
