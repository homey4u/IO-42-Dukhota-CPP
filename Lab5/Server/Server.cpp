#define _CRT_SECURE_NO_WARNINGS

#include <windows.h> // Головна бібліотека WinAPI (містить усі функції для роботи з Mailslot і вікнами)
#include <stdio.h>   // Для форматування рядків (sprintf_s)
#include <string>    // Для роботи з текстом (strcat_s, strcpy_s)

// Ідентифікатори кнопок та системного таймера сервера
#define ID_MAILSLOT_CREATE 1001 // ID кнопки "Створити скриньку (Create)"
#define ID_MAILSLOT_CLOSE  1002 // ID кнопки "Закрити скриньку (Close)"
#define IDT_TIMER          1003 // ID таймера для щосекундної перевірки поштової скриньки

// --- ГЛОБАЛЬНІ ЗМІННІ СЕРВЕРА ---
HINSTANCE hInst; // Дескриптор процесу програми
HWND hwndEdit;   // Дескриптор багаторядкового текстового поля для виведення логів

// Дескриптор поштової скриньки (спочатку неактивний = INVALID_HANDLE_VALUE)
HANDLE hMailslot = INVALID_HANDLE_VALUE;
// Системне ім'я поштової скриньки на локальному комп'ютері 
LPCSTR MailslotName = "\\\\.\\mailslot\\lab5";

char szBuf[1024];     // Тимчасовий буфер для читання одного повідомлення зі скриньки
char mess[4096] = ""; // Загальний буфер для збереження всієї історії повідомлень на екрані
DWORD cbRead;         // Кількість фактично прочитаних байтів функцією ReadFile

// Попереднє оголошення віконної процедури
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ СЕРВЕРА ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance;

    // Налаштування та реєстрація класу вікна сервера
    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = "MailslotServerClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    // Створюємо головне вікно сервера ліворуч зверху (X=150, Y=100, ширина=450, висота=380)
    HWND hWnd = CreateWindowA("MailslotServerClass", "s (Сервер - Поштова скринька)", WS_OVERLAPPEDWINDOW,
        150, 100, 450, 380, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Головний цикл повідомлень Windows
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОДІЙ ВІКНА СЕРВЕРА ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: // Виконується один раз під час створення вікна сервера
        // Створюємо дві кнопки керування поштовою скринькою
        CreateWindowA("BUTTON", "Створити скриньку (Create)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_MAILSLOT_CREATE, hInst, NULL);
        CreateWindowA("BUTTON", "Закрити скриньку (Close)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_MAILSLOT_CLOSE, hInst, NULL);

        // Створюємо текстове поле зі стартовим написом (як на рис. 2 методички)
        strcpy_s(mess, sizeof(mess), "Лабораторна робота - Поштова скриня.\r\n");
        hwndEdit = CreateWindowA("EDIT", mess,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 290, hWnd, NULL, hInst, NULL);
        break;

    case WM_SIZE: // Автоматичне масштабування текстового поля при зміні розмірів вікна
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND: // Обробка натискань на кнопки
        switch (LOWORD(wParam))
        {
            // --- ДОДАНА ЛОГІКА 1: СТВОРЕННЯ ТА ЗАПУСК ПОШТОВОЇ СКРИНЬКИ ---
        case ID_MAILSLOT_CREATE:
            if (hMailslot != INVALID_HANDLE_VALUE)
            {
                MessageBoxA(hWnd, "Поштова скринька вже створена і працює!", "Інфо", MB_OK | MB_ICONINFORMATION);
                break;
            }

            strcpy_s(mess, sizeof(mess), "Поштова скриня\r\n");
            SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);

            // Створюємо поштову скриньку функцією CreateMailslotA:
            // 0 - максимальний розмір повідомлення не обмежений;
            // MAILSLOT_WAIT_FOREVER - повідомлення зберігаються у скриньці, доки їх не прочитають
            hMailslot = CreateMailslotA(MailslotName, 0, MAILSLOT_WAIT_FOREVER, NULL);
            if (hMailslot == INVALID_HANDLE_VALUE)
            {
                // Якщо виникла помилка створення — виводимо код помилки GetLastError()
                char errBuf[256];
                sprintf_s(errBuf, sizeof(errBuf), "\r\nПоштовий сервер. Error: %lu\r\n", GetLastError());
                strcat_s(mess, sizeof(mess), errBuf);
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
                break;
            }

            // Якщо скринька успішно створена — виводимо повідомлення та запускаємо таймер (1000 мс = 1 сек)
            strcat_s(mess, sizeof(mess), "\r\nСервер запущено\r\n");
            SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
            SetTimer(hWnd, IDT_TIMER, 1000, (TIMERPROC)NULL);
            break;

            // --- ДОДАНА ЛОГІКА 2: ЗАКРИТТЯ ПОШТОВОЇ СКРИНЬКИ ---
        case ID_MAILSLOT_CLOSE:
            if (hMailslot != INVALID_HANDLE_VALUE)
            {
                KillTimer(hWnd, IDT_TIMER);           // Зупиняємо таймер опитування скриньки
                CloseHandle(hMailslot);               // Закриваємо дескриптор поштової скриньки
                hMailslot = INVALID_HANDLE_VALUE;     // Позначаємо скриньку як закриту
                strcat_s(mess, sizeof(mess), "\r\nПоштовий сервер закрито\r\n");
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
            }
            break;
        }
        break;

        // --- ДОДАНА ЛОГІКА 3: ЧИТАННЯ ПОШТОВОЇ СКРИНЬКИ ЗА ТАЙМЕРОМ (ЩОСЕКУНДИ) ---
    case WM_TIMER:
        if (wParam == IDT_TIMER && hMailslot != INVALID_HANDLE_VALUE)
        {
            DWORD cbMessage = 0; // Розмір наступного повідомлення в байтах
            DWORD cMessage = 0;  // Загальна кількість повідомлень, що чекають у скриньці

            // Перевіряємо стан поштової скриньки функцією GetMailslotInfo
            BOOL fResult = GetMailslotInfo(hMailslot, NULL, &cbMessage, &cMessage, NULL);

            // Якщо у черзі скриньки є повідомлення (cMessage > 0) — вичитуємо їх усі по черзі (FIFO)
            while (fResult && cMessage != 0 && cbMessage != MAILSLOT_NO_MESSAGE)
            {
                DWORD bytesToRead = (cbMessage < sizeof(szBuf) - 1) ? cbMessage : (sizeof(szBuf) - 1);

                // Читаємо повідомлення зі скриньки як зі звичайного файлу функцією ReadFile
                if (ReadFile(hMailslot, szBuf, bytesToRead, &cbRead, NULL))
                {
                    szBuf[cbRead] = '\0'; // Гарантуємо правильне завершення рядка
                    strcat_s(mess, sizeof(mess), "\r\n");
                    strcat_s(mess, sizeof(mess), szBuf);
                    strcat_s(mess, sizeof(mess), "\r\n");
                    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
                }

                // Перевіряємо, чи не залишилося в черзі ще повідомлень
                fResult = GetMailslotInfo(hMailslot, NULL, &cbMessage, &cMessage, NULL);
            }
        }
        break;

    case WM_DESTROY: // При закритті вікна сервера зупиняємо таймер і закриваємо скриньку
        KillTimer(hWnd, IDT_TIMER);
        if (hMailslot != INVALID_HANDLE_VALUE) CloseHandle(hMailslot);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}