#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>

// --- ІДЕНТИФІКАТОРИ КНОПОК ТА ПОВІДОМЛЕНЬ ---
#define ID_FILEMAPPING_START 1001 // Унікальний номер кнопки "Start (Створити)"
#define ID_FILEMAPPING_READ  1002 // Унікальний номер кнопки "Read (Прочитати)"
#define ID_FILEMAPPING_STOP  1003 // Унікальний номер кнопки "Stop (Зупинити)"
#define WM_AUTO_READ (WM_USER + 1) // Користувацьке повідомлення вікна для авточитання пам'яті (*)

HINSTANCE hInst;
HWND hWindow;
HWND hwndEdit;

// --- ГЛОБАЛЬНІ ЗМІННІ FILE MAPPING ТА СИНХРОНІЗАЦІЇ ---
HANDLE hMapFile = NULL;        // Дескриптор об'єкта відображення файлу (NULL = ще не створено)
LPVOID pBuf = NULL;            // Вказівник на початок спільної ділянки оперативної пам'яті
char mess[4096] = "";          // Накопичувальний текстовий буфер для виведення всієї історії на екран
char* m_mess = mess;           // Вказівник на початок буфера mess (як у методичці)
char szBuf[512];               // Допоміжний буфер розміром 512 байтів

HANDLE hMapMutex = NULL;       // Дескриптор міжпроцесного м'ютекса для захисту спільної пам'яті (*)
HANDLE hDataReadyEvent = NULL; // Дескриптор міжпроцесної події "Клієнт записав нові дані" (*)
HANDLE hStopWatchEvent = NULL; // Дескриптор локальної події "Зупинити фоновий потік сервера" (*)
HANDLE hWatcherThread = NULL;  // Дескриптор фонового потоку, що чекає на сигнали від клієнтів (*)

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
    wcex.lpszClassName = "FileMapServerClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    HWND hWnd = CreateWindowA("FileMapServerClass", "s (Сервер - FileMapping)", WS_OVERLAPPEDWINDOW,
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
// ФОНОВИЙ ПОТІК ДЛЯ АВТОМАТИЧНОГО ВИЯВЛЕННЯ ЗМІН (ЗАВДАННЯ «З ЗІРОЧКОЮ»)
// =========================================================================
DWORD WINAPI WatcherThreadProc(LPVOID lpParam)
{
    // Формуємо масив із двох подій: [0] - зупинка сервера, [1] - поява нових даних від клієнта
    HANDLE waitHandles[2] = { hStopWatchEvent, hDataReadyEvent };

    // Запускаємо безперервний цикл стеження за подіями
    while (true)
    {
        // Призупиняємо потік до спрацьовування будь-якої з 2-х подій (FALSE = будь-яка одна, INFINITE = без ліміту часу)
        DWORD res = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        // Якщо спрацювала подія з індексом 0 (hStopWatchEvent) — виходимо з циклу і завершуємо потік
        if (res == WAIT_OBJECT_0)
            break;

        // Якщо спрацювала подія з індексом 1 (hDataReadyEvent від клієнта) —
        if (res == WAIT_OBJECT_0 + 1)
        {
            // Асинхронно надсилаємо головному вікну сервера hWindow повідомлення WM_AUTO_READ для читання пам'яті
            PostMessageA(hWindow, WM_AUTO_READ, 0, 0);
        }
    }
    // Коректно завершуємо виконання потоку з кодом 0
    return 0;
}

