#define _CRT_SECURE_NO_WARNINGS

#include <windows.h> // Головна бібліотека WinAPI
#include <stdio.h>   // Для sprintf_s
#include <string>    // Для strlen, strcat_s

#define ID_CLIENT_SEND  2001 // ID кнопки "Надіслати у скриньку"
#define ID_CLIENT_RESET 2002 // ID кнопки "Оновити метрики"

// --- ГЛОБАЛЬНІ ЗМІННІ КЛІЄНТА 2 ---
HINSTANCE hInst;
HWND hwndEdit;

LPCSTR ServerName = "\\\\.\\mailslot\\lab5";

char szBuf[1024];
char mess[4096] = "";
DWORD cbWritten;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА 2 ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance;

    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = "MailslotClient2Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    // Створюємо вікно Клієнта 2 прямо під Клієнтом 1 (X=620, Y=360, ширина=450, висота=260)
    HWND hWnd = CreateWindowA("MailslotClient2Class", "2 (Client 2 - Варіант 1)", WS_OVERLAPPEDWINDOW,
        620, 360, 450, 260, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// =========================================================================
// РЕАЛІЗАЦІЯ ВАРІАНТА 1 (9 - 8 = 1) ДЛЯ КЛІЄНТА 2: ПІДГОТОВКА МЕТРИК СИСТЕМИ
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // ПУНКТ 1 ВАРІАНТА 1: Ширина та висота іконки додатку (SM_CXICON, SM_CYICON)
    int iconW = GetSystemMetrics(SM_CXICON);
    int iconH = GetSystemMetrics(SM_CYICON);

    // ПУНКТ 2 ВАРІАНТА 1: Ширина вертикальної полоси прокрутки (SM_CXVSCROLL)
    int scrollW = GetSystemMetrics(SM_CXVSCROLL);

    // ПУНКТ 3 ВАРІАНТА 1: Глибина кольору екрана (GetDeviceCaps(hdc, BITSPIXEL))
    HDC hdc = GetDC(hWnd);                           // Отримуємо контекст пристрою (екрана)
    int colorDepth = GetDeviceCaps(hdc, BITSPIXEL);  // Кількість бітів на піксель
    ReleaseDC(hWnd, hdc);                            // Звільняємо контекст пристрою

    // Формуємо дефолтне повідомлення Клієнта 2 та відображаємо його у полі hwndEdit
    sprintf_s(mess, sizeof(mess),
        "Дані Клієнта #2 (Варіант 1):\r\n"
        " - ширина іконки (SM_CXICON) = %d px\r\n"
        " - висота іконки (SM_CYICON) = %d px\r\n"
        " - ширина верт. прокрутки (SM_CXVSCROLL) = %d px\r\n"
        " - глибина кольору (BITSPIXEL) = %d біт/піксель",
        iconW, iconH, scrollW, colorDepth);

    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ВІДКРИТТЯ ПОШТОВОЇ СКРИНЬКИ ТА ВІДПРАВКА ДАНИХ НА СЕРВЕР ---
void SendToMailslot(HWND hWnd)
{
    int len = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    szBuf[len] = '\0';

    // Відкриваємо поштову скриньку сервера через CreateFileA
    HANDLE hMailslot = CreateFileA(ServerName,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hMailslot == INVALID_HANDLE_VALUE)
    {
        char errText[256];
        sprintf_s(errText, sizeof(errText), "\r\nПомилка поштового серверу: %lu\r\n", GetLastError());
        strcat_s(mess, sizeof(mess), errText);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        return;
    }

    // Записуємо дані у поштову скриньку функцією WriteFile та закриваємо дескриптор
    WriteFile(hMailslot, szBuf, (DWORD)strlen(szBuf) + 1, &cbWritten, NULL);
    CloseHandle(hMailslot);
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОДІЙ ВІКНА КЛІЄНТА 2 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateWindowA("BUTTON", "Надіслати у скриньку", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        CreateWindowA("BUTTON", "Оновити метрики", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_RESET, hInst, NULL);

        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        PrepareDefaultMetrics(hWnd);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SEND:  SendToMailslot(hWnd); break;
        case ID_CLIENT_RESET: PrepareDefaultMetrics(hWnd); break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}