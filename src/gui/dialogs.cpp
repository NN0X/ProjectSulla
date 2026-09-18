#include "gui.h"
#include "common.h"
#include <raylib/raylib.h>
#include "../config.h"
#include "../utils.h"
#include <filesystem>
#include <string>

bool handleDialogs(AppState& state)
{
        Vector2 mousePos = GetMousePosition();

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

                return true;
        }

        if (state.showSaveDialog || state.showLoadDialog || state.showRenameDialog || state.showCompileDialog)
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
                        if (state.showCompileDialog)
                        {
                                float chkX = GetScreenWidth()/2 - SAVE_DIALOG_INPUT_WIDTH/2;
                                float chkY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + 6;
                                Rectangle dynHit = { chkX, chkY, 90, 16 };
                                Rectangle staHit = { chkX + 100, chkY, 90, 16 };
                                if (CheckCollisionPointRec(mousePos, dynHit)) state.compileDynamic = !state.compileDynamic;
                                if (CheckCollisionPointRec(mousePos, staHit)) state.compileStatic = !state.compileStatic;
                        }
                        if (CheckCollisionPointRec(mousePos, cancelBtn))
                        {
                                state.showSaveDialog = false;
                                state.showLoadDialog = false;
                                state.showRenameDialog = false;
                                state.showCompileDialog = false;
                        }
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }

                if (IsKeyPressed(KEY_ENTER) || confirm)
                {
                        if (state.showRenameDialog)
                        {
                                state.labels[state.renamePartID] = state.fileNameBuffer;
                                state.showRenameDialog = false;
                                return true;
                        }
                        if (state.showCompileDialog)
                        {
                                std::string modName = std::string(state.fileNameBuffer);
                                doCompile(state, modName);
                                state.showCompileDialog = false;
                                return true;
                        }
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
                                                   state.positions, state.inputCounts, state.outputCounts, state.connectionWaypoints, fname);
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
                        state.showRenameDialog = false;
                        state.showCompileDialog = false;
                }
                return true;
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
                                   state.positions, state.inputCounts, state.outputCounts, state.connectionWaypoints, state.pendingSaveFilename);
                        state.showOverwriteConfirm = false;
                        refreshLayouts(state);
                }
                if (IsKeyPressed(KEY_ESCAPE))
                {
                        state.showOverwriteConfirm = false;
                        state.showSaveDialog = true;
                }
                return true;
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
                                state.partToDelete = "";
                        }
                        if (CheckCollisionPointRec(mousePos, confirmBtn)) confirm = true;
                }
                if (IsKeyPressed(KEY_ENTER) || confirm)
                {
                        if (!state.partToDelete.empty())
                        {
                                std::error_code ec;
                                std::filesystem::remove_all("parts/" + state.partToDelete, ec);
                                state.partToDelete = "";
                                refreshCompiledModules(state);
                        }
                        else
                        {
                                std::filesystem::remove("layouts/" + state.layoutToDelete);
                                refreshLayouts(state);
                        }
                        state.showDeleteConfirm = false;
                }
                if (IsKeyPressed(KEY_ESCAPE)) { state.showDeleteConfirm = false; state.partToDelete = ""; }
                return true;
        }

        return false;
}
