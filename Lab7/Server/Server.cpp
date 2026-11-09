#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// --- ІДЕНТИФІКАТОРИ КНОПОК СЕРВЕРА ---
#define ID_PIPE_START 1001 // Унікальний номер кнопки "Start (Створити Pipe)"
#define ID_PIPE_STOP  1002 // Унікальний номер кнопки "Stop (Закрити Pipe)"

HINSTANCE hInst;
HWND hWindow;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ ДЛЯ РОБОТИ З ІМЕНОВАНИМ КАНАЛОМ (NAMED PIPE) ---
// Дескриптор іменованого каналу (INVALID_HANDLE_VALUE означає, що канал ще не створений)
HANDLE hNamedPipe = INVALID_HANDLE_VALUE;
// Дескриптор фонового потоку сервера, який приймає підключення клієнтів без зависання вікна
HANDLE hPipeThread = NULL;
// Логічний прапорець: true — сервер працює і чекає клієнтів, false — сервер зупиняється
bool bServerRunning = false;

// Системне ім'я іменованого каналу на локальному комп'ютері (як на стор. 3 методички)
LPCSTR lpszPipeName = "\\\\.\\pipe\\LOCAL";

// Тимчасовий буфер на 512 байтів для прийому одного повідомлення з каналу
char szBuf[512];
// Накопичувальний текстовий буфер для зберігання всієї історії повідомлень на екрані сервера
char mess[4096] = "";
// Кількість байтів даних, фактично прочитаних із каналу функцією ReadFile
DWORD cbRead = 0;
// Кількість байтів даних, фактично записаних у канал функцією WriteFile
DWORD cbWritten = 0;

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
    wcex.lpszClassName = "NamedPipeServerClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("NamedPipeServerClass", "s (Сервер - Named Pipe)", WS_OVERLAPPEDWINDOW,
        150, 100, 460, 420, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    hWindow = hWnd;
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
// ФОНОВИЙ ПОТІК СЕРВЕРА: ОЧІКУВАННЯ КЛІЄНТІВ ТА ДВОСТОРОННІЙ ОБМІН ДАНИМИ
// =========================================================================
DWORD WINAPI PipeServerThread(LPVOID lpParam)
{
    // Цикл працює безперервно, доки сервер активний (дозволяє приймати Клієнта 1 і Клієнта 2 багато разів)
    while (bServerRunning && hNamedPipe != INVALID_HANDLE_VALUE)
    {
        // Чекаємо на підключення клієнта до нашого каналу функцією ConnectNamedPipe (аналог accept у сокетах)
        BOOL fConnected = ConnectNamedPipe(hNamedPipe, NULL);

        // Якщо клієнт встиг підключитися ще до виклику ConnectNamedPipe, система повертає код ERROR_PIPE_CONNECTED,
        // що також вважається успішним встановленням з'єднання (див. стор. 4 методички)
        if (!fConnected && GetLastError() == ERROR_PIPE_CONNECTED)
        {
            fConnected = TRUE;
        }

        // Якщо сервер було зупинено кнопкою Stop під час очікування — виходимо з циклу
        if (!bServerRunning)
            break;

        // Якщо клієнт успішно підключився до іменованого каналу:
        if (fConnected)
        {
            // 1. Читаємо надіслане клієнтом повідомлення з каналу функцією ReadFile у буфер szBuf
            if (ReadFile(hNamedPipe, szBuf, sizeof(szBuf) - 1, &cbRead, NULL) && cbRead > 0)
            {
                // Ставимо завершальний нуль-символ у кінці прочитаних байтів
                szBuf[cbRead] = '\0';

                // Формуємо рядок з отриманими даними для виведення у вікно сервера
                char block[1024];
                sprintf_s(block, sizeof(block), "\r\nОтримано через канал Pipe:\r\n%s\r\n", szBuf);
                // Доклеюємо новий блок тексту до загальної історії повідомлень сервера mess
                strcat_s(mess, sizeof(mess), block);
                // Оновлюємо вміст багаторядкового текстового поля hwndEdit на екрані сервера
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);

                // 2. Формуємо текст відповіді від сервера клієнту (оскільки канал двонаправлений — DUPLEX)
                const char* replyMsg = "Сервер Pipe: дані успішно прийнято та оброблено!";
                // Записуємо відповідь у той самий канал функцією WriteFile, щоб клієнт її прочитав
                WriteFile(hNamedPipe, replyMsg, (DWORD)strlen(replyMsg) + 1, &cbWritten, NULL);

                // 3. Примусово скидаємо буфер каналу функцією FlushFileBuffers, щоб клієнт гарантовано отримав усі байти
                FlushFileBuffers(hNamedPipe);
            }

            // 4. Від'єднуємо поточного клієнта від каналу функцією DisconnectNamedPipe,
            // щоб канал звільнився і зміг прийняти наступне підключення (від іншого або того ж клієнта)
            DisconnectNamedPipe(hNamedPipe);
        }
    }
    // Завершуємо фоновий потік сервера
    return 0;
}

