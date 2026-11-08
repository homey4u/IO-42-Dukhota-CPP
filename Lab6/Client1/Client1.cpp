#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// --- ІДЕНТИФІКАТОРИ КНОПОК КЛІЄНТА 1 ---
#define ID_CLIENT_SEND  2001 // Номер кнопки "Відправити у FileMapping"
#define ID_CLIENT_RESET 2002 // Номер кнопки "Оновити метрики"

HINSTANCE hInst;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ FILE MAPPING КЛІЄНТА 1 ---
HANDLE hMapFile = NULL; // Дескриптор відкритого об'єкта проекції файлу
LPVOID pBuf = NULL;     // Вказівник на спроектовану спільну пам'ять
char szBuf[512];        // Текстовий буфер на 512 байтів для підготовки повідомлення
DWORD cbWritten;        // Кількість символів, зчитаних із текстового поля клієнта

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
    wcex.lpszClassName = "FileMapClient1Class";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("FileMapClient1Class", "1 (Client 1 - Варіант 7)", WS_OVERLAPPEDWINDOW,
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
// РЕАЛІЗАЦІЯ ВАРІАНТА 7 ДЛЯ КЛІЄНТА 1: ЗБІР СИСТЕМНИХ ПАРАМЕТРІВ
// =========================================================================
void PrepareDefaultMetrics(HWND hWnd)
{
    // Запитуємо режим завантаження системи за константою SM_CLEANBOOT (0 - звичайний, 1 - безпечний)
    int cleanBoot = GetSystemMetrics(SM_CLEANBOOT);
    // Отримуємо час роботи системи з моменту увімкнення в мілісекундах і ділимо на 1000 для переведення в секунди
    ULONGLONG uptimeSec = GetTickCount64() / 1000;

    // Запитуємо системну метрику за константою SM_CXMIN з таблиці Варіанта 7
    int cxMin = GetSystemMetrics(SM_CXMIN);

    // Отримуємо дескриптор контексту пристрою (екрана) для вікна hWnd
    HDC hdc = GetDC(hWnd);
    // Запитуємо у відеодрайвера горизонтальну ширину екрана в пікселях (константа HORZRES)
    int screenWidth = GetDeviceCaps(hdc, HORZRES);
    // Звільняємо контекст пристрою назад системі
    ReleaseDC(hWnd, hdc);

    // Форматуємо зібрані показники Варіанта 7 у текстовий буфер szBuf
    sprintf_s(szBuf, sizeof(szBuf),
        "Дані від Клієнта №1 (Варіант 7):\r\n"
        " - Час системи / SM_CLEANBOOT = %d (Uptime: %llu с)\r\n"
        " - Стан антивірусу (SM_CXMIN) = %d\r\n"
        " - Ширина екрану (HORZRES) = %d px",
        cleanBoot, uptimeSec, cxMin, screenWidth);

    // Виводимо сформований текст у поле редагування hwndEdit клієнта
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)szBuf);
}

// =========================================================================
// ВІДКРИТТЯ FILE MAPPING ТА КОПІЮВАННЯ ДАНИХ У СПІЛЬНУ ПАМ'ЯТЬ
// =========================================================================
void SendToFileMapping(HWND hWnd)
{
    // Дізнаємося довжину тексту, який зараз знаходиться у полі hwndEdit клієнта
    cbWritten = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    // Обмежуємо довжину розміром буфера szBuf (511 символів + нуль-термінатор)
    if (cbWritten >= sizeof(szBuf)) cbWritten = sizeof(szBuf) - 1;
    // Копіюємо текст із вікна hwndEdit у масив szBuf
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(cbWritten + 1), (LPARAM)szBuf);
    // Ставимо завершальний нуль-символ у кінці рядка
    szBuf[cbWritten] = '\0';

    // Відкриваємо створений сервером об'єкт відображення за ім'ям "myFileMapping":
    // FILE_MAP_ALL_ACCESS - повний доступ на читання і запис;
    // FALSE - не успадковувати дескриптор дочірніми процесами.
    hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "myFileMapping");
    // Якщо сервер не запущено — об'єкт не знайдено (NULL), виводимо повідомлення про помилку
    if (hMapFile == NULL || hMapFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(hWnd, "Не можливо відкрити відображений у пам'яті об'єкт", "Map", MB_OK);
        return;
    }

    // Відкриваємо іменований м'ютекс сервера "myFileMappingMutex" для синхронізації запису (*)
    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, "myFileMappingMutex");
    // Якщо м'ютекс знайдено — захоплюємо його (чекаємо, поки пам'ять звільниться)
    if (hMutex != NULL) WaitForSingleObject(hMutex, INFINITE);

    // Проектуємо 512 байтів спільної пам'яті у адресний простір клієнта і отримуємо вказівник pBuf
    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 512);
    // Перевіряємо успішність отримання вказівника pBuf
    if (pBuf == NULL)
    {
        if (hMutex != NULL) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        CloseHandle(hMapFile);
        MessageBoxA(hWnd, "Подання проектованого файлу неможливе", "Map", MB_OK);
        return;
    }

    // Копіюємо вміст нашого масиву szBuf (512 байтів) напряму в спільну оперативну пам'ять за адресою pBuf
    CopyMemory((PVOID)pBuf, szBuf, sizeof(szBuf));

    // Відключаємо вказівник pBuf від адресного простору клієнта
    UnmapViewOfFile(pBuf);
    // Закриваємо дескриптор об'єкта відображення hMapFile
    CloseHandle(hMapFile);

    // Звільняємо м'ютекс після завершення запису в спільну пам'ять (*)
    if (hMutex != NULL)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    // Відкриваємо іменовану подію сервера "myFileMappingEvent" (*)
    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, "myFileMappingEvent");
    if (hEvent != NULL)
    {
        // Переводимо подію в сигнальний стан (SetEvent) — сервер миттєво дізнається про нові дані!
        SetEvent(hEvent);
        // Закриваємо дескриптор події
        CloseHandle(hEvent);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку для відправки тексту у спільну пам'ять FileMapping
        CreateWindowA("BUTTON", "Відправити у FileMapping", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SEND, hInst, NULL);
        // Створюємо кнопку для повторної генерації дефолтних метрик Варіанта 7
        CreateWindowA("BUTTON", "Оновити метрики", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_RESET, hInst, NULL);

        // Створюємо багаторядкове текстове поле hwndEdit
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Заповнюємо поле метриками Варіанта 7 одразу при запуску клієнта
        PrepareDefaultMetrics(hWnd);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SEND:  SendToFileMapping(hWnd); break;     // Натиснули "Відправити у FileMapping"
        case ID_CLIENT_RESET: PrepareDefaultMetrics(hWnd); break; // Натиснули "Оновити метрики"
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