// Вимикаємо попередження компілятора про застарілі мережеві функції (gethostbyname) та функції роботи з рядками
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h> // Підключаємо бібліотеку сокетів Windows Sockets 2 (обов'язково перед windows.h)
#include <windows.h>  // Підключаємо головну бібліотеку WinAPI (вікна, графіка GDI, системні метрики)
#include <stdio.h>    // Підключаємо функцію sprintf_s для форматування рядків
#include <string>     // Підключаємо функції роботи з текстом (strlen, strcpy_s, strcat_s)

// Автоматично підключаємо системну бібліотеку сокетів WS2_32.lib на етапі лінкування проєкту
#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000              // Номер порту сервера, до якого підключається Клієнт 2
#define WSA_NETEVENT (WM_USER + 1)  // Код повідомлення вікна для асинхронних подій на сокеті

// Ідентифікатори кнопок керування у вікні Клієнта 2:
#define ID_CLIENT_SETCONNECTION 2001 // ID кнопки "Підключитися"
#define ID_CLIENT_SENDMSG       2002 // ID кнопки "Надіслати дані"

// --- ГЛОБАЛЬНІ ЗМІННІ ---
HINSTANCE hInst; // Дескриптор екземпляра програми Клієнта 2 в пам'яті
HWND hwndEdit;   // Дескриптор багаторядкового текстового поля для виведення логів

char szBuf[1024];     // Тимчасовий буфер для формування або прийому одного мережевого пакета
char mess[4096] = ""; // Буфер для збереження всієї історії повідомлень у вікні клієнта

WSADATA wsaData;                         // Структура з інформацією про завантажену версію Winsock
WORD wVersionRequested = MAKEWORD(2, 2); // Запитуємо версію інтерфейсу Windows Sockets 2.2
SOCKET cln_socket = INVALID_SOCKET;      // Дескриптор сокета клієнта (до з'єднання = INVALID_SOCKET)
PHOSTENT phe;                            // Вказівник на структуру з мережевою інформацією про сервер
SOCKADDR_IN dest_sin;                    // Структура адреси сервера (IP-адреса + порт 5000)
char szHostName[128] = "localhost";      // Символьне ім'я сервера ("localhost" = локальний ПК, 127.0.0.1)

// Попереднє оголошення віконної процедури
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА 2 ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance; // Зберігаємо дескриптор додатка

    // Налаштовуємо та реєструємо клас головного вікна Клієнта 2
    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);                  // Розмір структури
    wcex.style = CS_HREDRAW | CS_VREDRAW;               // Перемальовувати вікно при зміні його розмірів
    wcex.lpfnWndProc = WndProc;                         // Вказуємо функцію WndProc для обробки подій вікна
    wcex.hInstance = hInstance;                         // Прив'язуємо клас до поточного процесу
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);    // Стандартна іконка програми
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);      // Стандартний курсор миші
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);   // Світло-сірий колір фону вікна
    wcex.lpszClassName = "Client2WindowClass";          // Ім'я класу вікна Клієнта 2
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);  // Мала іконка заголовка вікна

    RegisterClassExA(&wcex); // Реєструємо клас вікна в системі

    // Створюємо вікно Клієнта 2 прямо під вікном Клієнта 1 (координати X=620, Y=360, розмір 450x260)
    HWND hWnd = CreateWindowA("Client2WindowClass", "2 (Client 2 - Варіант 8)", WS_OVERLAPPEDWINDOW,
        620, 360, 450, 260, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow); // Відображаємо вікно на екрані
    UpdateWindow(hWnd);         // Оновлюємо клієнтську область вікна

    // Головний цикл обробки повідомлень Windows
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); // Перетворює клавіатурні події на символи
        DispatchMessage(&msg);  // Направляє повідомлення у віконну процедуру WndProc
    }
    return (int)msg.wParam;
}

// --- ФУНКЦІЯ ВСТАНОВЛЕННЯ З'ЄДНАННЯ ІЗ СЕРВЕРОМ ---
BOOL SetConnection(HWND hWnd)
{
    // Якщо сокет уже був відкритий раніше — закриваємо його перед новим підключенням
    if (cln_socket != INVALID_SOCKET)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
    }

    // 1. Створюємо потоковий TCP-сокет для мережі IPv4
    cln_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Socket error", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 2. Отримуємо IP-адресу вузла за ім'ям "localhost" (127.0.0.1)
    phe = gethostbyname(szHostName);
    if (phe == NULL)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Адреса хосту не визначена", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 3. Заповнюємо структуру адреси сервера dest_sin
    dest_sin.sin_family = AF_INET;                                   // Сімейство адрес IPv4
    dest_sin.sin_port = htons(SERV_PORT);                            // Порт сервера 5000 у мережевому порядку байтів
    memcpy((char*)&(dest_sin.sin_addr), phe->h_addr, phe->h_length); // Копіюємо IP-адресу сервера у структуру

    // 4. Встановлюємо з'єднання із сервером за допомогою функції connect()
    if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) == SOCKET_ERROR)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Помилка з'єднання (перевірте, чи запущено Сервер!)", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 5. Налаштовуємо асинхронний режим: при надходженні даних (FD_READ) або закритті з'єднання (FD_CLOSE)
    // вікно клієнта отримає повідомлення WSA_NETEVENT
    if (WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE))
    {
        MessageBoxA(hWnd, "WSAAsyncSelect error", "Error", MB_OK);
        return FALSE;
    }

    // Виводимо повідомлення про успішне підключення у текстове поле
    strcpy_s(mess, sizeof(mess), "Зв'язок встановлений!\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    return TRUE;
}

