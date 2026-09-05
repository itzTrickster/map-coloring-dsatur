#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <wchar.h>
#include <limits.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "bmp.h"
#include "dsatur.h"
#include "graph.h"

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_MAP = 1
} ScreenState;

ScreenState screenState = SCREEN_MENU;
char selectedMap[MAX_PATH];
unsigned int mapTexture = 0;
unsigned int aboutButtonTexture = 0;
unsigned int helpButtonTexture = 0;

int lastProgressPercent = -1;
FILE* logFile = NULL;
GLFWwindow* mainWindow = NULL;

unsigned int loadButtonTexture = 0;
unsigned int colorButtonTexture = 0;
unsigned int closeButtonTexture = 0;

unsigned int progressBarTexture = 0;
int progressBarVisible = 0;
int progressBarDone = 0;

const wchar_t LOAD_BUTTON_TEXT[] = { 0x0412,0x044B,0x0431,0x0440,0x0430,0x0442,0x044C,0x0020,0x043A,0x0430,0x0440,0x0442,0x0443,0 };
const wchar_t COLOR_BUTTON_TEXT[] = { 0x0420,0x0430,0x0441,0x043A,0x0440,0x0430,0x0441,0x0438,0x0442,0x044C,0x0021,0 };
const wchar_t ABOUT_BUTTON_TEXT[] = { 0x041E,0x0020,0x043F,0x0440,0x043E,0x0433,0x0440,0x0430,0x043C,0x043C,0x0435,0 };
const wchar_t HELP_BUTTON_TEXT[] = { 0x0421,0x043F,0x0440,0x0430,0x0432,0x043A,0x0430,0 };

Image* loadedMapImage = NULL;
unsigned char* mapRgbaBuffer = NULL;
Graph* mapGraph = NULL;
int* mapColors = NULL;
int** regionPixels = NULL;
int* regionPixelCounts = NULL;
int regionLookupCount = 0;

int animationRegionIndex = 0;
int animationInProgress = 0;
double nextAnimationTick = 0.0;
int leftMouseWasPressed = 0;

float mapUvOffsetX = 0.0f;
float mapUvOffsetY = 0.0f;
float mapUvZoom = 1.0f;

const float MAP_MIN_ZOOM = 1.0f;
const float MAP_MAX_ZOOM = 8.0f;
const float MAP_ZOOM_STEP = 1.1f;

double totalStartTime = 0.0;
double algorithmTime = 0.0;

const double ANIMATION_STEP_DELAY = 0.02;
const int ANIMATION_REGIONS_PER_STEP = 1;

unsigned int createTextButtonTexture(const wchar_t* text, int width, int height, int fontSize);

unsigned int createProgressBarTexture(int percent, int done)
{
    const int w = 1024;
    const int h = 80;

    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return 0;

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!dib || !pixels)
    {
        if (dib) DeleteObject(dib);
        DeleteDC(dc);
        return 0;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(dc, dib);

    RECT rect = { 0, 0, w, h };
    HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 240));
    FillRect(dc, &rect, bgBrush);
    DeleteObject(bgBrush);

    int fillWidth = (int)((w - 4) * percent / 100.0f);
    RECT fillRect = { 2, 2, 2 + fillWidth, h - 2 };
    COLORREF fillColor = done ? RGB(40, 180, 40) : RGB(0, 120, 215);
    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    FillRect(dc, &fillRect, fillBrush);
    DeleteObject(fillBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(100, 100, 100));
    HPEN oldPen = (HPEN)SelectObject(dc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 1, 1, w - 1, h - 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(borderPen);

    wchar_t label[32];
    if (done)
    {
        label[0] = 0x0413; label[1] = 0x043E; label[2] = 0x0442; label[3] = 0x043E;
        label[4] = 0x0432; label[5] = 0x043E; label[6] = '!'; label[7] = 0;
    }
    else
    {
        wsprintfW(label, L"%d%%", percent);
    }

    HFONT font = CreateFontW(-36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_SWISS, L"Segoe UI");
    if (!font)
    {
        font = CreateFontW(-36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FF_SWISS, L"Arial");
    }

    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
    DeleteObject(font);

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

    SelectObject(dc, oldBitmap);
    DeleteObject(dib);
    DeleteDC(dc);
    return texture;
}

