#ifndef GUI_H
#define GUI_H

#include "../appstate.h"

void initApp(AppState& state);
void handleInput(AppState& state);
void updateSimulation(AppState& state);
void drawApp(AppState& state);

#endif
