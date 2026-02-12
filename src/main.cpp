#include <raylib/raylib.h>
#include "config.h"
#include "appstate.h"
#include "gui.h"

int main()
{
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, WINDOW_TITLE);
        SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

        AppState state;
        initApp(state);

        while (!WindowShouldClose())
        {
                handleInput(state);
                updateSimulation(state);

                BeginDrawing();
                ClearBackground(RAYWHITE);

                drawApp(state);

                EndDrawing();
        }

        CloseWindow();
        return 0;
}
