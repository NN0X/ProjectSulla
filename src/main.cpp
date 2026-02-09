#include <iostream>
#include <map>
#include <vector>
#include <functional>

#include <raylib/raylib.h>

#include "part.h"
#include "primitives.h"
#include "utils.h"

typedef struct CurrentLayout
{
        std::map<int, Part> parts;
        std::map<int, PartType> partTypes;
        std::map<PartPin, PartPin> connections;
        std::map<int, std::string> labels;
        std::map<int, Vector2> positions;
} CurrentLayout;

int main()
{
        const int screenWidth = 1280;
        const int screenHeight = 720;

        InitWindow(screenWidth, screenHeight, "Sulla - Logic Editor");
        SetTargetFPS(60);

        CurrentLayout layout;
        int outputID = loadLayout(layout.parts, layout.partTypes, layout.connections, layout.labels, layout.positions, "layouts/sr_latch_layout.json");

        if (outputID == -1)
        {
                setSourcePart(layout.parts, 0);
                layout.partTypes[0] = PART_TYPE_SOURCE;
                layout.labels[0] = "Inputs (S, R)";
                layout.positions[0] = {100, 300};

                setPart(layout.parts, 1, nandPart);
                layout.partTypes[1] = PART_TYPE_NAND;
                layout.labels[1] = "NAND 1";
                layout.positions[1] = {400, 200};

                setPart(layout.parts, 2, nandPart);
                layout.partTypes[2] = PART_TYPE_NAND;
                layout.labels[2] = "NAND 2";
                layout.positions[2] = {400, 400};

                setOutputPart(layout.parts, 3);
                layout.partTypes[3] = PART_TYPE_OUTPUT;
                layout.labels[3] = "Output";
                layout.positions[3] = {700, 300};

                connectParts(layout.connections, {0, 0}, {1, 0});
                connectParts(layout.connections, {0, 1}, {2, 1});
                connectParts(layout.connections, {1, 0}, {2, 0});
                connectParts(layout.connections, {2, 0}, {1, 1});
                connectParts(layout.connections, {1, 0}, {3, 0});
                
                outputID = 3;
        }

        Part circuit = assemblePart(layout.parts, layout.connections, outputID);
        std::vector<State> currentInputs = {STATE_HIGH, STATE_HIGH};
        std::vector<State> lastResult = {STATE_UNDEFINED};

        int draggedID = -1;
        Vector2 dragOffset = {0, 0};

        while (!WindowShouldClose())
        {
                Vector2 mousePos = GetMousePosition();

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                        for (auto& [id, pos] : layout.positions)
                        {
                                Rectangle rec = {pos.x, pos.y, 140, 80};
                                if (CheckCollisionPointRec(mousePos, rec))
                                {
                                        if (IsKeyDown(KEY_LEFT_SHIFT) && layout.partTypes[id] == PART_TYPE_SOURCE)
                                        {
                                                if (mousePos.y < pos.y + 40) currentInputs[0] = (currentInputs[0] == STATE_HIGH) ? STATE_LOW : STATE_HIGH;
                                                else currentInputs[1] = (currentInputs[1] == STATE_HIGH) ? STATE_LOW : STATE_HIGH;
                                        }
                                        else
                                        {
                                                draggedID = id;
                                                dragOffset = {pos.x - mousePos.x, pos.y - mousePos.y};
                                        }
                                        break;
                                }
                        }
                }

                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) draggedID = -1;

                if (draggedID != -1)
                {
                        layout.positions[draggedID] = {mousePos.x + dragOffset.x, mousePos.y + dragOffset.y};
                }

                if (IsKeyPressed(KEY_S)) saveLayout(layout.partTypes, layout.connections, layout.labels, layout.positions, "layouts/sr_latch_layout.json");
                if (IsKeyPressed(KEY_L))
                {
                        outputID = loadLayout(layout.parts, layout.partTypes, layout.connections, layout.labels, layout.positions, "layouts/sr_latch_layout.json");
                        circuit = assemblePart(layout.parts, layout.connections, outputID);
                }

                lastResult = circuit(currentInputs);

                BeginDrawing();
                ClearBackground(RAYWHITE);

                for (auto const& [to, from] : layout.connections)
                {
                        Vector2 start = {layout.positions[from.first].x + 140, layout.positions[from.first].y + 40};
                        Vector2 end = {layout.positions[to.first].x, layout.positions[to.first].y + 40};
                        DrawLineBezier(start, end, 2.0f, LIGHTGRAY);
                }

                for (auto const& [id, type] : layout.partTypes)
                {
                        Vector2 pos = layout.positions[id];
                        Rectangle rec = {pos.x, pos.y, 140, 80};
                        
                        DrawRectangleRec(rec, (id == draggedID) ? LIGHTGRAY : WHITE);
                        DrawRectangleLinesEx(rec, 2, DARKGRAY);
                        
                        DrawText(layout.labels[id].c_str(), pos.x + 10, pos.y + 10, 10, DARKGRAY);
                        DrawText(PART_TYPE_STRINGS[type].c_str(), pos.x + 10, pos.y + 25, 20, BLACK);

                        if (type == PART_TYPE_SOURCE)
                        {
                                DrawText(TextFormat("S: %s", STATE_STRINGS[currentInputs[0]].c_str()), pos.x + 10, pos.y + 50, 10, BLUE);
                                DrawText(TextFormat("R: %s", STATE_STRINGS[currentInputs[1]].c_str()), pos.x + 10, pos.y + 65, 10, RED);
                        }
                        else if (type == PART_TYPE_OUTPUT && !lastResult.empty())
                        {
                                Color stateCol = (lastResult[0] == STATE_HIGH) ? GREEN : (lastResult[0] == STATE_LOW) ? MAROON : GRAY;
                                DrawCircle(pos.x + 120, pos.y + 40, 10, stateCol);
                        }
                }

                DrawText("Drag to move | Shift+Click Source to toggle | S: Save | L: Load", 10, 10, 20, DARKGRAY);

                EndDrawing();
        }

        CloseWindow();
        return 0;
}
