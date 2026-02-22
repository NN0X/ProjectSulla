#include "gui.h"
#include "common.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <format>

#include <raylib/raylib.h>
#include <raylib/raymath.h>

#include "../config.h"
#include "../primitives.h"
#include "../utils.h"
#include "../part.h"

void drawTextFit(const char* text, float x, float y, float width, int fontSize, Color color)
{
        int defaultSize = MeasureText(text, fontSize);
        if (defaultSize <= width)
        {
                DrawText(text, x, y, fontSize, color);
        }
        else
        {
                float scale = width / (float)defaultSize;
                int newSize = (int)(fontSize * scale);
                if (newSize < 5) newSize = 5;
                DrawText(text, x, y + (fontSize - newSize)/2, newSize, color);
        }
}

void drawGrid(const AppState& state)
{
        float startX = -20000.0f;
        float endX = 20000.0f;
        float startY = -20000.0f;
        float endY = 20000.0f;
        Color c = getThemeColor(state, COLOR_GRID_LIGHT, COLOR_GRID_DARK);

        for (float x = startX; x < endX; x += GRID_SIZE)
        {
                DrawLineV({x, startY}, {x, endY}, c);
        }
        for (float y = startY; y < endY; y += GRID_SIZE)
        {
                DrawLineV({startX, y}, {endX, y}, c);
        }
}

void drawWires(AppState& state)
{
        Color colorOff = state.darkMode ? COLOR_WIRE_OFF_DARK : COLOR_WIRE_OFF;

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

                Color c = colorOff;
                if (state.selectedConnection == it->first) c = COLOR_WIRE_SELECTED;

                DrawLineEx(start, {midX, start.y}, WIRE_THICKNESS, c);
                DrawLineEx({midX, start.y}, {midX, end.y}, WIRE_THICKNESS, c);
                DrawLineEx({midX, end.y}, end, WIRE_THICKNESS, c);

                Vector2 arrowPos = {midX, (start.y + end.y)/2.0f};
                if (fabs(start.y - end.y) < 2.0f) arrowPos = {(start.x + end.x)/2.0f, start.y};

                DrawCircleV(arrowPos, 2.0f, c);
        }

        if (state.wireStartPartID != -1)
        {
                int id = state.wireStartPartID;
                int pin = state.wireStartPin;
                Vector2 pos = {state.positions[id].first, state.positions[id].second};
                Vector2 size = getPartSize(state, id);
                int outCount = state.outputCounts[id];

                float pinYStep = (outCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (outCount - 1) : 0;
                float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + pin * pinYStep;
                if (outCount <= 1) yOff = 0;

                Vector2 start = {pos.x + size.x/2, pos.y + yOff};
                Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), state.camera);
                DrawLineEx(start, mouse, WIRE_THICKNESS, colorOff);
        }
}