void updateProgressBarTexture(int percent, int done)
{
    if (progressBarTexture)
    {
        glDeleteTextures(1, &progressBarTexture);
        progressBarTexture = 0;
    }
    progressBarTexture = createProgressBarTexture(percent, done);
}

void openLog()
{
    if (!logFile)
    {
        logFile = fopen("map_coloring.log", "a");
    }
}

void closeLog()
{
    if (logFile)
    {
        fclose(logFile);
        logFile = NULL;
    }
}

void logMessage(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    openLog();
    if (!logFile) return;

    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    va_end(args);
    fflush(logFile);
}

void updateWindowTitleByProgress(int percent)
{
    if (!mainWindow) return;

    if (percent < 0)
    {
        glfwSetWindowTitle(mainWindow, "map-coloring");
        return;
    }

    char title[128];
    snprintf(title, sizeof(title), "map-coloring - progress: %d%%", percent);
    glfwSetWindowTitle(mainWindow, title);
}

void reportProgressPercent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent == lastProgressPercent) return;

    lastProgressPercent = percent;
    logMessage("Progress: %d%%\n", percent);
    updateWindowTitleByProgress(percent);
    updateProgressBarTexture(percent, progressBarDone);
}

float clampf_value(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void clampMapView()
{
    mapUvZoom = clampf_value(mapUvZoom, MAP_MIN_ZOOM, MAP_MAX_ZOOM);

    float maxOffset = 0.5f - (0.5f / mapUvZoom);
    mapUvOffsetX = clampf_value(mapUvOffsetX, -maxOffset, maxOffset);
    mapUvOffsetY = clampf_value(mapUvOffsetY, -maxOffset, maxOffset);
}

void resetMapView()
{
    mapUvZoom = 1.0f;
    mapUvOffsetX = 0.0f;
    mapUvOffsetY = 0.0f;
}

void zoomMapView(float factor)
{
    mapUvZoom *= factor;
    clampMapView();
}

void panMapView(float dx, float dy)
{
    mapUvOffsetX += dx;
    mapUvOffsetY += dy;
    clampMapView();
}

void logGraphBuildFailureDetails()
{
    GraphBuildStatus status = get_last_graph_build_status();
    double matrixMb = (double)get_last_graph_adj_matrix_bytes() / (1024.0 * 1024.0);
    double limitMb = (double)get_graph_adj_memory_limit_bytes() / (1024.0 * 1024.0);

    if (status == GRAPH_BUILD_ERR_ADJ_TOO_LARGE)
    {
        logMessage("Graph build failed: adjacency matrix is too large (regions=%d, estimated=%.2f MB, limit=%.2f MB)\n",
            get_last_graph_region_count(), matrixMb, limitMb);
        return;
    }

    if (status == GRAPH_BUILD_ERR_MEMORY)
    {
        logMessage("Graph build failed: not enough memory (regions=%d, estimated adjacency=%.2f MB)\n",
            get_last_graph_region_count(), matrixMb);
        return;
    }

    if (status == GRAPH_BUILD_ERR_INVALID_IMAGE)
    {
        logMessage("Graph build failed: invalid input image\n");
        return;
    }

    logMessage("Graph build failed\n");
}

void logGraphBuildStats(Graph* graph)
{
    if (!graph) return;
    double matrixMb = (double)get_last_graph_adj_matrix_bytes() / (1024.0 * 1024.0);
    logMessage("Graph built: regions=%d, adjacency matrix=%.2f MB\n", graph->region_count, matrixMb);
}

void saveAppState()
{
    FILE* file = fopen("app_state.txt", "w");
    if (!file) return;

    fprintf(file, "last_map=%s\n", selectedMap[0] ? selectedMap : "");
    fclose(file);
}

void loadAppState()
{
    FILE* file = fopen("app_state.txt", "r");
    if (!file)
    {
        file = fopen("app_state.txt", "w");
        if (file) fclose(file);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "last_map=", 9) == 0)
        {
            const char* value = line + 9;
            size_t len = strcspn(value, "\r\n");
            if (len > 0 && len < sizeof(selectedMap))
            {
                memcpy(selectedMap, value, len);
                selectedMap[len] = '\0';
            }
        }
    }

    fclose(file);
}

