#ifndef SULLA_THEME_H
#define SULLA_THEME_H
#include <raylib/raylib.h>

#define THEME_ACCENT_GREEN     ((Color){90, 200, 130, 255})
#define THEME_ACCENT_GREEN_HI  ((Color){120, 225, 150, 255})
#define THEME_ACCENT_GREEN_HI2 ((Color){120, 220, 150, 255})
#define THEME_ACCENT_BLUE      ((Color){110, 160, 230, 255})
#define THEME_ACCENT_BLUE_A    ((Color){110, 160, 230, 200})
#define THEME_HEADER_HOVER     ((Color){140, 180, 240, 255})
#define THEME_WARN             ((Color){235, 130, 40, 255})
#define THEME_TEXT_DIM         ((Color){160, 165, 172, 255})
#define THEME_TEXT_DIM2        ((Color){120, 124, 132, 255})

#define THEME_BTN_FILL         ((Color){70, 74, 82, 255})
#define THEME_BTN_HOVER        ((Color){95, 100, 110, 255})
#define THEME_BTN_ACTIVE       ((Color){60, 150, 90, 255})
#define THEME_BTN_EDGE         ((Color){45, 48, 54, 255})
#define THEME_BTN_ICON         ((Color){225, 228, 233, 255})

#define THEME_PIN_INPUT        ((Color){95, 150, 230, 255})
#define THEME_PIN_LOW          ((Color){78, 90, 104, 255})
#define THEME_PIN_EMPTY        ((Color){40, 44, 50, 255})
#define THEME_PIN_EMPTY_EDGE   ((Color){115, 122, 132, 255})
#define THEME_PIN_BORDER       ((Color){20, 22, 26, 160})

#define THEME_GATE_ORANGE      ((Color){230, 150,  70, 255})
#define THEME_GATE_MAGENTA     ((Color){210, 120, 190, 255})
#define THEME_GATE_PURPLE      ((Color){170, 120, 210, 255})
#define THEME_GATE_BLUE        ((Color){120, 180, 230, 255})
#define THEME_GATE_GRAY        ((Color){160, 160, 170, 255})


#include "theme_colors.h"

struct Theme
{
        Color bg, grid, partBg, partBorder, text, uiBg, uiBorder;
};

inline Theme buildTheme(bool dark)
{
        Theme t;
        if (dark)
        {
                t.bg = COLOR_BG_DARK; t.grid = COLOR_GRID_DARK; t.partBg = COLOR_PART_BG_DARK;
                t.partBorder = COLOR_PART_BORDER_DARK; t.text = COLOR_TEXT_DARK; t.uiBg = COLOR_UI_BG_DARK;
                t.uiBorder = COLOR_UI_BORDER_DARK;
        }
        else
        {
                t.bg = COLOR_BG_LIGHT; t.grid = COLOR_GRID_LIGHT; t.partBg = COLOR_PART_BG_LIGHT;
                t.partBorder = COLOR_PART_BORDER_LIGHT; t.text = COLOR_TEXT_LIGHT; t.uiBg = COLOR_UI_BG_LIGHT;
                t.uiBorder = COLOR_UI_BORDER_LIGHT;
        }
        return t;
}

#endif
