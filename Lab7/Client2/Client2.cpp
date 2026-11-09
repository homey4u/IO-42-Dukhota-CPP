#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// --- ІДЕНТИФІКАТОРИ КНОПОК КЛІЄНТА 2 ---
#define ID_CLIENT_SEND  2001 // Номер кнопки "Надіслати через Pipe"
#define ID_CLIENT_RESET 2002 // Номер кнопки "Оновити метрики"

HINSTANCE hInst;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ ДЛЯ РОБОТИ З КАНАЛОМ У КЛІЄНТІ 2 ---
HANDLE hNamedPipe = INVALID_HANDLE_VALUE;          // Дескриптор відкритого клієнтом каналу Pipe
LPCSTR szPipeName = "\\\\.\\pipe\\LOCAL";          // Системне ім'я каналу сервера

char szBuf[512];        // Буфер для тексту повідомлення, що передається через канал
char mess[4096] = "";   // Буфер для відображення тексту у вікні Клієнта 2
DWORD cbWritten = 0;    // Кількість записаних у канал байтів
DWORD cbRead = 0;       // Кількість прочитаних із каналу байтів

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

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
    wcex.lpszClassName = "NamedPipeClient2Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("NamedPipeClient2Class", "2 (Client 2 - Варіант 2)", WS_OVERLAPPEDWINDOW,
        630, 360, 450, 260, nullptr, nullptr, hInstance, nullptr);
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
// РЕАЛІЗАЦІЯ ВАРІАНТА 2 (9 - 7 = 2) ДЛЯ КЛІЄНТА 2: ЗБІР СИСТЕМНИХ МЕТРИК
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // ПУНКТ 1 ВАРІАНТА 2: Отримуємо ширину панелі завдань / повноекранної робочої області (SM_CXFULLSCREEN)
    int cxFullScreen = GetSystemMetrics(SM_CXFULLSCREEN);

    // ПУНКТ 2 ВАРІАНТА 2: Отримуємо кількість підключених моніторів за константою SM_CMONITORS
    int monitorsCount = GetSystemMetrics(SM_CMONITORS);

    // ПУНКТ 3 ВАРІАНТА 2: Отримуємо горизонтальний DPI екрана через контекст пристрою (GetDeviceCaps(hdc, LOGPIXELSX))
    HDC hdc = GetDC(hWnd);                       // Беремо контекст графічного пристрою (екрана)
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);   // Запитуємо кількість пікселів на логічний дюйм по горизонталі
    ReleaseDC(hWnd, hdc);                        // Звільняємо контекст пристрою назад системі

    // Форматуємо зібрані показники Варіанта 2 (Клієнт 2) у буфер mess
    sprintf_s(mess, sizeof(mess),
        "Дані від Клієнта №2 (Варіант 2):\r\n"
        " - Ширина панелі завдань (SM_CXFULLSCREEN) = %d px\r\n"
        " - Кількість підключених моніторів (SM_CMONITORS) = %d\r\n"
        " - DPI горизонтально (LOGPIXELSX) = %d dpi",
        cxFullScreen, monitorsCount, dpiX);

    // Виводимо сформовані дані у текстове поле Клієнта 2
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// =========================================================================
// ПІДКЛЮЧЕННЯ ДО ІМЕНОВАНОГО КАНАЛУ ТА ОБМІН ДАНИМИ ІЗ СЕРВЕРОМ
// =========================================================================
void SendViaNamedPipe(HWND hWnd)
{
    // 1. Визначаємо довжину тексту у вікні Клієнта 2 та копіюємо його в буфер szBuf
    int len = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    szBuf[len] = '\0';

    // 2. Підключаємося до створеного сервером каналу "\\\\.\\pipe\\LOCAL" функцією CreateFileA
    hNamedPipe = CreateFileA(
        szPipeName,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // Якщо сервер не запущено — виводимо код помилки GetLastError()
    if (hNamedPipe == INVALID_HANDLE_VALUE)
    {
        char errText[256];
        sprintf_s(errText, sizeof(errText), "\r\nПомилка підключення до Pipe (CreateFile Error: %lu)", GetLastError());
        strcat_s(mess, sizeof(mess), errText);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        return;
    }

    // 3. Відправляємо буфер szBuf серверу через канал функцією WriteFile
    if (!WriteFile(hNamedPipe, szBuf, (DWORD)strlen(szBuf) + 1, &cbWritten, NULL))
    {
        CloseHandle(hNamedPipe);
        hNamedPipe = INVALID_HANDLE_VALUE;
        MessageBoxA(hWnd, "Помилка запису в канал (WriteFile)!", "Pipe", MB_OK | MB_ICONERROR);
        return;
    }

    // 4. Читаємо відповідь-підтвердження від сервера через канал функцією ReadFile
    char szReply[256];
    if (ReadFile(hNamedPipe, szReply, sizeof(szReply) - 1, &cbRead, NULL) && cbRead > 0)
    {
        szReply[cbRead] = '\0';
        char replyBlock[512];
        sprintf_s(replyBlock, sizeof(replyBlock), "\r\n\r\n[Відповідь від сервера]: %s", szReply);
        strcpy_s(mess, sizeof(mess), szBuf);
        strcat_s(mess, sizeof(mess), replyBlock);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }

    // 5. Закриваємо дескриптор каналу після завершення обміну
    CloseHandle(hNamedPipe);
    hNamedPipe = INVALID_HANDLE_VALUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку відправки даних через канал Pipe
        CreateWindowA("BUTTON", "Надіслати через Pipe", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        // Створюємо кнопку оновлення дефолтних метрик Варіанта 2
        CreateWindowA("BUTTON", "Оновити метрики", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_RESET, hInst, NULL);

        // Створюємо багаторядкове текстове поле hwndEdit
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Формуємо початкове повідомлення з метриками Клієнта 2 при старті
        PrepareDefaultMetrics(hWnd);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SEND:  SendViaNamedPipe(hWnd); break;
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