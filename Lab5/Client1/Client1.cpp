#define _CRT_SECURE_NO_WARNINGS

#include <windows.h> // Головна бібліотека WinAPI
#include <stdio.h>   // Для sprintf_s
#include <string>    // Для strlen, strcat_s

// Ідентифікатори кнопок Клієнта 1
#define ID_CLIENT_SEND  2001 // ID кнопки "Надіслати на сервер"
#define ID_CLIENT_RESET 2002 // ID кнопки "Оновити метрики"

// --- ГЛОБАЛЬНІ ЗМІННІ КЛІЄНТА 1 ---
HINSTANCE hInst;
HWND hwndEdit;

// Ім'я поштової скриньки сервера, куди клієнт буде надсилати повідомлення
LPCSTR ServerName = "\\\\.\\mailslot\\lab5";

char szBuf[1024];     // Буфер для тексту, який відправляється на сервер
char mess[4096] = ""; // Буфер для відображення тексту у вікні клієнта
DWORD cbWritten;      // Кількість байтів, записаних у поштову скриньку функцією WriteFile

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА 1 ---
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
    wcex.lpszClassName = "MailslotClient1Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    // Створюємо вікно Клієнта 1 праворуч зверху (X=620, Y=80, ширина=450, висота=260)
    HWND hWnd = CreateWindowA("MailslotClient1Class", "1 (Client 1 - Варіант 1)", WS_OVERLAPPEDWINDOW,
        620, 80, 450, 260, nullptr, nullptr, hInstance, nullptr);
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
// РЕАЛІЗАЦІЯ ВАРІАНТА 1 (9 - 8 = 1) ДЛЯ КЛІЄНТА 1: ПІДГОТОВКА МЕТРИК СИСТЕМИ
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // ПУНКТ 1 ВАРІАНТА 1: Визначаємо ширину (висоту рядка) полоси меню (SM_CYMENU)
    int menuBarHeight = GetSystemMetrics(SM_CYMENU);

    // ПУНКТ 2 ВАРІАНТА 1: Визначаємо наявність миші в системі (SM_MOUSEPRESENT)
    int mousePresent = GetSystemMetrics(SM_MOUSEPRESENT);
    const char* mouseStatus = (mousePresent != 0) ? "підключена (1)" : "відсутня (0)";

    // ПУНКТ 3 ВАРІАНТА 1: Визначаємо ширину екрана в пікселях через GetDeviceCaps(hdc, HORZRES)
    HDC hdc = GetDC(hWnd);                          // Отримуємо контекст пристрою (екрана)
    int screenWidth = GetDeviceCaps(hdc, HORZRES);  // Горизонтальна роздільна здатність екрана
    ReleaseDC(hWnd, hdc);                           // Звільняємо контекст пристрою

    // Формуємо дефолтне повідомлення Клієнта 1 та виводимо його у текстове поле hwndEdit
    sprintf_s(mess, sizeof(mess),
        "Дані Клієнта #1 (Варіант 1):\r\n"
        " - ширина полоси меню (SM_CYMENU) = %d px\r\n"
        " - наявність миші (SM_MOUSEPRESENT) = %s\r\n"
        " - ширина екрану (HORZRES) = %d px",
        menuBarHeight, mouseStatus, screenWidth);

    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ДОДАНА ФУНКЦІЯ: ВІДКРИТТЯ ПОШТОВОЇ СКРИНЬКИ ТА ЗАПИС ДАНИХ НА СЕРВЕР ---
void SendToMailslot(HWND hWnd)
{
    // 1. Зчитуємо текст, який зараз знаходиться у текстовому полі hwndEdit клієнта
    int len = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    szBuf[len] = '\0';

    // 2. Підключаємося до поштової скриньки сервера як до існуючого файлу функцією CreateFileA
    HANDLE hMailslot = CreateFileA(ServerName,
        GENERIC_WRITE,                        // Відкриваємо для запису даних
        FILE_SHARE_READ | FILE_SHARE_WRITE,   // Дозволяємо спільний доступ іншим клієнтам і серверу
        NULL,
        OPEN_EXISTING,                        // Відкривати лише якщо скринька вже створена сервером!
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    // 3. Якщо сервер закритий (скриньки не існує) — виводимо код помилки (як на рис. 8 методички)
    if (hMailslot == INVALID_HANDLE_VALUE)
    {
        char errText[256];
        sprintf_s(errText, sizeof(errText), "\r\nПомилка поштового серверу: %lu\r\n", GetLastError());
        strcat_s(mess, sizeof(mess), errText);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        return;
    }

    // 4. Записуємо наше повідомлення у поштову скриньку сервера функцією WriteFile
    WriteFile(hMailslot, szBuf, (DWORD)strlen(szBuf) + 1, &cbWritten, NULL);

    // 5. Обов'язково закриваємо дескриптор скриньки після відправки
    CloseHandle(hMailslot);
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОДІЙ ВІКНА КЛІЄНТА 1 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Кнопка відправки вмісту текстового поля на поштовий сервер
        CreateWindowA("BUTTON", "Надіслати у скриньку", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        // Кнопка відновлення дефолтних метрик Варіанта 1 у текстовому полі
        CreateWindowA("BUTTON", "Оновити метрики", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_RESET, hInst, NULL);

        // Текстове поле (в якому також можна редагувати текст вручну, як на рис. 6 методички)
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Одразу при старті формуємо дефолтне повідомлення з метриками Варіанта 1
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