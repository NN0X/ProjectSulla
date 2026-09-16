#include "gui.h"
#include "common.h"
#include "../gates.h"

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

static const Color WIRE_PALETTE[] = {
        {66, 135, 245, 255},
        {235, 64, 52, 255},
        {46, 184, 46, 255},
        {168, 50, 186, 255},
        {230, 150, 20, 255},
        {20, 184, 184, 255},
        {184, 62, 120, 255},
        {120, 100, 50, 255},
        {100, 100, 220, 255},
        {200, 200, 40, 255},
        {50, 150, 100, 255},
        {180, 80, 80, 255},
};
static const int WIRE_PALETTE_SIZE = sizeof(WIRE_PALETTE) / sizeof(WIRE_PALETTE[0]);

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
        Vector2 tl = GetScreenToWorld2D({0.0f, 0.0f}, state.camera);
        Vector2 br = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, state.camera);
        float startX = floorf(tl.x / GRID_SIZE) * GRID_SIZE;
        float endX = br.x + GRID_SIZE;
        float startY = floorf(tl.y / GRID_SIZE) * GRID_SIZE;
        float endY = br.y + GRID_SIZE;
        if ((endX - startX) / GRID_SIZE > 600.0f || (endY - startY) / GRID_SIZE > 600.0f) return;
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

static size_t computeOutputSlotIndex(const AppState& state, int targetID)
{
        size_t index = 0;
        for (std::map<int, PartType>::const_iterator it = state.partTypes.begin(); it != state.partTypes.end(); ++it)
        {
                if (it->first == targetID) break;
                if (it->second == PART_TYPE_OUTPUT || it->second == PART_TYPE_DISPLAY)
                {
                        index += state.inputCounts.at(it->first);
                }
        }
        return index;
}

void drawWires(AppState& state)
{
        const float KINK = 16.0f;
        const float HB = 14.0f, HS = 8.0f;
        const float VB = 14.0f, VS = 7.0f;

        struct W { std::map<PartPin, PartPin>::iterator it; Vector2 s; Vector2 e; bool fwd; int ci; float laneY; float vx1; float vx2; };
        std::vector<W> ws;
        ws.reserve(state.connections.size());
        std::map<PartPin, int> fanout;
        for (std::map<PartPin, PartPin>::iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                Vector2 s = getPinPos(state, it->second.first, false, it->second.second);
                Vector2 e = getPinPos(state, it->first.first, true, it->first.second);
                bool fwd = (s.x + KINK) < (e.x - KINK);
                int ci;
                std::map<PartPin, int>::iterator cit = state.connColorIdx.find(it->first);
                if (cit == state.connColorIdx.end()) { ci = state.nextConnColor++; state.connColorIdx[it->first] = ci; }
                else ci = cit->second;
                ws.push_back({it, s, e, fwd, ci, (s.y + e.y) / 2.0f, s.x + KINK, e.x - KINK});
                fanout[it->second]++;
        }

        std::map<int, std::vector<int> > hb;
        for (size_t i = 0; i < ws.size(); ++i)
                if (ws[i].fwd) hb[(int)roundf(ws[i].laneY / HB)].push_back((int)i);
        for (std::map<int, std::vector<int> >::iterator b = hb.begin(); b != hb.end(); ++b)
        {
                std::vector<int>& g = b->second;
                for (size_t r = 0; r < g.size(); ++r)
                        ws[g[r]].laneY += (float)((int)r - (int)((g.size() - 1) / 2)) * HS;
        }

        struct V { int wi; int slot; float x; };
        std::vector<V> verts;
        for (size_t i = 0; i < ws.size(); ++i)
                if (ws[i].fwd) { verts.push_back({(int)i, 0, ws[i].vx1}); verts.push_back({(int)i, 1, ws[i].vx2}); }
        std::map<int, std::vector<int> > vb;
        for (size_t k = 0; k < verts.size(); ++k) vb[(int)roundf(verts[k].x / VB)].push_back((int)k);
        for (std::map<int, std::vector<int> >::iterator b = vb.begin(); b != vb.end(); ++b)
        {
                std::vector<int>& g = b->second;
                for (size_t r = 0; r < g.size(); ++r)
                {
                        float off = (float)((int)r - (int)((g.size() - 1) / 2)) * VS;
                        if (verts[g[r]].slot == 0) ws[verts[g[r]].wi].vx1 += off;
                        else ws[verts[g[r]].wi].vx2 += off;
                }
        }

        for (size_t i = 0; i < ws.size(); ++i)
        {
                std::map<PartPin, PartPin>::iterator it = ws[i].it;
                Vector2 s = ws[i].s;
                Vector2 e = ws[i].e;
                bool hovered = (state.hoveredNet.first != -1 && it->second == state.hoveredNet);

                Color c;
                if (state.selectedConnection == it->first)
                        c = COLOR_WIRE_SELECTED;
                else
                {
                        c = WIRE_PALETTE[ws[i].ci % WIRE_PALETTE_SIZE];
                        if (state.visualizeSignals)
                        {
                                State v = STATE_LOW;
                                std::map<int, std::vector<State>>::iterator nit = state.netStates.find(it->second.first);
                                if (nit != state.netStates.end() && it->second.second < (int)nit->second.size())
                                        v = nit->second[it->second.second];
                                if (v != STATE_HIGH)
                                        c = (Color){ (unsigned char)(c.r * 0.34f), (unsigned char)(c.g * 0.34f), (unsigned char)(c.b * 0.34f), 255 };
                        }
                        if (hovered) c = (Color){ (unsigned char)(c.r + (255 - c.r) * 0.5f), (unsigned char)(c.g + (255 - c.g) * 0.5f), (unsigned char)(c.b + (255 - c.b) * 0.5f), 255 };
                }
                float th = hovered ? WIRE_THICKNESS + 1.5f : WIRE_THICKNESS;

                if (ws[i].fwd)
                {
                        float vx1 = ws[i].vx1, vx2 = ws[i].vx2, ly = ws[i].laneY;
                        DrawLineEx(s, {vx1, s.y}, th, c);
                        DrawLineEx({vx1, s.y}, {vx1, ly}, th, c);
                        DrawLineEx({vx1, ly}, {vx2, ly}, th, c);
                        DrawLineEx({vx2, ly}, {vx2, e.y}, th, c);
                        DrawLineEx({vx2, e.y}, e, th, c);
                }
                else
                {
                        float base = (s.y > e.y ? s.y : e.y) + 45.0f;
                        float midY = base + (float)(ws[i].ci % 6) * 9.0f;
                        float vx1 = s.x + KINK;
                        float vx2 = e.x - KINK;
                        DrawLineEx(s, {vx1, s.y}, th, c);
                        DrawLineEx({vx1, s.y}, {vx1, midY}, th, c);
                        DrawLineEx({vx1, midY}, {vx2, midY}, th, c);
                        DrawLineEx({vx2, midY}, {vx2, e.y}, th, c);
                        DrawLineEx({vx2, e.y}, e, th, c);
                }

                if (fanout[it->second] > 1) DrawCircleV(s, 3.0f, c);
        }
        if (state.wireStartPartID != -1)
        {
                int id = state.wireStartPartID;
                int pin = state.wireStartPin;
                Vector2 start = getPinPos(state, id, false, pin);
                Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), state.camera);
                Vector2 kink = {start.x + KINK, start.y};
                Color colorOff = state.darkMode ? COLOR_WIRE_OFF_DARK : COLOR_WIRE_OFF;
                DrawLineEx(start, kink, WIRE_THICKNESS, colorOff);
                DrawLineEx(kink, mouse, WIRE_THICKNESS, colorOff);
        }
}

