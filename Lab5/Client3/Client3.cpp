#define _CRT_SECURE_NO_WARNINGS

#include <windows.h> // Головна бібліотека WinAPI
#include <stdio.h>   // Для sprintf_s
#include <stdlib.h>  // Для функції atof() (перетворення тексту в дробове число)
#include <string>    // Для strlen, strcat_s

#define ID_CLIENT_CALC 2001 // ID кнопки "Обчислити площу"
#define ID_CLIENT_SEND 2002 // ID кнопки "Надіслати у скриньку"

// --- ГЛОБАЛЬНІ ЗМІННІ КЛІЄНТА 3 ---
HINSTANCE hInst;
HWND hwndInputA; // Поле вводу сторони A прямокутника
HWND hwndInputB; // Поле вводу сторони B прямокутника
HWND hwndEdit;   // Поле відображення результатів обчислень

LPCSTR ServerName = "\\\\.\\mailslot\\lab5";

char szBuf[1024];
char mess[4096] = "";
DWORD cbWritten;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА 3 ---
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
    wcex.lpszClassName = "MailslotClient3Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    // Створюємо вікно Клієнта 3 прямо під вікном Сервера (X=150, Y=500, ширина=450, висота=290)
    HWND hWnd = CreateWindowA("MailslotClient3Class", "3 (Client 3 * - Площа прямокутника)", WS_OVERLAPPEDWINDOW,
        150, 500, 450, 290, nullptr, nullptr, hInstance, nullptr);
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
// РЕАЛІЗАЦІЯ ЗАВДАННЯ «З ЗІРОЧКОЮ»: МАТЕМАТИЧНІ ОБЧИСЛЕННЯ ЗА ДОВІЛЬНИМИ ДАНИМИ
// =========================================================================
void CalculateRectangle()
{
    char strA[64] = "", strB[64] = "";

    // 1. Зчитуємо довільні числа (сторону A і сторону B), введені користувачем у поля вводу
    GetWindowTextA(hwndInputA, strA, sizeof(strA));
    GetWindowTextA(hwndInputB, strB, sizeof(strB));

    // 2. Перетворюємо текстові рядки у числа з плаваючою крапкою за допомогою функції atof()
    double sideA = atof(strA);
    double sideB = atof(strB);

    // 3. Виконуємо математичні обчислення: площа (S = A * B) та периметр (P = 2 * (A + B))
    double area = sideA * sideB;
    double perimeter = 2.0 * (sideA + sideB);

    // 4. Формуємо текстовий звіт із початковими даними та результатами обчислень
    sprintf_s(mess, sizeof(mess),
        "Дані Клієнта #3 (* Математичні обчислення):\r\n"
        " - Сторона A = %.2f\r\n"
        " - Сторона B = %.2f\r\n"
        " - Площа прямокутника (S = A * B) = %.2f\r\n"
        " - Периметр прямокутника (P = 2*(A+B)) = %.2f",
        sideA, sideB, area, perimeter);

    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ВІДКРИТТЯ ПОШТОВОЇ СКРИНЬКИ ТА ЗАПИС ОБЧИСЛЕНЬ НА СЕРВЕР ---
void SendToMailslot(HWND hWnd)
{
    // Спочатку оновлюємо обчислення за поточними введеними числами
    CalculateRectangle();

    int len = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    szBuf[len] = '\0';

    // Підключаємося до поштової скриньки сервера через CreateFileA
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

    // Записуємо результати математичних обчислень у скриньку сервера
    WriteFile(hMailslot, szBuf, (DWORD)strlen(szBuf) + 1, &cbWritten, NULL);
    CloseHandle(hMailslot);
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОДІЙ ВІКНА КЛІЄНТА 3 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Кнопки обчислення та відправки результатів
        CreateWindowA("BUTTON", "Обчислити площу", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 28, hWnd, (HMENU)ID_CLIENT_CALC, hInst, NULL);
        CreateWindowA("BUTTON", "Надіслати у скриньку", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 28, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);

        // Підписи та поля вводу для сторін прямокутника A і B
        CreateWindowA("STATIC", "Сторона A:", WS_CHILD | WS_VISIBLE,
            5, 42, 80, 24, hWnd, NULL, hInst, NULL);
        hwndInputA = CreateWindowA("EDIT", "15.5", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            90, 40, 115, 24, hWnd, NULL, hInst, NULL);

        CreateWindowA("STATIC", "Сторона B:", WS_CHILD | WS_VISIBLE,
            215, 42, 80, 24, hWnd, NULL, hInst, NULL);
        hwndInputB = CreateWindowA("EDIT", "8.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            300, 40, 120, 24, hWnd, NULL, hInst, NULL);

        // Поле виведення результатів обчислень
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 70, 415, 175, hWnd, NULL, hInst, NULL);

        // Виконуємо первинний розрахунок при старті вікна
        CalculateRectangle();
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 70, LOWORD(lParam) - 10, HIWORD(lParam) - 75, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_CALC: CalculateRectangle(); break;
        case ID_CLIENT_SEND: SendToMailslot(hWnd); break;
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