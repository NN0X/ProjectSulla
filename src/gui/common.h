#ifndef COMMON_H
#define COMMON_H

#include <raylib/raylib.h>
#include <string>
#include "../appstate.h"

Color getThemeColor(const AppState& state, Color light, Color dark);
Vector2 getPartSize(const AppState& state, int id);
Rectangle getBodyRect(const AppState& state, int id);
int getPinCount(const AppState& state, int id, bool isInput);
float getPinYOffset(const AppState& state, int id, bool isInput, int index);
Rectangle getPinRect(const AppState& state, int id, bool isInput, int index);
Vector2 getPinPos(const AppState& state, int id, bool isInput, int index);
void refreshLayouts(AppState& state);
void refreshCompiledModules(AppState& state);
void drawTextFit(const char* text, float x, float y, float width, int fontSize, Color color);
bool buildNativeSimulation(AppState& state);
void recompileSimulation(AppState& state);
void updateHistory(AppState& state);
void doUndo(AppState& state);
void doRedo(AppState& state);
void copySelection(AppState& state);
void pasteClipboard(AppState& state, Vector2 offset);
void duplicateSelection(AppState& state);
void selectAllParts(AppState& state);
void fitView(AppState& state);

void tidyLayout(AppState& state);
void cleanupInputPinConnections(AppState& state, int partID, int removedIdx);
void cleanupOutputPinConnections(AppState& state, int partID, int removedIdx);
void doCompile(AppState& state, const std::string& modName);
bool handleDialogs(AppState& state);

void dropPart(AppState& state, int type, Vector2 pos);
void deleteParts(AppState& state);
void runBenchmark(AppState& state);
void drawBenchmark(AppState& state);

#endif