void showAboutDialog()
{
    const wchar_t aboutText[] = {
        'M','a','p',' ','C','o','l','o','r','i','n','g','\n','\n',
        0x0410,0x0432,0x0442,0x043E,0x0440,0x044B,':',' ',
        0x041D,0x0438,0x0444,0x0430,0x043D,0x0442,0x044C,0x0435,0x0432,' ',
        0x0414,'.',0x0412,'.',',',' ',
        0x041A,0x043E,0x043F,0x0435,0x0438,0x043D,' ',
        0x0420,'.',0x0410,'.','\n',
        0x0413,0x0440,0x0443,0x043F,0x043F,0x0430,':',' ',
        '5','1','3','1','0','0','1','/','5','0','0','0','1','\n',
        0x0413,0x043E,0x0434,':',' ','2','0','2','6','\n',
        0x0412,0x0423,0x0417,':',' ',
        0x0421,0x041F,0x0431,0x041F,0x0423,'\n',
        0x041A,0x0430,0x0444,0x0435,0x0434,0x0440,0x0430,':',' ',
        0x0418,0x043D,0x0444,0x043E,0x0440,0x043C,0x0430,0x0446,0x0438,0x043E,0x043D,0x043D,0x0430,0x044F,' ',
        0x0431,0x0435,0x0437,0x043E,0x043F,0x0430,0x0441,0x043D,0x043E,0x0441,0x0442,0x044C,
        0
    };

    const wchar_t title[] = {
        0x041E,0x0020,0x043F,0x0440,0x043E,0x0433,0x0440,0x0430,0x043C,0x043C,0x0435,0
    };

    MessageBoxW(NULL, aboutText, title, MB_OK | MB_ICONINFORMATION);
}

void showHelpDialog()
{
    const wchar_t helpText[] = {
        'M','a','p',' ','C','o','l','o','r','i','n','g','\n','\n',
        0x0421,0x043F,0x0440,0x0430,0x0432,0x043A,0x0430,':','\n',
        '-',' ',
        0x0412,0x044B,0x0431,0x0440,0x0430,0x0442,0x044C,' ',
        0x043A,0x0430,0x0440,0x0442,0x0443,':',' ',
        0x043A,0x043D,0x043E,0x043F,0x043A,0x0430,' ',
        0x0432,' ',
        0x043C,0x0435,0x043D,0x044E,'\n',
        '-',' ',
        0x0420,0x0430,0x0441,0x043A,0x0440,0x0430,0x0441,0x0438,0x0442,0x044C,':',' ',
        0x043A,0x043D,0x043E,0x043F,0x043A,0x0430,' ',
        0x0432,' ',
        0x043E,0x043A,0x043D,0x0435,' ',
        0x043A,0x0430,0x0440,0x0442,0x044B,'\n',
        '-',' ',
        0x041D,0x0430,0x0437,0x0430,0x0434,' ',
        0x0432,' ',
        0x043C,0x0435,0x043D,0x044E,':',' ',
        0x043A,0x0440,0x0430,0x0441,0x043D,0x044B,0x0439,' ',
        0x043A,0x0440,0x0435,0x0441,0x0442,'\n','\n',
        0x0420,0x0435,0x0437,0x0443,0x043B,0x044C,0x0442,0x0430,0x0442,':','\n',
        '-',' ',
        0x0420,0x0430,0x0441,0x043A,0x0440,0x0430,0x0448,0x0435,0x043D,0x043D,0x0430,0x044F,' ',
        0x043A,0x0430,0x0440,0x0442,0x0430,' ',
        0x0441,0x043E,0x0445,0x0440,0x0430,0x043D,0x044F,0x0435,0x0442,0x0441,0x044F,0x0020,
        0x0430,0x0432,0x0442,0x043E,0x043C,0x0430,0x0442,0x0438,0x0447,0x0435,0x0441,0x043A,0x0438,
        0
    };

    const wchar_t title[] = { 0x0421,0x043F,0x0440,0x0430,0x0432,0x043A,0x0430,0 };

    MessageBoxW(NULL, helpText, title, MB_OK | MB_ICONINFORMATION);
}