// =========================================================================
// РЕАЛІЗАЦІЯ ВАРІАНТА 8 (КЛІЄНТ 2): ЗБІР ТА ВІДПРАВКА ПАРАМЕТРІВ СИСТЕМИ
// =========================================================================
void SendMsg(HWND hWnd)
{
    // Перевіряємо, чи встановлено з'єднання з сервером
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Спочатку встановіть з'єднання з сервером!", "Увага", MB_OK | MB_ICONWARNING);
        return;
    }

    // ПУНКТ 1 ВАРІАНТА 8: Визначаємо метрику SM_CXSCREEN (ширина екрана) та RGB-колір фону робочого столу
    int cxScreen = GetSystemMetrics(SM_CXSCREEN);        // Запитуємо у системи ширину основного монітора в пікселях
    COLORREF bgColor = GetSysColor(COLOR_DESKTOP);       // Запитуємо поточний колір фону робочого столу Windows

    // ПУНКТ 2 ВАРІАНТА 8: Визначаємо стандартний розмір піктограм на робочому столі (SM_CXICON)
    int iconSize = GetSystemMetrics(SM_CXICON);          // Запитуємо ширину системної іконки в пікселях (зазвичай 32 px)

    // ПУНКТ 3 ВАРІАНТА 8: Визначаємо горизонтальний DPI екрана через контекст пристрою (LOGPIXELSX)
    HDC hdc = GetDC(hWnd);                               // Отримуємо контекст графічного пристрою (екрана) для вікна
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);           // Запитуємо кількість пікселів на логічний дюйм по горизонталі
    ReleaseDC(hWnd, hdc);                                // Обов'язково звільняємо контекст пристрою у систему

    // Формуємо підсумковий текстовий рядок у буфері szBuf
    // (Макроси GetRValue, GetGValue, GetBValue розкладають колір bgColor на складові Red, Green, Blue від 0 до 255)
    sprintf_s(szBuf, sizeof(szBuf),
        "Колір фону / SM_CXSCREEN: %d (RGB: %u, %u, %u)\r\n"
        "Розмір піктограм (SM_CXICON): %d px\r\n"
        "DPI горизонтально (LOGPIXELSX): %d dpi\r\n",
        cxScreen, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor),
        iconSize, dpiX);

    // Відправляємо сформований буфер szBuf на сервер через відкритий сокет cln_socket
    if (send(cln_socket, szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        // Відображаємо надіслані дані у власному текстовому полі клієнта
        sprintf_s(mess, sizeof(mess), "Дані надіслані серверу:\r\n%s", szBuf);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
    else
    {
        strcat_s(mess, sizeof(mess), "\r\nПомилка надсилання повідомлення\r\n");
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОВІДОМЛЕНЬ ВІКНА КЛІЄНТА 2 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: // Виконується під час створення вікна
        // Створюємо кнопки "Підключитися" та "Надіслати дані"
        CreateWindowA("BUTTON", "Підключитися", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SETCONNECTION, hInst, NULL);
        CreateWindowA("BUTTON", "Надіслати дані", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SENDMSG, hInst, NULL);

        // Створюємо багаторядкове текстове поле з прокруткою для виведення повідомлень
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Ініціалізуємо бібліотеку Winsock
        if (WSAStartup(wVersionRequested, &wsaData) != 0)
        {
            MessageBoxA(hWnd, "WSAStartup Error", "ERROR", MB_OK | MB_ICONSTOP);
            return FALSE;
        }
        // Виводимо інформацію про версію та статус мережевої підсистеми
        sprintf_s(mess, sizeof(mess), "Використовується %s \r\nСтатус: %s\r\n",
            wsaData.szDescription, wsaData.szSystemStatus);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        break;

    case WM_SIZE: // Підлаштовуємо розмір текстового поля при зміні розміру вікна
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND: // Обробляємо натискання на кнопки
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SETCONNECTION: SetConnection(hWnd); break; // Клік по "Підключитися"
        case ID_CLIENT_SENDMSG:       SendMsg(hWnd); break;       // Клік по "Надіслати дані"
        }
        break;

    case WSA_NETEVENT: // Обробляємо мережеві події від сервера
        // Якщо від сервера надійшли дані (подія FD_READ):
        if (WSAGETSELECTEVENT(lParam) == FD_READ)
        {
            // Приймаємо байти з сокета у буфер szBuf
            int rc = recv(cln_socket, szBuf, sizeof(szBuf) - 1, 0);
            if (rc > 0)
            {
                szBuf[rc] = '\0'; // Завершуємо отриманий рядок нуль-термінатором
                char block[1200];
                sprintf_s(block, sizeof(block), "\r\nДані від сервера: %s\r\n", szBuf);
                strcat_s(mess, sizeof(mess), block);
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
            }
        }
        // Якщо сервер закрив з'єднання або примусово відключив клієнта (подія FD_CLOSE):
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            closesocket(cln_socket);     // Закриваємо сокет клієнта
            cln_socket = INVALID_SOCKET; // Скидаємо дескриптор для можливості повторного підключення
            MessageBoxA(hWnd, "Сервер закритий або відключив клієнта", "Server", MB_OK);
        }
        break;

    case WM_DESTROY: // При закритті вікна клієнта
        if (cln_socket != INVALID_SOCKET) closesocket(cln_socket); // Закриваємо сокет, якщо він активний
        WSACleanup();        // Звільняємо ресурси бібліотеки Winsock
        PostQuitMessage(0);  // Завершуємо цикл повідомлень програми
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}