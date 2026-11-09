#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// У таблиці завдання вказано символьне ім'я константи SM_KEYBOARDPRESENT для перевірки стану клавіатури.
// Визначаємо її, щоб код гарантовано компілювався у будь-якій версії Windows SDK:
#ifndef SM_KEYBOARDPRESENT
#define SM_KEYBOARDPRESENT 22
#endif

// --- ІДЕНТИФІКАТОРИ КНОПОК КЛІЄНТА 1 ---
#define ID_CLIENT_SEND  2001 // Номер кнопки "Надіслати через Pipe"
#define ID_CLIENT_RESET 2002 // Номер кнопки "Оновити метрики"

HINSTANCE hInst;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ ДЛЯ РОБОТИ З КАНАЛОМ У КЛІЄНТІ 1 ---
// Дескриптор відкритого клієнтом іменованого каналу
HANDLE hNamedPipe = INVALID_HANDLE_VALUE;
// Ім'я іменованого каналу сервера, до якого підключається клієнт (як на стор. 5 методички)
LPCSTR szPipeName = "\\\\.\\pipe\\LOCAL";

// Буфер для тексту, який клієнт відправляє на сервер
char szBuf[512];
// Буфер для збереження історії повідомлень на екрані клієнта
char mess[4096] = "";
// Кількість переданих у канал байтів
DWORD cbWritten = 0;
// Кількість прийнятих із каналу байтів (відповідь сервера)
DWORD cbRead = 0;

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
    wcex.lpszClassName = "NamedPipeClient1Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("NamedPipeClient1Class", "1 (Client 1 - Варіант 2)", WS_OVERLAPPEDWINDOW,
        630, 80, 450, 260, nullptr, nullptr, hInstance, nullptr);
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
// РЕАЛІЗАЦІЯ ВАРІАНТА 2 (9 - 7 = 2) ДЛЯ КЛІЄНТА 1: ЗБІР СИСТЕМНИХ МЕТРИК
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // ПУНКТ 1 ВАРІАНТА 2: Отримуємо висоту рядка заголовка вікна в пікселях за константою SM_CYCAPTION
    int captionHeight = GetSystemMetrics(SM_CYCAPTION);

    // ПУНКТ 2 ВАРІАНТА 2: Перевіряємо стан (наявність) клавіатури в системі (SM_KEYBOARDPRESENT та GetKeyboardType)
    int kbMetric = GetSystemMetrics(SM_KEYBOARDPRESENT);
    int kbType = GetKeyboardType(0); // Повертає тип підключеної клавіатури (> 0, якщо клавіатура активна)
    const char* kbStatus = (kbMetric != 0 || kbType > 0) ? "підключена (Активна)" : "відсутня";

    // ПУНКТ 3 ВАРІАНТА 2: Отримуємо висоту екрана в пікселях через контекст пристрою (GetDeviceCaps(hdc, VERTRES))
    HDC hdc = GetDC(hWnd);                         // Беремо контекст графічного пристрою (екрана)
    int screenHeight = GetDeviceCaps(hdc, VERTRES); // Запитуємо у драйвера вертикальну висоту екрана в пікселях
    ReleaseDC(hWnd, hdc);                          // Обов'язково звільняємо контекст пристрою

    // Форматуємо зібрані показники Варіанта 2 у буфер mess
    sprintf_s(mess, sizeof(mess),
        "Дані від Клієнта №1 (Варіант 2):\r\n"
        " - Висота заголовка вікна (SM_CYCAPTION) = %d px\r\n"
        " - Стан клавіатури (SM_KEYBOARDPRESENT) = %s\r\n"
        " - Висота екрану (VERTRES) = %d px",
        captionHeight, kbStatus, screenHeight);

    // Виводимо сформовані дані у текстове поле клієнта hwndEdit
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// =========================================================================
// ПІДКЛЮЧЕННЯ ДО ІМЕНОВАНОГО КАНАЛУ ТА ОБМІН ДАНИМИ ІЗ СЕРВЕРОМ
// =========================================================================
void SendViaNamedPipe(HWND hWnd)
{
    // 1. Дізнаємося довжину тексту, який зараз знаходиться у полі hwndEdit клієнта
    int len = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    // Обмежуємо довжину розміром буфера szBuf (511 символів + нуль-термінатор)
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;
    // Зчитуємо текст із поля hwndEdit у буфер szBuf
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    // Додаємо нуль-термінатор у кінець рядка
    szBuf[len] = '\0';

    // 2. Підключаємося до іменованого каналу сервера "\\\\.\\pipe\\LOCAL" функцією CreateFileA (стор. 5 методички):
    // GENERIC_READ | GENERIC_WRITE - відкриваємо канал одночасно і для читання, і для запису (двосторонній обмін);
    // 0 - монопольний доступ на час сеансу;
    // NULL - стандартні атрибути безпеки;
    // OPEN_EXISTING - підключатися лише в тому разі, якщо канал вже створений сервером;
    // FILE_ATTRIBUTE_NORMAL - стандартні атрибути файлу.
    hNamedPipe = CreateFileA(
        szPipeName,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // Якщо сервер не запущено (канал не знайдено) — виводимо код помилки GetLastError()
    if (hNamedPipe == INVALID_HANDLE_VALUE)
    {
        char errText[256];
        sprintf_s(errText, sizeof(errText), "\r\nПомилка підключення до Pipe (CreateFile Error: %lu)", GetLastError());
        strcat_s(mess, sizeof(mess), errText);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        return;
    }

    // 3. Відправляємо зчитаний буфер szBuf серверу через канал функцією WriteFile
    if (!WriteFile(hNamedPipe, szBuf, (DWORD)strlen(szBuf) + 1, &cbWritten, NULL))
    {
        CloseHandle(hNamedPipe);
        hNamedPipe = INVALID_HANDLE_VALUE;
        MessageBoxA(hWnd, "Помилка запису в канал (WriteFile)!", "Pipe", MB_OK | MB_ICONERROR);
        return;
    }

    // 4. Отримуємо зворотну відповідь-підтвердження від сервера через той самий канал функцією ReadFile (стор. 6 методички)
    char szReply[256];
    if (ReadFile(hNamedPipe, szReply, sizeof(szReply) - 1, &cbRead, NULL) && cbRead > 0)
    {
        szReply[cbRead] = '\0'; // Завершуємо отриману відповідь нуль-термінатором
        char replyBlock[512];
        sprintf_s(replyBlock, sizeof(replyBlock), "\r\n\r\n[Відповідь від сервера]: %s", szReply);
        // Дописуємо відповідь сервера у текстове поле клієнта
        strcpy_s(mess, sizeof(mess), szBuf);
        strcat_s(mess, sizeof(mess), replyBlock);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }

    // 5. Закриваємо дескриптор каналу після завершення сеансу обміну функцією CloseHandle
    CloseHandle(hNamedPipe);
    hNamedPipe = INVALID_HANDLE_VALUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку для відправки даних через іменований канал Pipe
        CreateWindowA("BUTTON", "Надіслати через Pipe", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        // Створюємо кнопку для оновлення дефолтних метрик Варіанта 2
        CreateWindowA("BUTTON", "Оновити метрики", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_RESET, hInst, NULL);

        // Створюємо багаторядкове текстове поле hwndEdit
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Формуємо початкове повідомлення з метриками Варіанта 2 одразу при відкритті вікна
        PrepareDefaultMetrics(hWnd);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SEND:  SendViaNamedPipe(hWnd); break;      // Клік по "Надіслати через Pipe"
        case ID_CLIENT_RESET: PrepareDefaultMetrics(hWnd); break; // Клік по "Оновити метрики"
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