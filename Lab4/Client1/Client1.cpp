// Вимикаємо помилки сучасного компілятора Visual Studio щодо "застарілих" функцій Winsock (gethostbyname) та C-рядків
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h> // Підключаємо бібліотеку сокетів Windows Sockets 2 (обов'язково перед windows.h!)
#include <windows.h>  // Підключаємо головну бібліотеку WinAPI для вікон, кнопок та системних метрик
#include <stdio.h>    // Підключаємо функцію sprintf_s для форматування тексту зі змінними
#include <string>     // Підключаємо функції для роботи з рядками (strlen, strcpy_s, strcat_s)

// Наказуємо компонувальнику (лінкеру) підключити системний файл реалізації мережі WS2_32.lib
#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000              // Номер порту сервера (має збігатися з портом у Server.cpp!)
#define WSA_NETEVENT (WM_USER + 1)  // Користувацьке повідомлення вікна: "Від сервера прийшли дані або розрив зв'язку"

// Унікальні номери (ID) для наших двох кнопок у вікні клієнта:
#define ID_CLIENT_SETCONNECTION 2001 // ID кнопки "Підключитися"
#define ID_CLIENT_SENDMSG       2002 // ID кнопки "Надіслати дані"

// --- ГЛОБАЛЬНІ ЗМІННІ КЛІЄНТА ---
HINSTANCE hInst; // Дескриптор (ідентифікатор) запущеної програми в оперативній пам'яті
HWND hwndEdit;   // Дескриптор білого багаторядкового текстового поля для виведення логів

char szBuf[1024];     // Тимчасовий буфер для формування одного повідомлення (на відправку або прийом)
char mess[4096] = ""; // Великий буфер, де зберігається вся історія тексту на екрані клієнта

WSADATA wsaData;                         // Структура, куди Windows запише інформацію про версію Winsock
WORD wVersionRequested = MAKEWORD(2, 2); // Просимо у системи завантажити версію Winsock 2.2
SOCKET cln_socket = INVALID_SOCKET;      // Сокет клієнта (спочатку порожній = INVALID_SOCKET)
PHOSTENT phe;                            // Вказівник на мережеву структуру з IP-адресою сервера
SOCKADDR_IN dest_sin;                    // Структура, куди ми запишемо IP-адресу та порт сервера (5000)
char szHostName[128] = "localhost";      // Ім'я комп'ютера-сервера ("localhost" означає цей самий ПК, IP 127.0.0.1)

// Попереднє оголошення головної функції обробки подій вікна
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance; // Зберігаємо ідентифікатор програми в глобальну змінну

    // Заповнюємо структуру налаштувань зовнішнього вигляду вікна
    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);                  // Розмір структури в байтах
    wcex.style = CS_HREDRAW | CS_VREDRAW;               // Перемальовувати вікно при зміні ширини чи висоти
    wcex.lpfnWndProc = WndProc;                         // Вказуємо функцію WndProc як обробник усіх подій вікна
    wcex.hInstance = hInstance;                         // Прив'язуємо вікно до нашої програми
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);    // Стандартна іконка програми
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);      // Стандартний курсор миші (стрілочка)
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);   // Світло-сірий колір фону вікна
    wcex.lpszClassName = "Client1WindowClass";          // Унікальне системне ім'я класу вікна Клієнта 1
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);  // Маленька іконка в заголовку вікна

    RegisterClassExA(&wcex); // Реєструємо створений шаблон вікна в ОС Windows

    // Створюємо вікно Клієнта 1 на екрані (координати X=620, Y=80, ширина=450, висота=260)
    HWND hWnd = CreateWindowA("Client1WindowClass", "1 (Client 1 - Варіант 8)", WS_OVERLAPPEDWINDOW,
        620, 80, 450, 260, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE; // Якщо вікно не створилося — завершуємо програму

    ShowWindow(hWnd, nCmdShow); // Показуємо вікно на екрані
    UpdateWindow(hWnd);         // Наказуємо вікну вперше себе намалювати

    // Нескінченний цикл повідомлень Windows (тримає програму відкритою і ловить кліки миші та сигнали мережі)
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); // Розшифровує натискання клавіш клавіатури
        DispatchMessage(&msg);  // Передає подію у нашу функцію WndProc
    }
    return (int)msg.wParam;
}

