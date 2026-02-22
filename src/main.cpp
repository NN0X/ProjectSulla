#include <raylib/raylib.h>

#include "appstate.h"
#include "gui/gui.h"
#include "utils.h"
#include "compiler/compiler.h"

int main()
{
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(1280, 720, "Sulla");
        SetTargetFPS(60);

        AppState appState;
        appState.hasCompiler = checkCompilerAvailability();

        refreshLayouts(appState);

        while (!WindowShouldClose() && !appState.shouldQuit)
        {
                handleInput(appState);
                updateSimulation(appState);

                BeginDrawing();
                drawApp(appState);
                EndDrawing();
        }

        CloseWindow();
        return 0;
}
