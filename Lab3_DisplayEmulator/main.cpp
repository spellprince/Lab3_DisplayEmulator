#define WIN32_LEAN_AND_MEAN
#define DISPLAY_SERVER

#include "DisplayProtocol.cpp"
#include "UDPServer.h"

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

const int DISPLAY_WIDTH = 320;
const int DISPLAY_HEIGHT = 240;
const int UDP_PORT = 12345;

HWND windowHandle;
HBITMAP displayBitmap;
HDC memoryDC;

std::mutex displayMutex;

uint16_t currentColor = 0xFFFF;
uint8_t currentOrientation = 0;

void updateDisplay()
{
    InvalidateRect(windowHandle, NULL, TRUE);
}

COLORREF convertColor(uint16_t color)
{
    int r = ((color >> 11) & 31) * 255 / 31;
    int g = ((color >> 5) & 63) * 255 / 63;
    int b = (color & 31) * 255 / 31;

    return RGB(r, g, b);
}

void clearDisplay(uint16_t color)
{
    HBRUSH brush = CreateSolidBrush(convertColor(color));

    RECT rect = { 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT };

    FillRect(memoryDC, &rect, brush);

    DeleteObject(brush);
}

void drawPixel(int x, int y, uint16_t color)
{
    SetPixel(memoryDC, x, y, convertColor(color));
}

void drawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    MoveToEx(memoryDC, x0, y0, NULL);
    LineTo(memoryDC, x1, y1);

    SelectObject(memoryDC, oldPen);
    DeleteObject(pen);
}

void drawRectangle(int x, int y, int w, int h, uint16_t color, bool filled)
{
    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    if (filled)
        Rectangle(memoryDC, x, y, x + w, y + h);
    else
        Rectangle(memoryDC, x, y, x + w, y + h);

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

void drawEllipse(int x, int y, int rx, int ry, uint16_t color, bool filled)
{
    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    Ellipse(memoryDC, x - rx, y - ry, x + rx, y + ry);

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

void drawCircle(int x, int y, int radius, uint16_t color, bool filled)
{
    drawEllipse(x, y, radius, radius, color, filled);
}

void drawRoundedRectangle(int x, int y, int w, int h,
    int radius, uint16_t color, bool filled)
{
    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    RoundRect(memoryDC, x, y, x + w, y + h,
        radius * 2, radius * 2);

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

void drawTextCommand(const Command& cmd)
{
    int x = cmd.parameters[0];
    int y = cmd.parameters[1];

    SetTextColor(memoryDC, convertColor(cmd.color));
    SetBkMode(memoryDC, TRANSPARENT);

    int fontSize = 16 + cmd.font * 2;

    HFONT font = CreateFontA(
        fontSize, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        "Arial"
    );

    HFONT oldFont = (HFONT)SelectObject(memoryDC, font);

    TextOutA(memoryDC, x, y,
        cmd.text.c_str(), (int)cmd.text.size());

    SelectObject(memoryDC, oldFont);
    DeleteObject(font);
}

void drawImageCommand(const Command& cmd)
{
    int x = cmd.parameters[0];
    int y = cmd.parameters[1];
    int w = cmd.parameters[2];
    int h = cmd.parameters[3];

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            int index = (row * w + col) * 2;

            if (index + 1 >= (int)cmd.imageData.size())
                return;

            uint16_t color = cmd.imageData[index]
                | (cmd.imageData[index + 1] << 8);

            drawPixel(x + col, y + row, color);
        }
    }
}

void executeCommand(const Command& cmd)
{
    std::lock_guard<std::mutex> lock(displayMutex);

    switch (cmd.type)
    {
    case CLEAR_DISPLAY:
        clearDisplay(cmd.color);
        break;

    case DRAW_PIXEL:
        drawPixel(cmd.parameters[0], cmd.parameters[1], cmd.color);
        break;

    case DRAW_LINE:
        drawLine(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.color
        );
        break;

    case DRAW_RECTANGLE:
        drawRectangle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.color, false
        );
        break;

    case FILL_RECTANGLE:
        drawRectangle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.color, true
        );
        break;

    case DRAW_ELLIPSE:
        drawEllipse(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.color, false
        );
        break;

    case FILL_ELLIPSE:
        drawEllipse(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.color, true
        );
        break;

    case DRAW_CIRCLE:
        drawCircle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.color, false
        );
        break;

    case FILL_CIRCLE:
        drawCircle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.color, true
        );
        break;

    case DRAW_ROUNDED_RECTANGLE:
        drawRoundedRectangle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.parameters[4], cmd.color, false
        );
        break;

    case FILL_ROUNDED_RECTANGLE:
        drawRoundedRectangle(
            cmd.parameters[0], cmd.parameters[1],
            cmd.parameters[2], cmd.parameters[3],
            cmd.parameters[4], cmd.color, true
        );
        break;

    case DRAW_TEXT:
        drawTextCommand(cmd);
        break;

    case DRAW_IMAGE:
        drawImageCommand(cmd);
        break;

    case SET_ORIENTATION:
        currentOrientation = cmd.orientation;
        break;

    case GET_WIDTH:
        std::cout << "Display width: " << DISPLAY_WIDTH << std::endl;
        break;

    case GET_HEIGHT:
        std::cout << "Display height: " << DISPLAY_HEIGHT << std::endl;
        break;
    }

    updateDisplay();
}

LRESULT CALLBACK WindowProcedure(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        lock_guard<mutex> lock(displayMutex);

        BitBlt(
            hdc, 0, 0,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            memoryDC, 0, 0, SRCCOPY
        );

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSA wc = {};

    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DisplayEmulator";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    windowHandle = CreateWindowA(
        "DisplayEmulator",
        "Display Module Emulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DISPLAY_WIDTH + 16, DISPLAY_HEIGHT + 39,
        NULL, NULL, hInstance, NULL
    );

    memoryDC = CreateCompatibleDC(NULL);

    HBITMAP bitmap = CreateCompatibleBitmap(
        GetDC(windowHandle),
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );

    displayBitmap = bitmap;

    SelectObject(memoryDC, displayBitmap);

    clearDisplay(0x0000);

    ShowWindow(windowHandle, SW_SHOW);

    UDPServer server;

    if (!server.start())
    {
        MessageBoxA(
            windowHandle,
            "Failed to start UDP server",
            "Error",
            MB_ICONERROR
        );
    }

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    server.stop();

    DeleteObject(displayBitmap);
    DeleteDC(memoryDC);

    return 0;
}