void drawParts(AppState& state)
{
        Color cBg = getThemeColor(state, COLOR_PART_BG_LIGHT, COLOR_PART_BG_DARK);
        Color cBorder = getThemeColor(state, COLOR_PART_BORDER_LIGHT, COLOR_PART_BORDER_DARK);
        Color cText = getThemeColor(state, COLOR_TEXT_LIGHT, COLOR_TEXT_DARK);

        for (std::map<int, std::pair<float, float>>::iterator it = state.positions.begin(); it != state.positions.end(); ++it)
        {
                int id = it->first;
                Vector2 pos = {it->second.first, it->second.second};
                Vector2 size = getPartSize(state, id);
                PartType type = state.partTypes[id];

                Color borderColor = cBorder;
                if (state.selectedParts.count(id)) borderColor = COLOR_PART_SELECTED;

                Rectangle body = {pos.x - size.x/2, pos.y - size.y/2, size.x, size.y};
                DrawRectangleRec(body, cBg);
                DrawRectangleLinesEx(body, 2.0f, borderColor);

                int txtW = MeasureText(state.labels[id].c_str(), 10);
                DrawText(state.labels[id].c_str(), body.x + size.x/2 - txtW/2, body.y + PART_LABEL_OFFSET, 10, cText);

                int inCount = state.inputCounts[id];
                int outCount = state.outputCounts[id];

                if (type == PART_TYPE_SOURCE)
                {
                        float pinYStep = (outCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (outCount - 1) : 0;
                        for(int i=0; i<outCount; ++i)
                        {
                                float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                if (outCount <= 1) yOff = 0;

                                State s = (i < (int)state.sourceValues[id].size()) ? state.sourceValues[id][i] : STATE_LOW;
                                Color ledColor = (s == STATE_HIGH) ? COLOR_LED_ON : COLOR_LED_OFF;
                                DrawRectangle(pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE, ledColor);
                                DrawRectangleLines(pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE, cBorder);
                        }
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
                        Color ledColor = (s == STATE_HIGH) ? COLOR_LED_OUT_ON : COLOR_LED_OUT_OFF;
                        DrawRectangle(body.x + size.x/2 - PART_LED_SIZE/2, body.y + size.y - PART_LED_SIZE - 5, PART_LED_SIZE, PART_LED_SIZE, ledColor);
                }

                if (type != PART_TYPE_SOURCE)
                {
                        float pinYStep = (inCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (inCount - 1) : 0;
                        for (int i = 0; i < inCount; ++i)
                        {
                                float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                if (inCount <= 1) yOff = 0;
                                Rectangle pinRect = {pos.x - size.x/2 - PIN_SIZE, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                DrawRectangleRec(pinRect, cBorder);
                        }
                }

                if (outCount > 0)
                {
                         float pinYStep = (outCount > 1) ? (size.y - PIN_Y_OFFSET_BASE*2) / (outCount - 1) : 0;
                         for (int i = 0; i < outCount; ++i)
                         {
                                float yOff = -size.y/2 + PIN_Y_OFFSET_BASE + i * pinYStep;
                                if (outCount <= 1) yOff = 0;
                                Rectangle pinRect = {pos.x + size.x/2, pos.y + yOff - PIN_SIZE/2, PIN_SIZE, PIN_SIZE};
                                DrawRectangleRec(pinRect, cBorder);
                         }
                }
        }

        if (state.isBoxSelecting)
        {
                Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), state.camera);
                float minX = std::min(state.boxSelectStart.x, mouse.x);
                float maxX = std::max(state.boxSelectStart.x, mouse.x);
                float minY = std::min(state.boxSelectStart.y, mouse.y);
                float maxY = std::max(state.boxSelectStart.y, mouse.y);
                DrawRectangleRec({minX, minY, maxX - minX, maxY - minY}, COLOR_SELECTION_BOX);
                DrawRectangleLinesEx({minX, minY, maxX - minX, maxY - minY}, 1.0f, COLOR_SELECTION_BORDER);
        }
}

void drawUI(AppState& state)
{
        Color uiBg = getThemeColor(state, COLOR_UI_BG_LIGHT, COLOR_UI_BG_DARK);
        Color uiBorder = getThemeColor(state, COLOR_UI_BORDER_LIGHT, COLOR_UI_BORDER_DARK);
        Color textC = getThemeColor(state, COLOR_TEXT_LIGHT, COLOR_TEXT_DARK);

        DrawRectangle(0, 0, GetScreenWidth(), TOOLBAR_HEIGHT, uiBg);
        DrawLine(0, TOOLBAR_HEIGHT, GetScreenWidth(), TOOLBAR_HEIGHT, uiBorder);

        const char* btnLabels[] = { "Save", "Load", "Clear", "Help", "Dark", state.isSimulating ? "Pause" : "Play", "Step", "+", "-" };
        float x = TOOLBAR_PADDING;
        
        for (int i = 0; i < 9; ++i)
        {
                Rectangle btn = {x, TOOLBAR_PADDING, TOOLBAR_BTN_WIDTH, TOOLBAR_BTN_HEIGHT};
                bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                DrawRectangleRoundedLines(btn, 0.2f, 8, hovered ? BLUE : DARKGRAY);
                int tw = MeasureText(btnLabels[i], 10);
                DrawText(btnLabels[i], btn.x + btn.width/2 - tw/2, btn.y + btn.height/2 - 5, 10, BLACK);
                x += TOOLBAR_BTN_WIDTH + TOOLBAR_BTN_SPACING;
        }

        if (state.showSideMenu)
        {
                DrawRectangle(0, TOOLBAR_HEIGHT, DEFAULT_SIDEMENU_WIDTH, GetScreenHeight() - TOOLBAR_HEIGHT, uiBg);
                DrawLine(DEFAULT_SIDEMENU_WIDTH, TOOLBAR_HEIGHT, DEFAULT_SIDEMENU_WIDTH, GetScreenHeight(), uiBorder);

                float y = SIDEMENU_Y_START;
                DrawText("Parts Library", SIDEMENU_PADDING_X, y, SIDEMENU_HEADER_TEXT_SIZE, textC);
                y += SIDEMENU_HEADER_MARGIN;

                for (int i = 0; i <= PART_TYPE_OUTPUT; ++i)
                {
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, DEFAULT_SIDEMENU_WIDTH - SIDEMENU_BUTTON_MARGIN*2, SIDEMENU_BUTTON_HEIGHT};
                        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                        DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                        DrawText(PART_TYPE_NAMES[i], btn.x + SIDEMENU_BUTTON_TEXT_OFFSET_X, btn.y + SIDEMENU_BUTTON_TEXT_OFFSET_Y, SIDEMENU_BUTTON_TEXT_SIZE, BLACK);

                        if (hovered)
                        {
                                DrawRectangleRoundedLines(btn, 0.2f, 8, BLUE);
                                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) state.draggingNewPartType = i;
                        }
                        y += SIDEMENU_BUTTON_SPACING;
                }

                y += 20;
                DrawText("Saved Layouts", SIDEMENU_PADDING_X, y, SIDEMENU_HEADER_TEXT_SIZE, textC);
                y += SIDEMENU_HEADER_MARGIN;

                std::vector<std::string> files = state.layoutFiles;
                for (size_t i = 0; i < files.size(); ++i)
                {
                        std::string file = files[i];
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, DEFAULT_SIDEMENU_WIDTH - SIDEMENU_BUTTON_MARGIN*2 - 25, SIDEMENU_BUTTON_HEIGHT};
                        Rectangle delBtn = {btn.x + btn.width + 5, y + 5, SIDEMENU_DELETE_BTN_SIZE, SIDEMENU_DELETE_BTN_SIZE};
                        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);

                        DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                        std::filesystem::path p(file);
                        drawTextFit(p.stem().string().c_str(), btn.x + 5, btn.y + 5, btn.width - 10, SIDEMENU_LIST_TEXT_SIZE, BLACK);

                        if (hovered)
                        {
                                DrawRectangleRoundedLines(btn, 0.2f, 8, BLUE);
                                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && Vector2Distance(GetMouseDelta(), {0,0}) > 1.0f) state.draggingLayoutFile = file;
                        }

                        DrawRectangleRec(delBtn, RED);
                        DrawText("X", delBtn.x + 5, delBtn.y + 2, 10, WHITE);
                        if (CheckCollisionPointRec(GetMousePosition(), delBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                                state.layoutToDelete = file;
                                state.showDeleteConfirm = true;
                        }

                        y += SIDEMENU_LIST_SPACING;
                }
        }

        if (state.draggingNewPartType != -1)
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - BASE_PART_WIDTH/2, m.y - BASE_PART_HEIGHT/2, BASE_PART_WIDTH, BASE_PART_HEIGHT, Fade(GRAY, 0.5f));
                DrawText(PART_TYPE_NAMES[state.draggingNewPartType], m.x, m.y, 10, textC);
        }
        else if (state.draggingLayoutFile != "")
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - BASE_PART_WIDTH/2, m.y - BASE_PART_HEIGHT/2, BASE_PART_WIDTH, BASE_PART_HEIGHT, Fade(GRAY, 0.5f));
                DrawText(state.draggingLayoutFile.c_str(), m.x, m.y, 10, textC);
        }

        if (state.showSaveDialog || state.showLoadDialog)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBg);
                DrawRectangleLines(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBorder);

                const char* title = state.showSaveDialog ? "Save As:" : "Load Layout:";
                int tW = MeasureText(title, 20);
                DrawText(title, GetScreenWidth()/2 - tW/2, GetScreenHeight()/2 - SAVE_DIALOG_TEXT_Y_OFFSET, 20, textC);

                Rectangle box = { (float)GetScreenWidth()/2 - SAVE_DIALOG_INPUT_WIDTH/2, (float)GetScreenHeight()/2 - SAVE_DIALOG_INPUT_Y_OFFSET, SAVE_DIALOG_INPUT_WIDTH, SAVE_DIALOG_INPUT_HEIGHT };
                DrawRectangleRec(box, LIGHTGRAY);
                DrawRectangleLinesEx(box, 1, DARKGRAY);

                drawTextFit(state.fileNameBuffer, box.x + 5, box.y + 5, SAVE_DIALOG_INPUT_WIDTH - 60, 20, BLACK);
                
                state.cursorTimer += GetFrameTime();
                if (fmod(state.cursorTimer, 1.0f) < 0.5f)
                {
                        int defaultSize = MeasureText(state.fileNameBuffer, 20);
                        if (defaultSize <= SAVE_DIALOG_INPUT_WIDTH - 60)
                        {
                                DrawRectangle(box.x + 5 + defaultSize + 2, box.y + 5, 10, 20, BLACK);
                        }
                        else
                        {
                                float scale = (SAVE_DIALOG_INPUT_WIDTH - 60) / (float)defaultSize;
                                int newSize = (int)(20 * scale);
                                if (newSize < 5) newSize = 5;
                                DrawRectangle(box.x + 5 + (SAVE_DIALOG_INPUT_WIDTH - 60) + 2, box.y + 5 + (20 - newSize)/2, 5, newSize, BLACK);
                        }
                }

                int extWidth = MeasureText(".json", 20);
                DrawText(".json", box.x + box.width - extWidth - 5, box.y + 5, 20, BLACK);

                float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;

                Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};

                DrawRectangleRounded(cancelBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(cancelBtn, 0.2f, 8, DARKGRAY);
                int cW = MeasureText("Cancel", 10);
                DrawText("Cancel", cancelBtn.x + cancelBtn.width/2 - cW/2, cancelBtn.y + cancelBtn.height/2 - 5, 10, BLACK);

                DrawRectangleRounded(confirmBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(confirmBtn, 0.2f, 8, DARKGRAY);
                const char* confirmLabel = state.showSaveDialog ? "Save" : "Load";
                int fW = MeasureText(confirmLabel, 10);
                DrawText(confirmLabel, confirmBtn.x + confirmBtn.width/2 - fW/2, confirmBtn.y + confirmBtn.height/2 - 5, 10, BLACK);
        }

        if (state.showOverwriteConfirm)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBg);
                DrawRectangleLines(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBorder);

                const char* oText = "File Exists. Overwrite?";
                int oW = MeasureText(oText, 20);
                DrawText(oText, GetScreenWidth()/2 - oW/2, GetScreenHeight()/2 - 30, 20, textC);

                float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;

                Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};

                DrawRectangleRounded(cancelBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(cancelBtn, 0.2f, 8, DARKGRAY);
                int cW = MeasureText("Cancel", 10);
                DrawText("Cancel", cancelBtn.x + cancelBtn.width/2 - cW/2, cancelBtn.y + cancelBtn.height/2 - 5, 10, BLACK);

                DrawRectangleRounded(confirmBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(confirmBtn, 0.2f, 8, DARKGRAY);
                int overW = MeasureText("Overwrite", 10);
                DrawText("Overwrite", confirmBtn.x + confirmBtn.width/2 - overW/2, confirmBtn.y + confirmBtn.height/2 - 5, 10, BLACK);
        }

        if (state.showDeleteConfirm)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBg);
                DrawRectangleLines(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBorder);

                const char* dText = "Confirm Delete?";
                int dW = MeasureText(dText, 20);
                DrawText(dText, GetScreenWidth()/2 - dW/2, GetScreenHeight()/2 - 30, 20, textC);

                float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;

                Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};

                DrawRectangleRounded(cancelBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(cancelBtn, 0.2f, 8, DARKGRAY);
                int cW = MeasureText("Cancel", 10);
                DrawText("Cancel", cancelBtn.x + cancelBtn.width/2 - cW/2, cancelBtn.y + cancelBtn.height/2 - 5, 10, BLACK);

                DrawRectangleRounded(confirmBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(confirmBtn, 0.2f, 8, DARKGRAY);
                int delW = MeasureText("Delete", 10);
                DrawText("Delete", confirmBtn.x + confirmBtn.width/2 - delW/2, confirmBtn.y + confirmBtn.height/2 - 5, 10, BLACK);
        }

        if (state.showQuitConfirm)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBg);
                DrawRectangleLines(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBorder);

                const char* qText = "Are you sure you want to quit?";
                int qW = MeasureText(qText, 20);
                DrawText(qText, GetScreenWidth()/2 - qW/2, GetScreenHeight()/2 - 30, 20, textC);

                float btnY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + SAVE_DIALOG_BTN_Y_OFFSET;
                float startX = GetScreenWidth()/2 - SAVE_DIALOG_BTN_WIDTH - SAVE_DIALOG_BTN_SPACING/2;

                Rectangle cancelBtn = {startX, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};
                Rectangle confirmBtn = {startX + SAVE_DIALOG_BTN_WIDTH + SAVE_DIALOG_BTN_SPACING, btnY, SAVE_DIALOG_BTN_WIDTH, SAVE_DIALOG_BTN_HEIGHT};

                DrawRectangleRounded(cancelBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(cancelBtn, 0.2f, 8, DARKGRAY);
                int cW = MeasureText("Cancel", 10);
                DrawText("Cancel", cancelBtn.x + cancelBtn.width/2 - cW/2, cancelBtn.y + cancelBtn.height/2 - 5, 10, BLACK);

                DrawRectangleRounded(confirmBtn, 0.2f, 8, LIGHTGRAY);
                DrawRectangleRoundedLines(confirmBtn, 0.2f, 8, DARKGRAY);
                int qbW = MeasureText("Quit", 10);
                DrawText("Quit", confirmBtn.x + confirmBtn.width/2 - qbW/2, confirmBtn.y + confirmBtn.height/2 - 5, 10, BLACK);
        }

        if (state.contextMenu.active)
        {
                Vector2 p = state.contextMenu.position;
                DrawRectangle(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*3, uiBg);
                DrawRectangleLines(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*3, uiBorder);

                DrawText("Add Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                DrawText("Remove Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                DrawText("Delete Part", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*2 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, RED);

                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT}))
                {
                        DrawRectangleLines(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                }
                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y+CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT}))
                {
                        DrawRectangleLines(p.x, p.y+CM_ROW_HEIGHT, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                }
                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y+CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT}))
                {
                        DrawRectangleLines(p.x, p.y+CM_ROW_HEIGHT*2, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                }
        }

        if (state.showHelp)
        {
                float helpX = (GetScreenWidth() - HELP_MENU_WIDTH) / 2.0f;
                float helpY = (GetScreenHeight() - HELP_MENU_HEIGHT) / 2.0f;

                DrawRectangle(helpX - HELP_SHADOW_OFFSET, helpY - HELP_SHADOW_OFFSET, HELP_MENU_WIDTH + HELP_SHADOW_OFFSET*2, HELP_MENU_HEIGHT + HELP_SHADOW_OFFSET*2, DARKGRAY);
                DrawRectangle(helpX, helpY, HELP_MENU_WIDTH, HELP_MENU_HEIGHT, uiBg);

                float y = helpY + HELP_PADDING;
                float x = helpX + HELP_PADDING;
                DrawText("HELP (Press H to toggle)", x, y, HELP_HEADER_SIZE, textC);
                y += 30;
                DrawText("---------------------------", x, y, HELP_HEADER_SIZE, GRAY);
                y += 20;
                DrawText("General:", x, y, HELP_HEADER_SIZE, DARKBLUE);
                y += HELP_SECTION_SPACING;
                DrawText("  TAB: Toggle Sidemenu", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  F11: Fullscreen", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  Mouse Wheel: Zoom", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  S: Save Layout, L: Load Layout", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_SECTION_SPACING;
                DrawText("  D: Toggle Dark Mode", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_SECTION_SPACING;

                DrawText("Editing:", x, y, HELP_HEADER_SIZE, DARKBLUE);
                y += HELP_SECTION_SPACING;
                DrawText("  Drag from Menu: Add Part", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  Shift+Click: Multi-select", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  Right Click Part: Options", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;

                DrawText("Simulation:", x, y, HELP_HEADER_SIZE, DARKBLUE);
                y += HELP_SECTION_SPACING;
                DrawText("  SPACE: Run/Pause", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  Right Arrow: Step", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
                DrawText("  UP/DOWN: Speed (Hz)", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
        }

        std::string hzStr;
        if (state.targetHZ >= 1000000.0f) hzStr = std::format("{:.1f} MHz", state.targetHZ / 1000000.0f);
        else if (state.targetHZ >= 1000.0f) hzStr = std::format("{:.1f} kHz", state.targetHZ / 1000.0f);
        else hzStr = std::format("{:.1f} Hz", state.targetHZ);

        DrawText(hzStr.c_str(), GetScreenWidth() - HUD_X_OFFSET, HUD_Y, HUD_FONT_SIZE, textC);
        DrawText(std::format("Tick: {}", state.stepCount).c_str(), GetScreenWidth() - HUD_X_OFFSET, HUD_Y + 25, HUD_FONT_SIZE, textC);

        Rectangle resetBtn = { (float)GetScreenWidth() - HUD_X_OFFSET, HUD_Y + 50, 60, HUD_BTN_SIZE };
        DrawRectangleRec(resetBtn, LIGHTGRAY);
        DrawText("Reset", resetBtn.x + 5, resetBtn.y + 2, 10, BLACK);
        if (CheckCollisionPointRec(GetMousePosition(), resetBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
                state.stepCount = 0;
        }
}

void drawApp(AppState& state)
{
        Color bg = getThemeColor(state, COLOR_BG_LIGHT, COLOR_BG_DARK);
        ClearBackground(bg);

        BeginMode2D(state.camera);
        drawGrid(state);
        drawWires(state);
        drawParts(state);
        EndMode2D();

        drawUI(state);
}