static bool isLogicGate(PartType t)
{
        return t == PART_TYPE_AND || t == PART_TYPE_OR || t == PART_TYPE_NOT || t == PART_TYPE_NAND ||
               t == PART_TYPE_NOR || t == PART_TYPE_XOR || t == PART_TYPE_XNOR;
}

static Color gateAccent(PartType t)
{
        switch (t)
        {
        case PART_TYPE_AND:  return (Color){ 90, 156, 220, 255};
        case PART_TYPE_NAND: return (Color){120, 180, 230, 255};
        case PART_TYPE_OR:   return (Color){ 92, 184,  92, 255};
        case PART_TYPE_NOR:  return (Color){ 80, 190, 160, 255};
        case PART_TYPE_XOR:  return (Color){170, 120, 210, 255};
        case PART_TYPE_XNOR: return (Color){210, 120, 190, 255};
        case PART_TYPE_NOT:  return (Color){230, 150,  70, 255};
        default:             return (Color){160, 160, 170, 255};
        }
}

static void drawToolbarIcon(int idx, Rectangle b, Color c, Color bg, const AppState& state);
static void drawPin(Rectangle r, bool isInput, bool connected, int sig)
{
        Color base = isInput ? (Color){95, 150, 230, 255} : (Color){90, 200, 130, 255};
        if (sig == 1) base = (Color){120, 225, 150, 255};
        else if (sig == 0) base = (Color){78, 90, 104, 255};
        if (isInput && !connected)
        {
                DrawRectangleRounded(r, 0.35f, 4, (Color){40, 44, 50, 255});
                DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.5f, (Color){115, 122, 132, 255});
        }
        else
        {
                DrawRectangleRounded(r, 0.35f, 4, base);
                DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f, (Color){20, 22, 26, 160});
        }
}

