#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>

using namespace Gdiplus;

// Constants
const int BALL_SIZE = 128;
const float GRAVITY = 0.6f;
const float FRICTION = 0.99f;
const float BOUNCE = 0.75f;
const int UPDATE_TIMER = 1;

struct Ball {
    float x, y;
    float vx, vy;
    Image* img;
    bool isDragging;
};

std::vector<Ball> balls;
ULONG_PTR gdiplusToken;
POINT lastMouse;
int draggingIdx = -1;

void UpdatePhysics() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    for (auto& ball : balls) {
        if (ball.isDragging) continue;

        ball.vy += GRAVITY;
        ball.vx *= FRICTION;
        ball.vy *= FRICTION;

        ball.x += ball.vx;
        ball.y += ball.vy;

        // Boundary checks
        if (ball.x < 0) { ball.x = 0; ball.vx = -ball.vx * BOUNCE; }
        if (ball.x > screenWidth - BALL_SIZE) { ball.x = (float)screenWidth - BALL_SIZE; ball.vx = -ball.vx * BOUNCE; }
        if (ball.y < 0) { ball.y = 0; ball.vy = -ball.vy * BOUNCE; }
        if (ball.y > screenHeight - BALL_SIZE) { ball.y = (float)screenHeight - BALL_SIZE; ball.vy = -ball.vy * BOUNCE; }
        
        // Horizontal friction when on ground
        if (ball.y >= screenHeight - BALL_SIZE - 1) {
            ball.vx *= 0.95f;
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            SetTimer(hwnd, UPDATE_TIMER, 16, NULL);
            return 0;

        case WM_TIMER:
            if (wParam == UPDATE_TIMER) {
                UpdatePhysics();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Double buffering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            SelectObject(memDC, memBitmap);

            Graphics graphics(memDC);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.Clear(Color(0, 0, 0, 0)); // Transparent

            for (const auto& ball : balls) {
                graphics.DrawImage(ball.img, (REAL)ball.x, (REAL)ball.y, (REAL)BALL_SIZE, (REAL)BALL_SIZE);
            }

            // Update Layered Window
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptSrc = { 0, 0 };
            SIZE sizeWnd = { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            POINT ptDest = { 0, 0 };
            
            UpdateLayeredWindow(hwnd, hdc, &ptDest, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            for (int i = (int)balls.size() - 1; i >= 0; --i) {
                if (mx >= balls[i].x && mx <= balls[i].x + BALL_SIZE &&
                    my >= balls[i].y && my <= balls[i].y + BALL_SIZE) {
                    draggingIdx = i;
                    balls[i].isDragging = true;
                    lastMouse = {mx, my};
                    SetCapture(hwnd);
                    // Move to front
                    Ball b = balls[i];
                    balls.erase(balls.begin() + i);
                    balls.push_back(b);
                    draggingIdx = (int)balls.size() - 1;
                    break;
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (draggingIdx != -1) {
                int mx = LOWORD(lParam);
                int my = HIWORD(lParam);
                balls[draggingIdx].vx = (float)(mx - lastMouse.x) * 1.5f;
                balls[draggingIdx].vy = (float)(my - lastMouse.y) * 1.5f;
                balls[draggingIdx].x += (float)(mx - lastMouse.x);
                balls[draggingIdx].y += (float)(my - lastMouse.y);
                lastMouse = {mx, my};
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (draggingIdx != -1) {
                balls[draggingIdx].isDragging = false;
                draggingIdx = -1;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_RBUTTONDOWN: // Exit on right click
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PolandBallApp";
    wc.hCursor = LoadCursor(NULL, IDC_HAND);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        L"PolandBallApp", L"PolandBall Desktop",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    // Remove WS_EX_TRANSPARENT so we can catch mouse events
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

    // Load balls
    balls.push_back({100, 100, 10, 5, new Image(L"balls/poland/normal.png"), false});
    balls.push_back({400, 200, -8, 2, new Image(L"balls/japan/normal.png"), false});
    balls.push_back({700, 150, 5, -5, new Image(L"balls/usa/normal.png"), false});
    balls.push_back({200, 300, 12, 0, new Image(L"balls/germany/normal.png"), false});
    balls.push_back({500, 400, -10, -10, new Image(L"balls/russia/normal.png"), false});

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    for (auto& ball : balls) delete ball.img;
    GdiplusShutdown(gdiplusToken);
    return 0;
}