// --- ФУНКЦІЯ ПІДКЛЮЧЕННЯ КЛІЄНТА ДО СЕРВЕРА ---
BOOL SetConnection(HWND hWnd)
{
    // Якщо клієнт вже був підключений раніше — спочатку закриваємо старий сокет для чистого перепідключення
    if (cln_socket != INVALID_SOCKET)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
    }

    // КРОК 1: Створюємо клієнтський сокет (AF_INET = мережа IPv4, SOCK_STREAM = надійний протокол TCP)
    cln_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Socket error", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // КРОК 2: Дізнаємося IP-адресу сервера за його ім'ям "localhost" (перетворює "localhost" на 127.0.0.1)
    phe = gethostbyname(szHostName);
    if (phe == NULL)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Адреса хосту не визначена", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // КРОК 3: Заповнюємо паспорт адреси сервера (куди саме стукати)
    dest_sin.sin_family = AF_INET;                                   // Вказуємо тип мережі IPv4
    dest_sin.sin_port = htons(SERV_PORT);                            // Переводимо порт 5000 у мережевий формат байтів
    memcpy((char*)&(dest_sin.sin_addr), phe->h_addr, phe->h_length); // Копіюємо знайдену IP-адресу (127.0.0.1) у структуру

    // КРОК 4: Стукаємо на сервер і встановлюємо з'єднання функцією connect()
    if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) == SOCKET_ERROR)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Помилка з'єднання (перевірте, чи запущено Сервер!)", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // КРОК 5: Вмикаємо асинхронний режим. Коли сервер щось відповість (FD_READ) або вимкнеться (FD_CLOSE),
    // Windows автоматично надішле нашому вікну hWnd повідомлення WSA_NETEVENT
    if (WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE))
    {
        MessageBoxA(hWnd, "WSAAsyncSelect error", "Error", MB_OK);
        return FALSE;
    }

    // Виводимо повідомлення про успішне з'єднання у текстове поле клієнта
    strcpy_s(mess, sizeof(mess), "Зв'язок встановлений!\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    return TRUE;
}