static void drawGateGlyph(PartType type, Rectangle b, Color fill, Color accent)
{
        float x = b.x, y = b.y, w = b.width, h = b.height;
        bool neg = (type == PART_TYPE_NAND || type == PART_TYPE_NOR || type == PART_TYPE_XNOR || type == PART_TYPE_NOT);
        bool orLike = (type == PART_TYPE_OR || type == PART_TYPE_NOR || type == PART_TYPE_XOR || type == PART_TYPE_XNOR);
        bool xorLike = (type == PART_TYPE_XOR || type == PART_TYPE_XNOR);
        float bubbleR = 4.0f;
        float gw = w - (neg ? bubbleR * 2.0f + 1.0f : 0.0f);

        if (type == PART_TYPE_NOT)
        {
                Vector2 p1 = {x, y}, p2 = {x, y + h}, p3 = {x + gw, y + h/2};
                DrawTriangle(p2, p1, p3, fill);
                DrawTriangleLines(p2, p1, p3, accent);
        }
        else if (!orLike)
        {
                float r = h / 2.0f;
                float flat = x + gw - r;
                if (flat < x) flat = x;
                DrawRectangleRec({x, y, flat - x, h}, fill);
                DrawCircleSector({flat, y + r}, r, -90.0f, 90.0f, 32, fill);
                DrawLine((int)x, (int)y, (int)flat, (int)y, accent);
                DrawLine((int)x, (int)(y + h), (int)flat, (int)(y + h), accent);
                DrawLine((int)x, (int)y, (int)x, (int)(y + h), accent);
                DrawCircleSectorLines({flat, y + r}, r, -90.0f, 90.0f, 32, accent);
        }
        else
        {
                Vector2 bt = {x, y};
                Vector2 bb = {x, y + h};
                Vector2 bm = {x + w * 0.24f, y + h/2};
                Vector2 tip = {x + gw, y + h/2};
                DrawTriangle(bm, bt, tip, fill);
                DrawTriangle(bb, bm, tip, fill);
                DrawLineEx(bt, tip, 1.5f, accent);
                DrawLineEx(bb, tip, 1.5f, accent);
                DrawLineEx(bt, bm, 1.5f, accent);
                DrawLineEx(bm, bb, 1.5f, accent);
                if (xorLike)
                {
                        Vector2 xt = {x - 4, y};
                        Vector2 xb = {x - 4, y + h};
                        Vector2 xm = {x - 4 + w * 0.24f, y + h/2};
                        DrawLineEx(xt, xm, 1.5f, accent);
                        DrawLineEx(xm, xb, 1.5f, accent);
                }
        }
        if (neg)
        {
                Vector2 bc = {x + gw + bubbleR, y + h/2};
                DrawCircle((int)bc.x, (int)bc.y, bubbleR, fill);
                DrawCircleLines((int)bc.x, (int)bc.y, bubbleR, accent);
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
                if (isLogicGate(type))
                {
                        Color accent = state.selectedParts.count(id) ? COLOR_PART_SELECTED : gateAccent(type);
                        const std::string& partName = state.labels[id];
                        float titleH = partName.empty() ? 0.0f : PART_TITLE_HEIGHT;
                        if (!partName.empty())
                        {
                                int txtW = MeasureText(partName.c_str(), 10);
                                float nx = body.x + (size.x - txtW) / 2.0f;
                                if (nx < body.x + 2) nx = body.x + 2;
                                drawTextFit(partName.c_str(), nx, body.y + PART_LABEL_OFFSET, size.x - TEXT_PADDING, 10, cText);
                        }
                        Rectangle glyph = {body.x, body.y + titleH, size.x, size.y - titleH};
                        drawGateGlyph(type, glyph, cBg, accent);
                }
                else
                {
                        DrawRectangleRounded(body, 0.12f, 6, cBg);
                        DrawRectangleRoundedLines(body, 0.12f, 6, borderColor);
                        const std::string& partName = state.labels[id];
                        if (!partName.empty())
                        {
                                int txtW = MeasureText(partName.c_str(), 10);
                                float availW = size.x - TEXT_PADDING;
                                float nx = body.x + (size.x - txtW) / 2.0f;
                                if (nx < body.x + 2) nx = body.x + 2;
                                drawTextFit(partName.c_str(), nx, body.y + PART_LABEL_OFFSET, availW, 10, cText);
                        }
                }
                int inCount = state.inputCounts[id];
                int outCount = state.outputCounts[id];
                if (type == PART_TYPE_SOURCE)
                {
                        for(int i=0; i<outCount; ++i)
                        {
                                float yOff = getPinYOffset(state, id, false, i);
                                State s = (i < (int)state.sourceValues[id].size()) ? state.sourceValues[id][i] : STATE_LOW;
                                Color ledColor = (s == STATE_HIGH) ? COLOR_LED_ON : COLOR_LED_OFF;
                                DrawRectangle(pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE, ledColor);
                                DrawRectangleLines(pos.x - size.x/2 + 5, pos.y + yOff - SOURCE_TOGGLE_SIZE/2, SOURCE_TOGGLE_SIZE, SOURCE_TOGGLE_SIZE, cBorder);
                        }
                }
                else if (type == PART_TYPE_OUTPUT)
                {
                        size_t index = computeOutputSlotIndex(state, id);
                        for (int i = 0; i < inCount; ++i)
                        {
                                float yOff = getPinYOffset(state, id, true, i);
                                State s = (index + i < state.lastOutputStates.size()) ? state.lastOutputStates[index + i] : STATE_LOW;
                                Color ledColor = (s == STATE_HIGH) ? COLOR_LED_OUT_ON : COLOR_LED_OUT_OFF;
                                DrawRectangle(pos.x + size.x/2 - PART_LED_SIZE - 5, pos.y + yOff - PART_LED_SIZE/2, PART_LED_SIZE, PART_LED_SIZE, ledColor);
                        }
                }
                else if (type == PART_TYPE_DISPLAY)
                {
                        Rectangle screen = {body.x + DISPLAY_SCREEN_MARGIN, body.y + 14, size.x - DISPLAY_SCREEN_MARGIN*2, size.y - 17};
                        Color screenColor = DISPLAY_SCREEN_COLOR;
                        DrawRectangleRec(screen, screenColor);

                        size_t index = computeOutputSlotIndex(state, id);
                        int decimalValue = 0;
                        for (int i = 0; i < inCount; ++i)
                        {
                                State s = (index + i < state.lastOutputStates.size()) ? state.lastOutputStates[index + i] : STATE_LOW;
                                if (s == STATE_HIGH) decimalValue |= (1 << i);
                        }
                        std::string valStr = std::to_string(decimalValue);
                        int valW = MeasureText(valStr.c_str(), DISPLAY_FONT_SIZE);
                        Color valColor = DISPLAY_VALUE_COLOR;
                        DrawText(valStr.c_str(), (int)(screen.x + screen.width/2 - valW/2), (int)(screen.y + screen.height/2 - DISPLAY_FONT_SIZE/2), DISPLAY_FONT_SIZE, valColor);
                }
                else if (type == PART_TYPE_CLOCK)
                {
                        float cy = body.y + PART_TITLE_HEIGHT + (size.y - PART_TITLE_HEIGHT) / 2.0f;
                        float lo = cy + 7, hi = cy - 7;
                        float x0 = body.x + 8, x1 = body.x + size.x - 10;
                        Color wave = gateAccent(PART_TYPE_NOT);
                        float half = 9.0f;
                        float stepTime = 1.0f / (state.targetHZ > 0.0001f ? state.targetHZ : 1.0f);
                        float sub = (state.isSimulating && stepTime > 0.0f) ? (state.simTimer / stepTime) : 0.0f;
                        if (sub < 0.0f) sub = 0.0f; if (sub > 1.0f) sub = 1.0f;
                        float phase = (float)(state.stepCount % 100000ULL) + sub;
                        float scroll = fmodf(phase * half, 2.0f * half);
                        auto levelAt = [&](float xx) -> float { long k = (long)floorf((xx + scroll) / half); return (((k % 2) + 2) % 2 == 0) ? lo : hi; };
                        Vector2 last = {x0, levelAt(x0)};
                        float firstB = ceilf((x0 + scroll) / half) * half - scroll;
                        for (float bx = firstB; bx < x1 - 0.5f; bx += half)
                        {
                                if (bx <= x0) continue;
                                DrawLineEx(last, {bx, last.y}, 1.5f, wave);
                                float ny = (last.y == lo) ? hi : lo;
                                DrawLineEx({bx, last.y}, {bx, ny}, 1.5f, wave);
                                last = {bx, ny};
                        }
                        DrawLineEx(last, {x1, last.y}, 1.5f, wave);
                }
                if (type != PART_TYPE_SOURCE && type != PART_TYPE_CLOCK)
                {
                        for (int i = 0; i < inCount; ++i)
                        {
                                Rectangle pinRect = getPinRect(state, id, true, i);
                                float yOff = getPinYOffset(state, id, true, i);
                                bool iconn = state.connections.count({id, i}) > 0;
                                int isig = -1;
                                if (iconn)
                                {
                                        PartPin src = state.connections.at({id, i});
                                        std::map<int, std::vector<State>>::const_iterator nit = state.netStates.find(src.first);
                                        if (nit != state.netStates.end() && src.second < (int)nit->second.size()) isig = (nit->second[src.second] == STATE_HIGH) ? 1 : 0;
                                }
                                drawPin(pinRect, true, iconn, isig);
                                if (state.inputPinLabels.count(id) && i < (int)state.inputPinLabels.at(id).size())
                                {
                                        const std::string& lbl = state.inputPinLabels.at(id)[i];
                                        if (!lbl.empty())
                                        {
                                                DrawText(lbl.c_str(), (int)(pos.x - size.x/2 + PIN_LABEL_INSET), (int)(pos.y + yOff - PIN_LABEL_FONT_SIZE/2), PIN_LABEL_FONT_SIZE, cText);
                                        }
                                }
                        }
                }
                if (outCount > 0)
                {
                        for (int i = 0; i < outCount; ++i)
                        {
                                Rectangle pinRect = getPinRect(state, id, false, i);
                                float yOff = getPinYOffset(state, id, false, i);
                                int osig = -1;
                                std::map<int, std::vector<State>>::const_iterator nit = state.netStates.find(id);
                                if (nit != state.netStates.end() && i < (int)nit->second.size()) osig = (nit->second[i] == STATE_HIGH) ? 1 : 0;
                                drawPin(pinRect, false, true, osig);
                                if (state.outputPinLabels.count(id) && i < (int)state.outputPinLabels.at(id).size())
                                {
                                        const std::string& lbl = state.outputPinLabels.at(id)[i];
                                        if (!lbl.empty())
                                        {
                                                int lblW = MeasureText(lbl.c_str(), PIN_LABEL_FONT_SIZE);
                                                DrawText(lbl.c_str(), (int)(pos.x + size.x/2 - PIN_LABEL_INSET - lblW), (int)(pos.y + yOff - PIN_LABEL_FONT_SIZE/2), PIN_LABEL_FONT_SIZE, cText);
                                        }
                                }
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
        const char* btnLabels[] = { "Save", "Load", "Clear", "Help", state.darkMode ? "Light" : "Dark", state.isSimulating ? "Pause" : "Play", "Step", "Reset", "+", "-", "Compile" };
        float x = TOOLBAR_PADDING;
        for (int i = 0; i < 11; ++i)
        {
                Rectangle btn = {x, TOOLBAR_PADDING, TOOLBAR_BTN_WIDTH, TOOLBAR_BTN_HEIGHT};
                bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                bool active = (i == 5 && state.isSimulating) || (i == 3 && state.showHelp);
                Color fill = active ? (Color){60, 150, 90, 255} : (hovered ? (Color){95, 100, 110, 255} : (Color){70, 74, 82, 255});
                Color edge = active ? (Color){120, 220, 150, 255} : (hovered ? (Color){110, 160, 230, 255} : (Color){45, 48, 54, 255});
                Color ic = active ? WHITE : (Color){225, 228, 233, 255};
                DrawRectangleRounded(btn, 0.25f, 8, fill);
                DrawRectangleRoundedLinesEx(btn, 0.25f, 8, 1.5f, edge);
                drawToolbarIcon(i, btn, ic, fill, state);
                int tw = MeasureText(btnLabels[i], 8);
                DrawText(btnLabels[i], (int)(btn.x + btn.width / 2 - tw / 2), (int)(btn.y + btn.height - 10), 8, ic);
                x += TOOLBAR_BTN_WIDTH + TOOLBAR_BTN_SPACING;
        }
        bool notDragging = (state.draggingNewPartType == -1 && state.draggingLayoutFile == "" && state.draggingCompiledFile == "");
        if (state.showSideMenu)
        {
                int screenH = GetScreenHeight();
                DrawRectangle(0, TOOLBAR_HEIGHT, state.sidebarWidth, screenH - TOOLBAR_HEIGHT, uiBg);
                DrawLine(state.sidebarWidth, TOOLBAR_HEIGHT, state.sidebarWidth, screenH, uiBorder);
                BeginScissorMode(0, (int)TOOLBAR_HEIGHT, (int)state.sidebarWidth, screenH - (int)TOOLBAR_HEIGHT);
                float yStart = SIDEMENU_Y_START - state.sidebarScroll;
                float y = yStart;
                Vector2 mp = GetMousePosition();
                auto sbHeader = [&](const char* title, int section) -> bool {
                        Rectangle hr = {0, y - 2, state.sidebarWidth, (float)SIDEMENU_HEADER_TEXT_SIZE + 6.0f};
                        bool hov = CheckCollisionPointRec(mp, hr) && mp.y >= TOOLBAR_HEIGHT;
                        float ax = SIDEMENU_PADDING_X + 2, ay = y + SIDEMENU_HEADER_TEXT_SIZE / 2.0f;
                        if (state.sidebarCollapsed[section]) DrawTriangle({ax, ay - 4}, {ax, ay + 4}, {ax + 6, ay}, textC);
                        else DrawTriangle({ax - 1, ay - 2}, {ax + 7, ay - 2}, {ax + 3, ay + 5}, textC);
                        DrawText(title, SIDEMENU_PADDING_X + 14, y, SIDEMENU_HEADER_TEXT_SIZE, hov ? (Color){140, 180, 240, 255} : textC);
                        if (hov && notDragging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) state.sidebarCollapsed[section] = !state.sidebarCollapsed[section];
                        y += SIDEMENU_HEADER_MARGIN;
                        return !state.sidebarCollapsed[section];
                };
                if (sbHeader("Parts Library", 0))
                for (int i = 0; i <= PART_TYPE_DISPLAY; ++i)
                {
                        if (i == PART_TYPE_CUSTOM) continue;

                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, state.sidebarWidth - SIDEMENU_BUTTON_MARGIN*2, SIDEMENU_BUTTON_HEIGHT};
                        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                        DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                        DrawText(partTypeName((PartType)i), btn.x + SIDEMENU_BUTTON_TEXT_OFFSET_X, btn.y + SIDEMENU_BUTTON_TEXT_OFFSET_Y, SIDEMENU_BUTTON_TEXT_SIZE, BLACK);
                        if (hovered && notDragging)
                        {
                                DrawRectangleRoundedLines(btn, 0.2f, 8, BLUE);
                                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) state.draggingNewPartType = i;
                        }
                        y += SIDEMENU_BUTTON_SPACING;
                }
                y += 20;
                std::vector<std::string> files = state.layoutFiles;
                if (sbHeader("Saved Layouts", 1))
                for (size_t i = 0; i < files.size(); ++i)
                {
                        std::string file = files[i];
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, state.sidebarWidth - SIDEMENU_BUTTON_MARGIN*2 - 25, SIDEMENU_BUTTON_HEIGHT};
                        Rectangle delBtn = {btn.x + btn.width + 5, y + 5, SIDEMENU_DELETE_BTN_SIZE, SIDEMENU_DELETE_BTN_SIZE};
                        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                        DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                        std::filesystem::path p(file);
                        drawTextFit(p.stem().string().c_str(), btn.x + 5, btn.y + 5, btn.width - 10, SIDEMENU_LIST_TEXT_SIZE, BLACK);
                        if (hovered && notDragging)
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
                y += 20;
                if (sbHeader("Compiled Modules", 2))
                for (size_t i = 0; i < state.compiledModules.size(); ++i)
                {
                        std::string mod = state.compiledModules[i];
                        Rectangle btn = {SIDEMENU_BUTTON_MARGIN, y, state.sidebarWidth - SIDEMENU_BUTTON_MARGIN*2 - 25, SIDEMENU_BUTTON_HEIGHT};
                        Rectangle delBtn = {btn.x + btn.width + 5, y + 5, SIDEMENU_DELETE_BTN_SIZE, SIDEMENU_DELETE_BTN_SIZE};
                        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
                        DrawRectangleRounded(btn, 0.2f, 8, hovered ? LIGHTGRAY : GRAY);
                        drawTextFit(mod.c_str(), btn.x + 5, btn.y + 5, btn.width - 10, SIDEMENU_LIST_TEXT_SIZE, BLACK);
                        if (hovered && notDragging)
                        {
                                DrawRectangleRoundedLines(btn, 0.2f, 8, BLUE);
                                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && Vector2Distance(GetMouseDelta(), {0,0}) > 1.0f) state.draggingCompiledFile = mod;
                        }
                        DrawRectangleRec(delBtn, RED);
                        DrawText("X", delBtn.x + 5, delBtn.y + 2, 10, WHITE);
                        if (CheckCollisionPointRec(GetMousePosition(), delBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        {
                                state.partToDelete = mod;
                                state.showDeleteConfirm = true;
                        }
                        y += SIDEMENU_LIST_SPACING;
                }
                EndScissorMode();

                float contentH = y - yStart;
                float visibleH = (float)screenH - SIDEMENU_Y_START;
                state.sidebarMaxScroll = (contentH > visibleH) ? (contentH - visibleH + 12.0f) : 0.0f;
                if (state.sidebarScroll > state.sidebarMaxScroll) state.sidebarScroll = state.sidebarMaxScroll;
                if (state.sidebarMaxScroll > 0.0f)
                {
                        float trackH = visibleH;
                        float thumbH = trackH * (visibleH / contentH);
                        if (thumbH < 24.0f) thumbH = 24.0f;
                        float thumbY = SIDEMENU_Y_START + (state.sidebarScroll / state.sidebarMaxScroll) * (trackH - thumbH);
                        DrawRectangleRounded({state.sidebarWidth - 6.0f, thumbY, 4.0f, thumbH}, 0.5f, 6, LIGHTGRAY);
                }

                Rectangle resizeH = {state.sidebarWidth - 3.0f, TOOLBAR_HEIGHT, 6.0f, (float)screenH - TOOLBAR_HEIGHT};
                bool overRH = CheckCollisionPointRec(mp, resizeH);
                if (overRH && notDragging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) state.sidebarResizing = true;
                if (state.sidebarResizing)
                {
                        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                        {
                                state.sidebarWidth = mp.x;
                                if (state.sidebarWidth < 150.0f) state.sidebarWidth = 150.0f;
                                if (state.sidebarWidth > 420.0f) state.sidebarWidth = 420.0f;
                        }
                        else state.sidebarResizing = false;
                }
                if (overRH || state.sidebarResizing) DrawRectangle((int)(state.sidebarWidth - 2), (int)TOOLBAR_HEIGHT, 4, screenH - (int)TOOLBAR_HEIGHT, (Color){110, 160, 230, 200});
        }
        if (state.draggingNewPartType != -1)
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - BASE_PART_WIDTH/2, m.y - BASE_PART_HEIGHT/2, BASE_PART_WIDTH, BASE_PART_HEIGHT, Fade(GRAY, 0.5f));
                DrawText(partTypeName((PartType)state.draggingNewPartType), m.x, m.y, 10, textC);
        }
        else if (state.draggingLayoutFile != "")
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - BASE_PART_WIDTH/2, m.y - BASE_PART_HEIGHT/2, BASE_PART_WIDTH, BASE_PART_HEIGHT, Fade(GRAY, 0.5f));
                DrawText(state.draggingLayoutFile.c_str(), m.x, m.y, 10, textC);
        }
        else if (state.draggingCompiledFile != "")
        {
                Vector2 m = GetMousePosition();
                DrawRectangle(m.x - BASE_PART_WIDTH/2, m.y - BASE_PART_HEIGHT/2, BASE_PART_WIDTH, BASE_PART_HEIGHT, Fade(GRAY, 0.5f));
                DrawText(state.draggingCompiledFile.c_str(), m.x, m.y, 10, textC);
        }
        if (state.showSaveDialog || state.showLoadDialog || state.showRenameDialog || state.showCompileDialog)
        {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawRectangle(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBg);
                DrawRectangleLines(GetScreenWidth()/2 - DIALOG_WIDTH/2, GetScreenHeight()/2 - DIALOG_HEIGHT/2, DIALOG_WIDTH, DIALOG_HEIGHT, uiBorder);
                const char* title = state.showRenameDialog ? "Rename Part:" : (state.showCompileDialog ? "Compile As:" : (state.showSaveDialog ? "Save As:" : "Load Layout:"));
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
                if (!state.showRenameDialog && !state.showCompileDialog)
                {
                        int extWidth = MeasureText(".json", 20);
                        DrawText(".json", box.x + box.width - extWidth - 5, box.y + 5, 20, BLACK);
                }
                if (state.showCompileDialog)
                {
                        float chkY = GetScreenHeight()/2 - DIALOG_HEIGHT/2 + 6;
                        auto drawTick = [&](float x, bool on, const char* label) {
                                Rectangle chk = { x, chkY, 14, 14 };
                                DrawRectangleRec(chk, LIGHTGRAY);
                                DrawRectangleLinesEx(chk, 1, DARKGRAY);
                                if (on)
                                {
                                        DrawLineEx({chk.x + 3, chk.y + 7}, {chk.x + 6, chk.y + 11}, 2, DARKGREEN);
                                        DrawLineEx({chk.x + 6, chk.y + 11}, {chk.x + 12, chk.y + 2}, 2, DARKGREEN);
                                }
                                DrawText(label, chk.x + 20, chk.y + 2, 10, textC);
                        };
                        drawTick(box.x, state.compileDynamic, "Dynamic");
                        drawTick(box.x + 100, state.compileStatic, "Static");
                }
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
                const char* confirmLabel = state.showRenameDialog ? "Rename" : (state.showCompileDialog ? "Compile" : (state.showSaveDialog ? "Save" : "Load"));
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
                PartType type = state.partTypes[state.contextMenu.targetPartID];
                bool isOutput = (type == PART_TYPE_OUTPUT);
                bool canModPins = (type != PART_TYPE_CUSTOM);
                int numRows = isOutput ? 6 : 4;
                DrawRectangle(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*numRows, uiBg);
                DrawRectangleLines(p.x, p.y, CM_WIDTH, CM_ROW_HEIGHT*numRows, uiBorder);
                if (isOutput)
                {
                        DrawText("Edit Label", p.x + CM_TEXT_OFFSET_X, p.y + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Add Input", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Remove Input", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*2 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Add Output", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*3 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Remove Output", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*4 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Delete Part", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*5 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, RED);
                        for (int i = 0; i < numRows; ++i)
                        {
                                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y + CM_ROW_HEIGHT*i, CM_WIDTH, CM_ROW_HEIGHT}))
                                {
                                        DrawRectangleLines(p.x, p.y + CM_ROW_HEIGHT*i, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                                }
                        }
                }
                else
                {
                        DrawText("Edit Label", p.x + CM_TEXT_OFFSET_X, p.y + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, textC);
                        DrawText("Add Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, canModPins ? textC : GRAY);
                        DrawText("Remove Pin", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*2 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, canModPins ? textC : GRAY);
                        DrawText("Delete Part", p.x + CM_TEXT_OFFSET_X, p.y + CM_ROW_HEIGHT*3 + CM_TEXT_OFFSET_Y, CM_TEXT_SIZE, RED);
                        for (int i = 0; i < 4; ++i)
                        {
                                if (i == 1 || i == 2) if (!canModPins) continue;
                                if (CheckCollisionPointRec(GetMousePosition(), {p.x, p.y + CM_ROW_HEIGHT*i, CM_WIDTH, CM_ROW_HEIGHT}))
                                {
                                        DrawRectangleLines(p.x, p.y + CM_ROW_HEIGHT*i, CM_WIDTH, CM_ROW_HEIGHT, BLUE);
                                }
                        }
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
                DrawText("  B: Benchmark current circuit", x, y, HELP_TEXT_SIZE, textC);
                y += HELP_LINE_SPACING;
        }
        drawBenchmark(state);
        auto fmtHz = [](float hz) -> std::string {
                if (hz >= 1000000.0f) return std::format("{:.2f} MHz", hz / 1000000.0f);
                if (hz >= 1000.0f) return std::format("{:.2f} kHz", hz / 1000.0f);
                return std::format("{:.1f} Hz", hz);
        };
        int hxr = GetScreenWidth() - 12;
        std::string tgtStr = std::format("target {}", fmtHz(state.targetHZ));
        DrawText(tgtStr.c_str(), hxr - MeasureText(tgtStr.c_str(), HUD_FONT_SIZE), HUD_Y, HUD_FONT_SIZE, textC);
        Color actualC = textC;
        if (state.isSimulating && state.simSaturated) actualC = (Color){235, 130, 40, 255};
        std::string actStr = state.isSimulating ? std::format("actual {}", fmtHz(state.actualHz)) : std::string("actual --");
        if (state.isSimulating && state.simSaturated) actStr += "  MAX";
        DrawText(actStr.c_str(), hxr - MeasureText(actStr.c_str(), HUD_FONT_SIZE), HUD_Y + 20, HUD_FONT_SIZE, actualC);
        std::string tickStr = std::format("Tick: {}", state.stepCount);
        DrawText(tickStr.c_str(), hxr - MeasureText(tickStr.c_str(), HUD_FONT_SIZE), HUD_Y + 40, HUD_FONT_SIZE, textC);
        std::string vizStr = state.visualizeSignals ? "signal viz: ON (V)" : "signal viz: off (V)";
        Color vizC = state.visualizeSignals ? (Color){90, 200, 130, 255} : (Color){120, 124, 132, 255};
        DrawText(vizStr.c_str(), hxr - MeasureText(vizStr.c_str(), HUD_FONT_SIZE), HUD_Y + 62, HUD_FONT_SIZE, vizC);
        std::string engStr = state.nativeActive ? "engine: native" : "engine: interpreted";
        Color engC = state.nativeActive ? (Color){90, 200, 130, 255} : (Color){160, 165, 172, 255};
        DrawText(engStr.c_str(), hxr - MeasureText(engStr.c_str(), HUD_FONT_SIZE), HUD_Y + 84, HUD_FONT_SIZE, engC);
}