// =========================================================================
// ФУНКЦІЯ СТВОРЕННЯ ІМЕНОВАНОГО КАНАЛУ ТА ЗАПУСКУ СЕРВЕРА
// =========================================================================
void StartPipeServer(HWND hWnd)
{
    // Перевіряємо, чи канал вже не створений раніше
    if (hNamedPipe != INVALID_HANDLE_VALUE)
    {
        MessageBoxA(hWnd, "Сервер іменованого каналу вже запущено!", "Pipe", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Створюємо іменований канал функцією CreateNamedPipeA (параметри зі стор. 3 методички):
    // lpszPipeName - ім'я каналу "\\\\.\\pipe\\LOCAL";
    // PIPE_ACCESS_DUPLEX - двонаправлений режим (і сервер, і клієнт можуть читати та писати);
    // PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT - передача даних окремими повідомленнями у синхронному режимі;
    // PIPE_UNLIMITED_INSTANCES - необмежена кількість екземплярів каналу (до 255);
    // 512, 512 - розміри вихідного та вхідного буферів каналу в байтах;
    // 5000 - тайм-аут за замовчуванням у мілісекундах (5 секунд);
    // NULL - стандартні атрибути безпеки Windows.
    hNamedPipe = CreateNamedPipeA(
        lpszPipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        512, 512, 5000, NULL);

    // Якщо створити канал не вдалося — виводимо код системної помилки GetLastError()
    if (hNamedPipe == INVALID_HANDLE_VALUE)
    {
        char errBuf[256];
        sprintf_s(errBuf, sizeof(errBuf), "CreateNamedPipe: Помилка %lu\r\n", GetLastError());
        MessageBoxA(hWnd, errBuf, "Помилка", MB_OK | MB_ICONERROR);
        return;
    }

    // Встановлюємо прапорець активної роботи сервера в true
    bServerRunning = true;
    // Запускаємо фоновий потік PipeServerThread для очікування клієнтів без блокування вікна
    hPipeThread = CreateThread(NULL, 0, PipeServerThread, NULL, 0, NULL);

    // Виводимо повідомлення про запуск сервера у текстове поле
    strcpy_s(mess, sizeof(mess), "Канал \\\\.\\pipe\\LOCAL створено.\r\nСервер очікує на підключення клієнтів...\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// =========================================================================
// ФУНКЦІЯ ЗУПИНКИ СЕРВЕРА ТА ЗАКРИТТЯ ІМЕНОВАНОГО КАНАЛУ
// =========================================================================
void StopPipeServer(HWND hWnd)
{
    // Якщо канал вже закритий — нічого не робимо
    if (hNamedPipe == INVALID_HANDLE_VALUE)
        return;

    // Скидаємо прапорець роботи сервера, щоб фоновий потік вийшов із циклу while
    bServerRunning = false;

    // Якщо фоновий потік зараз чекає клієнта всередині ConnectNamedPipe —
    // функція CancelSynchronousIo миттєво перериває це очікування без зависання!
    if (hPipeThread != NULL)
    {
        CancelSynchronousIo(hPipeThread);
        // Чекаємо завершення фонового потоку
        WaitForSingleObject(hPipeThread, 1000);
        // Закриваємо дескриптор потоку
        CloseHandle(hPipeThread);
        hPipeThread = NULL;
    }

    // Закриваємо дескриптор іменованого каналу функцією CloseHandle
    CloseHandle(hNamedPipe);
    // Скидаємо змінну дескриптора у початковий неактивний стан
    hNamedPipe = INVALID_HANDLE_VALUE;

    // Дописуємо у вікно повідомлення про закриття каналу
    strcat_s(mess, sizeof(mess), "\r\nКанал Pipe закрито. Сервер зупинено.\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку створення та запуску каналу Pipe
        CreateWindowA("BUTTON", "Start (Створити Pipe)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_PIPE_START, hInst, NULL);
        // Створюємо кнопку закриття каналу Pipe
        CreateWindowA("BUTTON", "Stop (Закрити Pipe)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_PIPE_STOP, hInst, NULL);

        // Записуємо стартовий текст у буфер mess
        strcpy_s(mess, sizeof(mess), "Лабораторна робота - Канали обміну даними (Named Pipes).\r\n");
        // Створюємо багаторядкове поле hwndEdit для відображення логів сервера
        hwndEdit = CreateWindowA("EDIT", mess,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 425, 330, hWnd, NULL, hInst, NULL);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_PIPE_START: StartPipeServer(hWnd); break; // Натиснули "Start (Створити Pipe)"
        case ID_PIPE_STOP:  StopPipeServer(hWnd); break;  // Натиснули "Stop (Закрити Pipe)"
        }
        break;

    case WM_DESTROY:
        // При закритті вікна коректно зупиняємо сервер і закриваємо канал
        StopPipeServer(hWnd);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}