// =========================================================================
// ФУНКЦІЯ ЧИТАННЯ ДАНИХ ІЗ ПРОЕКЦІЇ ФАЙЛУ В ПАМ'ЯТЬ
// =========================================================================
void ReadFileMappingData(HWND hWnd, bool isManual)
{
    // Перевіряємо, чи створено об'єкт проекції файлу (чи натиснуто кнопку Start)
    if (hMapFile == NULL)
    {
        // Якщо функцію викликано вручну кнопкою Read — показуємо попередження
        if (isManual)
            MessageBoxA(hWnd, "Сервер FileMapping ще не запущено!", "Map", MB_OK | MB_ICONWARNING);
        return; // Виходимо з функції, оскільки об'єкт ще не існує
    }

    // Блокуємо спільний м'ютекс, щоб клієнти не змінювали пам'ять під час читання сервером
    WaitForSingleObject(hMapMutex, INFINITE);

    // Відображаємо об'єкт hMapFile у віртуальну пам'ять сервера (0, 0 = зміщення 0; 0 = весь розмір об'єкта)
    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    // Перевіряємо, чи вдалося отримати дійсний вказівник на спільну пам'ять
    if (pBuf == NULL)
    {
        ReleaseMutex(hMapMutex); // У разі помилки обов'язково звільняємо м'ютекс
        MessageBoxA(hWnd, "Подання проектованого файлу неможливе", "Map", MB_OK | MB_ICONERROR);
        return; // Перериваємо виконання функції
    }

    // Створюємо тимчасовий буфер для форматування прочитаного рядка
    char block[1024];
    // Копіюємо текст прямо з оперативної пам'яті за адресою (LPCSTR)pBuf у буфер block
    sprintf_s(block, sizeof(block), "\r\n%s\r\n", (LPCSTR)pBuf);
    // Дописуємо прочитаний текст у кінець загального логу сервера mess
    strcat_s(mess, sizeof(mess), block);
    // Виводимо оновлений лог mess у багаторядкове текстове поле hwndEdit на екрані
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)m_mess);

    // Відключаємо вказівник pBuf від адресного простору сервера після завершення читання
    UnmapViewOfFile(pBuf);
    // Скидаємо вказівник у NULL для уникнення звернень до звільненої пам'яті
    pBuf = NULL;

    // Звільняємо м'ютекс, дозволяючи клієнтам знову записувати дані у спільну пам'ять
    ReleaseMutex(hMapMutex);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Створюємо кнопку запуску сервера (ID_FILEMAPPING_START)
        CreateWindowA("BUTTON", "Start (Створити)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 138, 30, hWnd, (HMENU)ID_FILEMAPPING_START, hInst, NULL);
        // Створюємо кнопку примусового ручного читання проекції (ID_FILEMAPPING_READ)
        CreateWindowA("BUTTON", "Read (Прочитати)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            148, 5, 138, 30, hWnd, (HMENU)ID_FILEMAPPING_READ, hInst, NULL);
        // Створюємо кнопку зупинки сервера (ID_FILEMAPPING_STOP)
        CreateWindowA("BUTTON", "Stop (Зупинити)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            291, 5, 138, 30, hWnd, (HMENU)ID_FILEMAPPING_STOP, hInst, NULL);

        // Записуємо початковий заголовок у буфер mess
        strcpy_s(mess, sizeof(mess), "Лабораторна робота - Відображення (проекція) файлу в пам'ять.\r\n");
        // Створюємо багаторядкове поле редагування hwndEdit для виведення отриманих даних
        hwndEdit = CreateWindowA("EDIT", mess,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 425, 330, hWnd, NULL, hInst, NULL);

        // Створюємо іменований м'ютекс "myFileMappingMutex" для взаємного виключення між процесами (*)
        hMapMutex = CreateMutexA(NULL, FALSE, "myFileMappingMutex");
        // Створюємо іменовану подію з автоскиданням "myFileMappingEvent" для сигналів від клієнтів (*)
        hDataReadyEvent = CreateEventA(NULL, FALSE, FALSE, "myFileMappingEvent");
        // Створюємо подію ручного скидання для безпечної зупинки потоку WatcherThreadProc (*)
        hStopWatchEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        break;

    case WM_SIZE:
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
            // --- СТВОРЕННЯ ТА ЗАПУСК FILE MAPPING ---
        case ID_FILEMAPPING_START:
            // Якщо об'єкт уже створений — повідомляємо користувача і виходимо
            if (hMapFile != NULL)
            {
                MessageBoxA(hWnd, "Сервер FileMapping вже запущено!", "Map", MB_OK | MB_ICONINFORMATION);
                break;
            }

            // Створюємо об'єкт відображення в оперативній пам'яті:
            // INVALID_HANDLE_VALUE - створюємо в ОЗП (у файлі підкачки системи) без фізичного файлу на диску;
            // NULL - стандартні атрибути безпеки;
            // PAGE_READWRITE - дозвіл і на читання, і на запис;
            // 0, 512 - розмір виділеної ділянки пам'яті (512 байтів);
            // "myFileMapping" - глобальне ім'я об'єкта, за яким до нього підключаться клієнти.
            hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 512, "myFileMapping");
            // Перевіряємо, чи успішно створено об'єкт проекції
            if (hMapFile == NULL || hMapFile == INVALID_HANDLE_VALUE)
            {
                hMapFile = NULL;
                MessageBoxA(hWnd, "Не можливо відкрити відображений у пам'яті об'єкт", "Map", MB_OK);
                break;
            }

            // Переводимо подію зупинки в несигнальний стан перед запуском потоку
            ResetEvent(hStopWatchEvent);
            // Створюємо фоновий потік WatcherThreadProc для автоматичного стеження за змінами пам'яті (*)
            hWatcherThread = CreateThread(NULL, 0, WatcherThreadProc, NULL, 0, NULL);

            // Виводимо повідомлення про запуск сервера у вікно
            strcpy_s(mess, sizeof(mess), "Сервер FileMapping запущено\r\n");
            SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)m_mess);
            break;

            // --- РУЧНЕ ЧИТАННЯ ДАНИХ ЗА КНОПКОЮ "READ" ---
        case ID_FILEMAPPING_READ:
            // Викликаємо функцію читання з прапорцем isManual = true
            ReadFileMappingData(hWnd, true);
            break;

            // --- ЗУПИНКА СЕРВЕРА ТА ЗАКРИТТЯ FILE MAPPING ---
        case ID_FILEMAPPING_STOP:
            if (hMapFile != NULL)
            {
                // Подаємо сигнал фоновому потоку завершити роботу
                SetEvent(hStopWatchEvent);
                if (hWatcherThread != NULL)
                {
                    // Чекаємо повного завершення фонового потоку
                    WaitForSingleObject(hWatcherThread, INFINITE);
                    // Закриваємо дескриптор потоку
                    CloseHandle(hWatcherThread);
                    hWatcherThread = NULL;
                }

                // Закриваємо дескриптор об'єкта відображення файлу (знищуємо спільну пам'ять)
                CloseHandle(hMapFile);
                hMapFile = NULL;

                // Виводимо повідомлення про зупинку сервера
                strcpy_s(mess, sizeof(mess), "Сервер FileMapping зупинено\r\n");
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)m_mess);
            }
            break;
        }
        break;

        // --- ОБРОБНИК АВТОМАТИЧНОГО ВИЯВЛЕННЯ ЗМІН (ЗІРОЧКА *) ---
    case WM_AUTO_READ:
        // Викликаємо читання пам'яті автоматично (isManual = false) після сигналу від клієнта
        ReadFileMappingData(hWnd, false);
        break;

    case WM_DESTROY:
        // При закритті вікна зупиняємо фоновий потік і вивільняємо всі системні дескриптори
        SetEvent(hStopWatchEvent);
        if (hWatcherThread != NULL)
        {
            WaitForSingleObject(hWatcherThread, 1000);
            CloseHandle(hWatcherThread);
        }
        if (hMapFile != NULL) CloseHandle(hMapFile);
        if (hMapMutex != NULL) CloseHandle(hMapMutex);
        if (hDataReadyEvent != NULL) CloseHandle(hDataReadyEvent);
        if (hStopWatchEvent != NULL) CloseHandle(hStopWatchEvent);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}