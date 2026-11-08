#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// Визначаємо макрос SM_CYBUTTON із таблиці завдання як стандартну константу висоти кнопок меню SM_CYMENUSIZE
#ifndef SM_CYBUTTON
#define SM_CYBUTTON SM_CYMENUSIZE
#endif

// --- ІДЕНТИФІКАТОРИ КНОПОК КЛІЄНТА 2 ---
#define ID_CLIENT_SEND  2001 // Номер кнопки "Відправити у FileMapping"
#define ID_CLIENT_RESET 2002 // Номер кнопки "Оновити метрики"

HINSTANCE hInst;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ FILE MAPPING КЛІЄНТА 2 ---
HANDLE hMapFile = NULL; // Дескриптор відкритого об'єкта проекції файлу
LPVOID pBuf = NULL;     // Вказівник на спроектовану ділянку пам'яті
char szBuf[512];        // Буфер на 512 байтів для передачі даних
DWORD cbWritten;        // Довжина зчитаного тексту з поля клієнта

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
    wcex.lpszClassName = "FileMapClient2Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("FileMapClient2Class", "2 (Client 2 - Варіант 7)", WS_OVERLAPPEDWINDOW,
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
// РЕАЛІЗАЦІЯ ВАРІАНТА 7 ДЛЯ КЛІЄНТА 2: ЗБІР СИСТЕМНИХ ПАРАМЕТРІВ
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // Запитуємо у системи висоту кнопок у рядку меню в пікселях (SM_CYBUTTON / SM_CYMENUSIZE)
    int cyButton = GetSystemMetrics(SM_CYBUTTON);

    // Запитуємо мінімальну ширину трекінгу вікна / кнопки «Пуск» за константою SM_CXMINTRACK
    int cxMinTrack = GetSystemMetrics(SM_CXMINTRACK);

    // Отримуємо контекст пристрою (екрана) для нашого вікна
    HDC hdc = GetDC(hWnd);
    // Запитуємо у відеодрайвера кількість рядків екрана по вертикалі в пікселях (константа VERTRES)
    int vertRes = GetDeviceCaps(hdc, VERTRES);
    // Звільняємо контекст пристрою
    ReleaseDC(hWnd, hdc);

    // Форматуємо зібрані показники Варіанта 7 (Клієнт 2) у буфер szBuf
    sprintf_s(szBuf, sizeof(szBuf),
        "Дані від Клієнта №2 (Варіант 7):\r\n"
        " - Висота кнопок у меню (SM_CYBUTTON) = %d px\r\n"
        " - Ширина кнопки «Пуск» (SM_CXMINTRACK) = %d px\r\n"
        " - Кількість рядків екрану (VERTRES) = %d px",
        cyButton, cxMinTrack, vertRes);

    // Відображаємо підготовлений текст у вікні Клієнта 2
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)szBuf);
}

// =========================================================================
// ВІДКРИТТЯ FILE MAPPING ТА КОПІЮВАННЯ ДАНИХ У СПІЛЬНУ ПАМ'ЯТЬ
// =========================================================================
void SendToFileMapping(HWND hWnd)
{
    // Визначаємо кількість символів у текстовому полі hwndEdit
    cbWritten = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    // Перевіряємо, щоб довжина не перевищувала розмір буфера szBuf
    if (cbWritten >= sizeof(szBuf)) cbWritten = sizeof(szBuf) - 1;
    // Зчитуємо текст із поля hwndEdit у буфер szBuf
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(cbWritten + 1), (LPARAM)szBuf);
    // Додаємо нуль-термінатор у кінець зчитаного рядка
    szBuf[cbWritten] = '\0';

    // Відкриваємо створений сервером об'єкт проекції файлу з ім'ям "myFileMapping"
    hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "myFileMapping");
    // Якщо сервер не запущено — виводимо діалогове вікно помилки
    if (hMapFile == NULL || hMapFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(hWnd, "Не можливо відкрити відображений у пам'яті об'єкт", "Map", MB_OK);
        return;
    }

    // Відкриваємо спільний м'ютекс "myFileMappingMutex" та блокуємо його перед записом (*)
    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, "myFileMappingMutex");
    if (hMutex != NULL) WaitForSingleObject(hMutex, INFINITE);

    // Відображаємо 512 байтів об'єкта hMapFile у пам'ять Клієнта 2 та отримуємо вказівник pBuf
    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 512);
    if (pBuf == NULL)
    {
        if (hMutex != NULL) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        CloseHandle(hMapFile);
        MessageBoxA(hWnd, "Подання проектованого файлу неможливе", "Map", MB_OK);
        return;
    }

    // Копіюємо байти повідомлення з szBuf у спільну пам'ять за адресою pBuf
    CopyMemory((PVOID)pBuf, szBuf, sizeof(szBuf));

    // Відключаємо вказівник pBuf та закриваємо дескриптор проекції hMapFile
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    // Звільняємо м'ютекс після запису (*)
    if (hMutex != NULL)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    // Відкриваємо подію "myFileMappingEvent" та подаємо сигнал SetEvent серверу про нові дані (*)
    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "myFileMappingEvent");
    if (hEvent != NULL)
    {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку відправки даних у проекцію файлу
        CreateWindowA("BUTTON", "Відправити у FileMapping", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        // Створюємо кнопку оновлення дефолтних метрик Варіанта 7
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
        case ID_CLIENT_SEND:  SendToFileMapping(hWnd); break;
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