// --- ФУНКЦІЯ ЗБОРУ ДАНИХ ВАРІАНТА 8 (КЛІЄНТ 1) ТА ВІДПРАВКИ НА СЕРВЕР ---
void SendMsg(HWND hWnd)
{
    // Перевіряємо, чи клієнт підключився до сервера перед відправкою
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Спочатку встановіть з'єднання з сервером!", "Увага", MB_OK | MB_ICONWARNING);
        return;
    }

    // ЗАВДАННЯ ВАРІАНТА 8 (Пункт 1): Визначаємо версію ОС (чи це Windows Server R2)
    int osVersion = GetSystemMetrics(SM_SERVERR2);

    // ЗАВДАННЯ ВАРІАНТА 8 (Пункт 2): Визначаємо кількість підключених моніторів
    int monitorCount = GetSystemMetrics(SM_CMONITORS);

    // ЗАВДАННЯ ВАРІАНТА 8 (Пункт 3): Визначаємо глибину кольору екрана (біт на піксель)
    HDC hdc = GetDC(hWnd);                           // Отримуємо контекст пристрою (екрана) для нашого вікна
    int colorDepth = GetDeviceCaps(hdc, BITSPIXEL);  // Запитуємо у відеодрайвера кількість бітів на піксель (32)
    ReleaseDC(hWnd, hdc);                            // Обов'язково звільняємо контекст пристрою

    // Формуємо єдиний текстовий рядок із трьома показниками у буфері szBuf
    sprintf_s(szBuf, sizeof(szBuf),
        "Версія ОС (SM_SERVERR2): %d\r\n"
        "Кількість моніторів (SM_CMONITORS): %d\r\n"
        "Глибина кольору (BITSPIXEL): %d біт/піксель\r\n",
        osVersion, monitorCount, colorDepth);

    // Відправляємо сформований текст через сокет cln_socket на сервер функцією send()
    if (send(cln_socket, szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        // Якщо відправка успішна — показуємо відправлені дані у своєму вікні
        sprintf_s(mess, sizeof(mess), "Дані надіслані серверу:\r\n%s", szBuf);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
    else
    {
        strcat_s(mess, sizeof(mess), "\r\nПомилка надсилання повідомлення\r\n");
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОДІЙ ВІКНА КЛІЄНТА ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: // Спрацьовує один раз при відкритті вікна клієнта
        // Малюємо кнопку "Підключитися" (при натисканні надішле ID_CLIENT_SETCONNECTION)
        CreateWindowA("BUTTON", "Підключитися", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SETCONNECTION, hInst, NULL);
        // Малюємо кнопку "Надіслати дані" (при натисканні надішле ID_CLIENT_SENDMSG)
        CreateWindowA("BUTTON", "Надіслати дані", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SENDMSG, hInst, NULL);

        // Малюємо багаторядкове текстове поле hwndEdit під кнопками
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 40, 415, 175, hWnd, NULL, hInst, NULL);

        // Вмикаємо бібліотеку Winsock для цього клієнта
        if (WSAStartup(wVersionRequested, &wsaData) != 0)
        {
            MessageBoxA(hWnd, "WSAStartup Error", "ERROR", MB_OK | MB_ICONSTOP);
            return FALSE;
        }
        // Виводимо стартовий напис про версію та статус Winsock
        sprintf_s(mess, sizeof(mess), "Використовується %s \r\nСтатус: %s\r\n",
            wsaData.szDescription, wsaData.szSystemStatus);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        break;

    case WM_SIZE: // Автоматично розтягує текстове поле, якщо ти розтягуєш вікно мишкою
        MoveWindow(hwndEdit, 5, 40, LOWORD(lParam) - 10, HIWORD(lParam) - 45, TRUE);
        break;

    case WM_COMMAND: // Спрацьовує при кліку на одну з двох кнопок
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SETCONNECTION: SetConnection(hWnd); break; // Натиснули "Підключитися"
        case ID_CLIENT_SENDMSG:       SendMsg(hWnd); break;       // Натиснули "Надіслати дані"
        }
        break;

    case WSA_NETEVENT: // Спрацьовує, коли від Сервера прийшла подія по мережі
        // Якщо Сервер надіслав нам текстове повідомлення у відповідь (FD_READ):
        if (WSAGETSELECTEVENT(lParam) == FD_READ)
        {
            // Читаємо байти від сервера функцією recv() у буфер szBuf
            int rc = recv(cln_socket, szBuf, sizeof(szBuf) - 1, 0);
            if (rc > 0)
            {
                szBuf[rc] = '\0'; // Ставимо маркер кінця рядка
                char block[1200];
                sprintf_s(block, sizeof(block), "\r\nДані від сервера: %s\r\n", szBuf);
                strcat_s(mess, sizeof(mess), block); // Доклеюємо до тексту у вікні
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
            }
        }
        // Якщо Сервер примусово відключив нас або сам закрився (FD_CLOSE):
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            closesocket(cln_socket);     // Закриваємо сокет з нашого боку
            cln_socket = INVALID_SOCKET; // Позначаємо, що з'єднання більше немає
            MessageBoxA(hWnd, "Сервер закритий або відключив клієнта", "Server", MB_OK);
        }
        break;

    case WM_DESTROY: // Спрацьовує, коли ми закриваємо вікно клієнта хрестиком
        if (cln_socket != INVALID_SOCKET) closesocket(cln_socket); // Закриваємо сокет
        WSACleanup();        // Вивільняємо бібліотеку Winsock
        PostQuitMessage(0);  // Завершуємо роботу програми
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}