static void drawToolbarIcon(int idx, Rectangle b, Color c, Color bg, const AppState& state)
{
        float cx = b.x + b.width / 2.0f;
        float cy = b.y + 11.0f;
        float s = 6.0f;
        switch (idx)
        {
        case 0:
                DrawLineEx({cx, cy - s}, {cx, cy + s - 2}, 2, c);
                DrawLineEx({cx, cy + s - 2}, {cx - 4, cy + s - 6}, 2, c);
                DrawLineEx({cx, cy + s - 2}, {cx + 4, cy + s - 6}, 2, c);
                DrawLineEx({cx - s, cy + s}, {cx + s, cy + s}, 2, c);
                break;
        case 1:
                DrawLineEx({cx, cy + s - 2}, {cx, cy - s}, 2, c);
                DrawLineEx({cx, cy - s}, {cx - 4, cy - s + 4}, 2, c);
                DrawLineEx({cx, cy - s}, {cx + 4, cy - s + 4}, 2, c);
                DrawLineEx({cx - s, cy + s}, {cx + s, cy + s}, 2, c);
                break;
        case 2:
                DrawRectangleLinesEx({cx - 5, cy - 2, 10, 10}, 1.5f, c);
                DrawLineEx({cx - 7, cy - 2}, {cx + 7, cy - 2}, 2, c);
                DrawLineEx({cx - 2, cy - 5}, {cx + 2, cy - 5}, 2, c);
                break;
        case 3:
                DrawText("?", (int)(cx - 3), (int)(cy - 7), 16, c);
                break;
        case 4:
                if (state.darkMode)
                {
                        DrawCircleLines((int)cx, (int)cy, 4, c);
                        for (int a = 0; a < 8; ++a) { float an = a * 3.14159f / 4.0f; DrawLineEx({cx + cosf(an) * 6, cy + sinf(an) * 6}, {cx + cosf(an) * 8, cy + sinf(an) * 8}, 1.5f, c); }
                }
                else { DrawCircle((int)cx, (int)cy, 6, c); DrawCircle((int)(cx + 3), (int)(cy - 2), 6, bg); }
                break;
        case 5:
                if (state.isSimulating) { DrawRectangle((int)(cx - 4), (int)(cy - 6), 3, 12, c); DrawRectangle((int)(cx + 1), (int)(cy - 6), 3, 12, c); }
                else DrawTriangle({cx - 4, cy - 6}, {cx - 4, cy + 6}, {cx + 6, cy}, c);
                break;
        case 6:
                DrawTriangle({cx - 6, cy - 5}, {cx - 6, cy + 5}, {cx + 2, cy}, c);
                DrawRectangle((int)(cx + 3), (int)(cy - 6), 3, 12, c);
                break;
        case 7:
                DrawRing({cx, cy}, 5, 7, 30, 320, 20, c);
                DrawTriangle({cx + 6, cy - 8}, {cx + 10, cy - 5}, {cx + 4, cy - 3}, c);
                break;
        case 8:
                DrawLineEx({cx - 6, cy}, {cx + 6, cy}, 2.5f, c);
                DrawLineEx({cx, cy - 6}, {cx, cy + 6}, 2.5f, c);
                break;
        case 9:
                DrawLineEx({cx - 6, cy}, {cx + 6, cy}, 2.5f, c);
                break;
        case 10:
                DrawRectangleLinesEx({cx - 5, cy - 5, 10, 10}, 1.5f, c);
                for (int k = -3; k <= 3; k += 3) { DrawLineEx({cx + k, cy - 8}, {cx + k, cy - 5}, 1.5f, c); DrawLineEx({cx + k, cy + 5}, {cx + k, cy + 8}, 1.5f, c); DrawLineEx({cx - 8, cy + k}, {cx - 5, cy + k}, 1.5f, c); DrawLineEx({cx + 5, cy + k}, {cx + 8, cy + k}, 1.5f, c); }
                break;
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
