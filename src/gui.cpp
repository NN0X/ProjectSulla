#include "gui.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>

#include <raylib/raylib.h>
#include <raylib/raymath.h>

#include "config.h"
#include "primitives.h"
#include "utils.h"
#include "part.h"

void initApp(AppState& state)
{
        setSourcePart(state.parts, state.rootSourceID);
        setSourcePart(state.parts, state.rootSinkID);
        
        if (std::filesystem::exists("layouts"))
        {
                for (const auto& entry : std::filesystem::directory_iterator("layouts"))
                {
                        if (entry.path().extension() == ".json")
                        {
                                state.layoutFiles.push_back(entry.path().filename().string());
                        }
                }
        }
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

void dropPart(AppState& state, int type, Vector2 pos)
{
        int id = state.nextID++;
        if (type == PART_TYPE_SOURCE) setSourcePart(state.parts, id);
        else if (type == PART_TYPE_OUTPUT) setOutputPart(state.parts, id);
        else setPart(state.parts, id, getPartFromType((PartType)type));
        
        state.partTypes[id] = (PartType)type;
        state.positions[id] = {pos.x, pos.y};
        state.inputCounts[id] = 2;
        state.labels[id] = PART_TYPE_NAMES[type];
        state.simulation = nullptr;
}

void deletePart(AppState& state, int id)
{
        state.parts.erase(id);
        state.partTypes.erase(id);
        state.positions.erase(id);
        state.inputCounts.erase(id);
        
        std::vector<PartPin> toRemove;
        for(auto it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                if (it->first.first == id || it->second.first == id)
                {
                        toRemove.push_back(it->first);
                }
        }
        for(const auto& key : toRemove) state.connections.erase(key);
        state.selectedPartID = -1;
        state.simulation = nullptr;
}

void handleInput(AppState& state)
{
        float sideMenuWidth = state.showSideMenu ? DEFAULT_SIDEMENU_WIDTH : 0;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_TAB)) state.showSideMenu = !state.showSideMenu;
        if (IsKeyPressed(KEY_H)) state.showHelp = !state.showHelp;

        if (state.showSaveDialog)
        {
                int key = GetCharPressed();
                while (key > 0)
                {
                        if ((key >= 32) && (key <= 125))
                        {
                                int len = std::string(state.saveFileNameBuffer).length();
                                if (len < SAVE_FILENAME_MAX_LEN)
                                {
                                        state.saveFileNameBuffer[len] = (char)key;
                                        state.saveFileNameBuffer[len + 1] = '\0';
                                }
                        }
                        key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE))
                {
                        int len = std::string(state.saveFileNameBuffer).length();
                        if (len > 0) state.saveFileNameBuffer[len - 1] = '\0';
                }
                if (IsKeyPressed(KEY_ENTER))
                {
                        std::string fname = "layouts/" + std::string(state.saveFileNameBuffer) + ".json";
                        if (!std::filesystem::exists("layouts")) std::filesystem::create_directory("layouts");
                        saveLayout(state.parts, state.partTypes, state.connections, state.labels, 
                                   state.positions, state.inputCounts, fname);
                        state.showSaveDialog = false;
                        
                        state.layoutFiles.clear();
                        for (const auto& entry : std::filesystem::directory_iterator("layouts"))
                        {
                                if (entry.path().extension() == ".json") state.layoutFiles.push_back(entry.path().filename().string());
                        }
                }
                if (IsKeyPressed(KEY_ESCAPE)) state.showSaveDialog = false;
                return;
        }

        Vector2 mousePos = GetMousePosition();
        bool mouseOverSideMenu = (mousePos.x < sideMenuWidth);

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && state.draggingNewPartType != -1)
        {
                if (!mouseOverSideMenu)
                {
                        float gx = round(mousePos.x / GRID_SIZE) * GRID_SIZE;
                        float gy = round(mousePos.y / GRID_SIZE) * GRID_SIZE;
                        dropPart(state, state.draggingNewPartType, {gx, gy});
                }
                state.draggingNewPartType = -1;
                return;
        }

        if (state.contextMenu.active)
        {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                        Vector2 p = state.contextMenu.position;
                        Rectangle rAdd = {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT};
                        Rectangle rRem = {p.x, p.y + CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT};
                        Rectangle rDel = {p.x, p.y + CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT};

                        if (CheckCollisionPointRec(mousePos, rAdd)) {
                                state.inputCounts[state.contextMenu.targetPartID]++;
                                state.simulation = nullptr;
                                state.contextMenu.active = false;
                        } else if (CheckCollisionPointRec(mousePos, rRem)) {
                                if (state.inputCounts[state.contextMenu.targetPartID] > 0)
                                        state.inputCounts[state.contextMenu.targetPartID]--;
                                state.simulation = nullptr;
                                state.contextMenu.active = false;
                        } else if (CheckCollisionPointRec(mousePos, rDel)) {
                                deletePart(state, state.contextMenu.targetPartID);
                                state.contextMenu.active = false;
                        } else {
                                state.contextMenu.active = false;
                        }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) state.contextMenu.active = false;
                return;
        }

        if (IsKeyPressed(KEY_SPACE)) state.isSimulating = !state.isSimulating;
        if (IsKeyPressed(KEY_UP)) state.targetHZ *= 2.0f;
        if (IsKeyPressed(KEY_DOWN)) state.targetHZ *= 0.5f;
        if (IsKeyPressed(KEY_S)) state.showSaveDialog = true;
        
        if (IsKeyPressed(KEY_DELETE))
        {
                 if (state.selectedConnection.first != -1)
                 {
                         state.connections.erase(state.selectedConnection);
                         state.selectedConnection = {-1, -1};
                         state.simulation = nullptr;
                 }
                 else if (state.selectedPartID != -1)
                 {
                         deletePart(state, state.selectedPartID);
                 }
        }

        if (mouseOverSideMenu && state.draggingNewPartType == -1) return;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && state.draggingNewPartType == -1)
        {
                bool hitSomething = false;
                state.selectedPartID = -1;
                state.selectedConnection = {-1, -1};

                for (const auto& conn : state.connections)
                {
                        int fromID = conn.second.first;
                        int toID = conn.first.first;
                        Vector2 start = {state.positions[fromID].first + PART_SIZE/2, state.positions[fromID].second};
                        
                        int inCount = state.inputCounts[toID];
                        float pinYStep = (inCount > 1) ? (PART_SIZE - 10.0f) / (inCount - 1) : 0;
                        float yOff = -PART_SIZE/2 + PIN_Y_OFFSET_BASE + conn.first.second * pinYStep;
                        if (inCount <= 1) yOff = 0;
                        
                        Vector2 end = {state.positions[toID].first - PART_SIZE/2, state.positions[toID].second + yOff};
                        float midX = (start.x + end.x) / 2.0f;
                        
                        Vector2 p1 = {midX, start.y};
                        Vector2 p2 = {midX, end.y};
                        
                        Vector2 mp = mousePos;
                        bool hit = false;
                        
                        if (CheckCollisionPointRec(mp, {fminf(start.x, p1.x)-WIRE_HITBOX_PADDING, start.y-WIRE_HITBOX_PADDING, fabsf(start.x-p1.x)+WIRE_HITBOX_SIZE, WIRE_HITBOX_SIZE})) hit = true;
                        else if (CheckCollisionPointRec(mp, {p1.x-WIRE_HITBOX_PADDING, fminf(p1.y, p2.y)-WIRE_HITBOX_PADDING, WIRE_HITBOX_SIZE, fabsf(p1.y-p2.y)+WIRE_HITBOX_SIZE})) hit = true;
                        else if (CheckCollisionPointRec(mp, {fminf(p2.x, end.x)-WIRE_HITBOX_PADDING, end.y-WIRE_HITBOX_PADDING, fabsf(p2.x-end.x)+WIRE_HITBOX_SIZE, WIRE_HITBOX_SIZE})) hit = true;
                        
                        if (hit)
                        {
                                state.selectedConnection = conn.first;
                                hitSomething = true;
                                break;
                        }
                }
                
                if (!hitSomething)
                {
                        for (auto it = state.positions.begin(); it != state.positions.end(); ++it)
                        {
                                int id = it->first;
                                Vector2 pos = {it->second.first, it->second.second};
                                Rectangle body = {pos.x - PART_SIZE/2, pos.y - PART_SIZE/2, PART_SIZE, PART_SIZE};

                                int inCount = state.inputCounts.count(id) ? state.inputCounts[id] : 2;
                                if (state.partTypes[id] == PART_TYPE_SOURCE) inCount = 0;
                                
                                float pinYStep = (inCount > 1) ? (PART_SIZE - 10.0f) / (inCount - 1) : 0;
                                for (int i = 0; i < inCount; ++i)
                                {
                                        float yOff = -PART_SIZE/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                        if (inCount <= 1) yOff = 0;
                                        Rectangle pinRect = {pos.x - PART_SIZE/2 - PIN_SIZE, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                        
                                        if (CheckCollisionPointRec(mousePos, pinRect))
                                        {
                                                hitSomething = true;
                                                if (state.wireStartPartID != -1)
                                                {
                                                        state.connections[{id, i}] = {state.wireStartPartID, state.wireStartPin};
                                                        state.simulation = nullptr;
                                                        state.wireStartPartID = -1;
                                                }
                                                break;
                                        }
                                }
                                if (hitSomething) break;

                                int outCount = 1;
                                if (state.partTypes[id] == PART_TYPE_OUTPUT) outCount = 0;
                                for (int i = 0; i < outCount; ++i)
                                {
                                        Rectangle pinRect = {pos.x + PART_SIZE/2, pos.y - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                        if (CheckCollisionPointRec(mousePos, pinRect))
                                        {
                                                hitSomething = true;
                                                state.wireStartPartID = id;
                                                state.wireStartPin = i;
                                                break;
                                        }
                                }
                                if (hitSomething) break;

                                if (CheckCollisionPointRec(mousePos, body))
                                {
                                        state.dragPartID = id;
                                        state.dragStartMousePos = mousePos;
                                        state.dragPartStartPos = pos;
                                        state.isDragging = false;
                                        state.selectedPartID = id;
                                        hitSomething = true;
                                        break;
                                }
                        }
                }
                
                if (!hitSomething)
                {
                        state.wireStartPartID = -1;
                }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && state.dragPartID != -1)
        {
                if (Vector2Distance(state.dragStartMousePos, mousePos) > DRAG_THRESHOLD)
                {
                        state.isDragging = true;
                }
                if (state.isDragging)
                {
                        Vector2 diff = Vector2Subtract(mousePos, state.dragStartMousePos);
                        float gx = round((state.dragPartStartPos.x + diff.x) / GRID_SIZE) * GRID_SIZE;
                        float gy = round((state.dragPartStartPos.y + diff.y) / GRID_SIZE) * GRID_SIZE;
                        state.positions[state.dragPartID] = {gx, gy};
                }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
                if (state.dragPartID != -1 && !state.isDragging)
                {
                        int id = state.dragPartID;
                        if (state.partTypes[id] == PART_TYPE_SOURCE)
                        {
                                size_t index = 0;
                                for(auto srcIt = state.partTypes.begin(); srcIt != state.partTypes.end(); ++srcIt)
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
                }
                state.dragPartID = -1;
                state.isDragging = false;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
                state.wireStartPartID = -1;
                for (auto it = state.positions.begin(); it != state.positions.end(); ++it)
                {
                        Vector2 pos = {it->second.first, it->second.second};
                        Rectangle body = {pos.x - PART_SIZE/2, pos.y - PART_SIZE/2, PART_SIZE, PART_SIZE};
                        if (CheckCollisionPointRec(mousePos, body))
                        {
                                state.contextMenu.active = true;
                                state.contextMenu.targetPartID = it->first;
                                state.contextMenu.position = mousePos;
                                break;
                        }
                }
        }
}

void updateSimulation(AppState& state)
{
        if (!state.simulation) recompileSimulation(state);

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
        else if (IsKeyPressed(KEY_RIGHT)) step = true;

        if (step && state.simulation)
        {
                state.lastOutputStates = state.simulation(state.runtimeInput);
        }
}

void drawGrid(const AppState& state)
{
        float startX = state.showSideMenu ? DEFAULT_SIDEMENU_WIDTH : 0;
        for (float x = startX; x < GetScreenWidth(); x += GRID_SIZE) DrawLine(x, 0, x, GetScreenHeight(), LIGHTGRAY);
        for (float y = 0; y < GetScreenHeight(); y += GRID_SIZE) DrawLine(startX, y, GetScreenWidth(), y, LIGHTGRAY);
}

void drawWires(AppState& state)
{
        for (auto it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                int fromID = it->second.first;
                int toID = it->first.first;
                Vector2 start = {state.positions[fromID].first + PART_SIZE/2, state.positions[fromID].second};
                
                int inCount = state.inputCounts[toID];
                float pinYStep = (inCount > 1) ? (PART_SIZE - 10.0f) / (inCount - 1) : 0;
                float yOff = -PART_SIZE/2 + PIN_Y_OFFSET_BASE + it->first.second * pinYStep;
                if (inCount <= 1) yOff = 0;
                
                Vector2 end = {state.positions[toID].first - PART_SIZE/2, state.positions[toID].second + yOff};
                float midX = (start.x + end.x) / 2.0f;

                Color c = COLOR_WIRE_OFF;
                if (state.selectedConnection == it->first) c = COLOR_WIRE_SELECTED;
                
                DrawLineEx(start, {midX, start.y}, WIRE_THICKNESS, c);
                DrawLineEx({midX, start.y}, {midX, end.y}, WIRE_THICKNESS, c);
                DrawLineEx({midX, end.y}, end, WIRE_THICKNESS, c);
        }

        if (state.wireStartPartID != -1)
        {
                Vector2 start = {state.positions[state.wireStartPartID].first + PART_SIZE/2, state.positions[state.wireStartPartID].second};
                DrawLineV(start, GetMousePosition(), BLACK);
        }
}

void drawParts(AppState& state)
{
        for (auto it = state.positions.begin(); it != state.positions.end(); ++it)
        {
                int id = it->first;
                Vector2 pos = {it->second.first, it->second.second};
                PartType type = state.partTypes[id];

                Color color = COLOR_PART_BG;
                Color borderColor = COLOR_PART_BORDER;
                if (id == state.selectedPartID) borderColor = COLOR_PART_SELECTED;

                Rectangle body = {pos.x - PART_SIZE/2, pos.y - PART_SIZE/2, PART_SIZE, PART_SIZE};
                DrawRectangleRec(body, color);
                DrawRectangleLinesEx(body, 2.0f, borderColor);
                
                DrawText(state.labels[id].c_str(), body.x + PART_LABEL_OFFSET, body.y + PART_LABEL_OFFSET, 10, BLACK);

                if (type == PART_TYPE_SOURCE)
                {
                        size_t index = 0;
                        for(auto srcIt = state.partTypes.begin(); srcIt != state.partTypes.end(); ++srcIt) {
                                if (srcIt->second == PART_TYPE_SOURCE) { if (srcIt->first == id) break; index++; }
                        }
                        State s = (index < state.runtimeInput.size()) ? state.runtimeInput[index] : STATE_LOW;
                        Color ledColor = (s == STATE_HIGH) ? COLOR_LED_ON : COLOR_LED_OFF;
                        DrawRectangle(body.x + PART_LED_OFFSET_X, body.y + PART_LED_OFFSET_Y, PART_LED_SIZE, PART_LED_SIZE, ledColor);
                }
                else if (type == PART_TYPE_OUTPUT)
                {
                        size_t index = 0;
                        for(auto outIt = state.partTypes.begin(); outIt != state.partTypes.end(); ++outIt) {
                                if (outIt->second == PART_TYPE_OUTPUT) { if (outIt->first == id) break; index++; }
                        }
                        State s = (index < state.lastOutputStates.size()) ? state.lastOutputStates[index] : STATE_LOW;
                        Color ledColor = (s == STATE_HIGH) ? COLOR_LED_OUT_ON : COLOR_LED_OUT_OFF;
                        DrawRectangle(body.x + PART_LED_OFFSET_X, body.y + PART_LED_OFFSET_Y, PART_LED_SIZE, PART_LED_SIZE, ledColor);
                }

                int inCount = state.inputCounts.count(id) ? state.inputCounts[id] : 2;
                if (type != PART_TYPE_SOURCE)
                {
                        float pinYStep = (inCount > 1) ? (PART_SIZE - 10.0f) / (inCount - 1) : 0;
                        for (int i = 0; i < inCount; ++i)
                        {
                                float yOff = -PART_SIZE/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                if (inCount <= 1) yOff = 0;
                                Rectangle pinRect = {pos.x - PART_SIZE/2 - PIN_SIZE, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                DrawRectangleRec(pinRect, BLACK);
                        }
                }
                
                if (type != PART_TYPE_OUTPUT)
                {
                        Rectangle pinRect = {pos.x + PART_SIZE/2, pos.y - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                        DrawRectangleRec(pinRect, BLACK);
                }
        }
}

void drawApp(AppState& state)
{
        drawGrid(state);
        drawWires(state);
        drawParts(state);
        
        if (state.showSideMenu)
        {
                DrawRectangle(0, 0, DEFAULT_SIDEMENU_WIDTH, GetScreenHeight(), RAYWHITE);
                DrawLine(DEFAULT_SIDEMENU_WIDTH, 0, DEFAULT_SIDEMENU_WIDTH, GetScreenHeight(), GRAY);
                
                float y = SIDEMENU_Y_START;
                DrawText("Parts Library", SIDEMENU_PADDING_X, y, SIDEMENU_HEADER_TEXT_SIZE, DARKGRAY); y += SIDEMENU_HEADER_MARGIN;

                for (int i = 0; i <= PART_TYPE_OUTPUT; ++i)
                {
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, DEFAULT_SIDEMENU_WIDTH - SIDEMENU_BUTTON_MARGIN*2, SIDEMENU_BUTTON_HEIGHT};
                        DrawRectangleRec(btn, LIGHTGRAY);
                        DrawText(PART_TYPE_NAMES[i], btn.x + SIDEMENU_BUTTON_TEXT_OFFSET_X, btn.y + SIDEMENU_BUTTON_TEXT_OFFSET_Y, SIDEMENU_BUTTON_TEXT_SIZE, BLACK);
                        
                        if (CheckCollisionPointRec(GetMousePosition(), btn))
                        {
                                DrawRectangleLinesEx(btn, 2, BLUE);
                                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                                {
                                        state.draggingNewPartType = i;
                                }
                        }
                        y += SIDEMENU_BUTTON_SPACING;
                }

                y += 20;
                DrawText("Saved Layouts", SIDEMENU_PADDING_X, y, SIDEMENU_HEADER_TEXT_SIZE, DARKGRAY); y += SIDEMENU_HEADER_MARGIN;
                
                for (const auto& file : state.layoutFiles)
                {
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, DEFAULT_SIDEMENU_WIDTH - SIDEMENU_BUTTON_MARGIN*2, SIDEMENU_BUTTON_HEIGHT};
                        DrawRectangleRec(btn, LIGHTGRAY);
                        
                        std::filesystem::path p(file);
                        DrawText(p.stem().string().c_str(), btn.x + SIDEMENU_LIST_TEXT_OFFSET_X, btn.y + SIDEMENU_LIST_TEXT_OFFSET_Y, SIDEMENU_LIST_TEXT_SIZE, BLACK);
                        
                        if (CheckCollisionPointRec(GetMousePosition(), btn))
                        {
                                DrawRectangleLinesEx(btn, 2, BLUE);
                                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                                {
                                        Vector2 m = GetMousePosition();
                                        float gx = round(m.x / GRID_SIZE) * GRID_SIZE;
                                        float gy = round(m.y / GRID_SIZE) * GRID_SIZE;
                                        std::string path = "layouts/" + file;
                                        importLayout(state.parts, state.partTypes, state.connections, state.labels, 
                                                        state.positions, state.inputCounts, path, state.nextID, gx, gy);
                                        state.simulation = nullptr;
                                }
                        }
                        y += SIDEMENU_LIST_SPACING;
                }
        }

        if (state.draggingNewPartType != -1)
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - PART_SIZE/2, m.y - PART_SIZE/2, PART_SIZE, PART_SIZE, Fade(GRAY, 0.5f));
                DrawText(PART_TYPE_NAMES[state.draggingNewPartType], m.x, m.y, 10, BLACK);
        }

        if (state.showSaveDialog)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - SAVE_DIALOG_WIDTH/2, GetScreenHeight()/2 - SAVE_DIALOG_HEIGHT/2, SAVE_DIALOG_WIDTH, SAVE_DIALOG_HEIGHT, RAYWHITE);
                DrawText("Save As:", GetScreenWidth()/2 - SAVE_DIALOG_TEXT_X_OFFSET, GetScreenHeight()/2 - SAVE_DIALOG_TEXT_Y_OFFSET, 20, BLACK);
                
                Rectangle box = { (float)GetScreenWidth()/2 - SAVE_DIALOG_TEXT_X_OFFSET, (float)GetScreenHeight()/2 - SAVE_DIALOG_INPUT_Y_OFFSET, SAVE_DIALOG_INPUT_WIDTH, SAVE_DIALOG_INPUT_HEIGHT };
                DrawRectangleRec(box, LIGHTGRAY);
                DrawRectangleLinesEx(box, 1, DARKGRAY);
                DrawText(state.saveFileNameBuffer, box.x + 5, box.y + 5, 20, BLACK);
                DrawText(".json", box.x + SAVE_DIALOG_EXT_OFFSET, box.y + 5, 20, WHITE);
        }

        if (state.contextMenu.active)
        {
                Vector2 p = state.contextMenu.position;
                DrawRectangle(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*3, RAYWHITE);
                DrawRectangleLines(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*3, BLACK);
                
                DrawText("Add Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, BLACK);
                DrawText("Remove Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, BLACK);
                DrawText("Delete Part", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*2 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, RED);
                
                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT})) DrawRectangleLines(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y+CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT})) DrawRectangleLines(p.x, p.y+CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y+CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT})) DrawRectangleLines(p.x, p.y+CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
        }
        
        if (state.showHelp)
        {
                float helpX = (GetScreenWidth() - HELP_MENU_WIDTH) / 2.0f;
                float helpY = (GetScreenHeight() - HELP_MENU_HEIGHT) / 2.0f;
                
                DrawRectangle(helpX - HELP_SHADOW_OFFSET, helpY - HELP_SHADOW_OFFSET, HELP_MENU_WIDTH + HELP_SHADOW_OFFSET*2, HELP_MENU_HEIGHT + HELP_SHADOW_OFFSET*2, DARKGRAY);
                DrawRectangle(helpX, helpY, HELP_MENU_WIDTH, HELP_MENU_HEIGHT, RAYWHITE);
                
                float y = helpY + HELP_PADDING;
                float x = helpX + HELP_PADDING;
                DrawText("HELP (Press H to toggle)", x, y, HELP_HEADER_SIZE, BLACK); y += 30;
                DrawText("---------------------------", x, y, HELP_HEADER_SIZE, GRAY); y += 20;
                DrawText("General:", x, y, HELP_HEADER_SIZE, DARKBLUE); y += HELP_SECTION_SPACING;
                DrawText("  TAB: Toggle Sidemenu", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  F11: Fullscreen", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  S: Save Layout", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_SECTION_SPACING;

                DrawText("Editing:", x, y, HELP_HEADER_SIZE, DARKBLUE); y += HELP_SECTION_SPACING;
                DrawText("  Drag from Menu: Add Part", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  Drag Pin to Pin: Wire", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  Right Click Part: Options", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  DEL: Delete Selected", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_SECTION_SPACING;
                
                DrawText("Simulation:", x, y, HELP_HEADER_SIZE, DARKBLUE); y += HELP_SECTION_SPACING;
                DrawText("  SPACE: Run/Pause", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  Right Arrow: Step", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
                DrawText("  UP/DOWN: Speed (Hz)", x, y, HELP_TEXT_SIZE, BLACK); y += HELP_LINE_SPACING;
        }

        DrawText(std::format("HZ: {:.1f}", state.targetHZ).c_str(), GetScreenWidth() - HUD_HZ_X_OFFSET, HUD_HZ_Y, HUD_FONT_SIZE, BLACK);
}
