// Вимикаємо попередження компілятора про застарілі мережеві функції та функції роботи з рядками
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h> // Бібліотека для роботи з сокетами Windows Sockets 2
#include <windows.h>  // Головна бібліотека WinAPI для вікон та елементів керування
#include <stdio.h>    // Функції форматованого вводу-виводу (sprintf_s)
#include <string>     // Функції для роботи з рядками (strlen, strcpy_s, strcat_s)

// Підключаємо системну бібліотеку сокетів WS2_32.lib
#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000              // Порт сервера (5000)
#define WSA_NETEVENT (WM_USER + 1)  // Повідомлення для асинхронних подій на сокеті

// Ідентифікатори кнопок керування:
#define ID_CLIENT_SETCONNECTION 2001 // ID кнопки "Підключитися"
#define ID_CLIENT_SENDMSG       2002 // ID кнопки "Надіслати текст"

// --- ГЛОБАЛЬНІ ЗМІННІ ---
HINSTANCE hInst; // Дескриптор екземпляра програми Клієнта 3
HWND hwndInput;  // Дескриптор однорядкового поля для вводу довільного тексту користувачем (Завдання з зірочкою!)
HWND hwndEdit;   // Дескриптор багаторядкового поля для відображення історії логів

char szBuf[1024];     // Буфер для зчитування введеного тексту та обміну по мережі
char mess[4096] = ""; // Буфер для збереження історії логів на екрані

WSADATA wsaData;                         // Структура даних бібліотеки Winsock
WORD wVersionRequested = MAKEWORD(2, 2); // Версія Winsock 2.2
SOCKET cln_socket = INVALID_SOCKET;      // Сокет Клієнта 3
PHOSTENT phe;                            // Вказівник на інформацію про хост сервера
SOCKADDR_IN dest_sin;                    // Структура адреси сервера
char szHostName[128] = "localhost";      // Ім'я вузла сервера ("localhost")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ КЛІЄНТА 3 ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance;

    // Реєстрація класу вікна Клієнта 3
    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = "Client3WindowClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex);

    // Створюємо вікно Клієнта 3 прямо під вікном Сервера (координати X=150, Y=510, розмір 450x290)
    HWND hWnd = CreateWindowA("Client3WindowClass", "3 (Client 3 * - Довільний текст)", WS_OVERLAPPEDWINDOW,
        150, 510, 450, 290, nullptr, nullptr, hInstance, nullptr);
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

// --- ФУНКЦІЯ ВСТАНОВЛЕННЯ З'ЄДНАННЯ ІЗ СЕРВЕРОМ ---
BOOL SetConnection(HWND hWnd)
{
    // Закриваємо попередній сокет, якщо відбувається повторне перепідключення
    if (cln_socket != INVALID_SOCKET)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
    }

    // 1. Створюємо потоковий TCP-сокет
    cln_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Socket error", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 2. Визначаємо IP-адресу сервера за ім'ям "localhost"
    phe = gethostbyname(szHostName);
    if (phe == NULL)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Адреса хосту не визначена", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 3. Заповнюємо структуру адреси сервера (IPv4, порт 5000)
    dest_sin.sin_family = AF_INET;
    dest_sin.sin_port = htons(SERV_PORT);
    memcpy((char*)&(dest_sin.sin_addr), phe->h_addr, phe->h_length);

    // 4. Виконуємо підключення до сервера через connect()
    if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) == SOCKET_ERROR)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Помилка з'єднання (перевірте, чи запущено Сервер!)", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    // 5. Налаштовуємо асинхронне отримання подій читання (FD_READ) та закриття (FD_CLOSE)
    if (WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE))
    {
        MessageBoxA(hWnd, "WSAAsyncSelect error", "Error", MB_OK);
        return FALSE;
    }

    strcpy_s(mess, sizeof(mess), "Зв'язок встановлений!\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    return TRUE;
}

// =========================================================================
// РЕАЛІЗАЦІЯ ЗАВДАННЯ «З ЗІРОЧКОЮ» (КЛІЄНТ 3): НАДСИЛАННЯ ДОВІЛЬНОГО ТЕКСТУ
// =========================================================================
void SendMsg(HWND hWnd)
{
    // Перевіряємо, чи встановлено з'єднання з сервером
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Спочатку встановіть з'єднання з сервером!", "Увага", MB_OK | MB_ICONWARNING);
        return;
    }

    // 1. Дізнаємося довжину тексту, який користувач ввів у поле вводу hwndInput
    int len = SendMessageA(hwndInput, WM_GETTEXTLENGTH, 0, 0);
    if (len <= 0)
    {
        MessageBoxA(hWnd, "Введіть текст у поле вводу!", "Увага", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (len >= sizeof(szBuf)) len = sizeof(szBuf) - 1;

    // 2. Зчитуємо введений користувачем текст із поля hwndInput у буфер szBuf
    SendMessageA(hwndInput, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)szBuf);
    szBuf[len] = '\0'; // Гарантуємо правильне завершення рядка нуль-термінатором

    // 3. Відправляємо зчитаний довільний текст на сервер функцією send()
    if (send(cln_socket, szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        sprintf_s(mess, sizeof(mess), "Дані надіслані серверу:\r\n%s\r\n", szBuf);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
    else
    {
        strcat_s(mess, sizeof(mess), "\r\nПомилка надсилання повідомлення\r\n");
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОВІДОМЛЕНЬ ВІКНА КЛІЄНТА 3 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: // Виконується при створенні вікна Клієнта 3
        // Створюємо кнопки керування
        CreateWindowA("BUTTON", "Підключитися", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SETCONNECTION, hInst, NULL);
        CreateWindowA("BUTTON", "Надіслати текст", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 30, hWnd, (HMENU)ID_CLIENT_SENDMSG, hInst, NULL);

        // Створюємо однорядкове поле hwndInput для вводу довільного тексту з клавіатури
        hwndInput = CreateWindowA("EDIT", "Привіт, Сервере! Повідомлення від Клієнта 3.",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            5, 40, 415, 28, hWnd, NULL, hInst, NULL);

        // Створюємо багаторядкове поле hwndEdit для відображення статусу та історії повідомлень
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 73, 415, 170, hWnd, NULL, hInst, NULL);

        // Ініціалізуємо бібліотеку Winsock
        if (WSAStartup(wVersionRequested, &wsaData) != 0)
        {
            MessageBoxA(hWnd, "WSAStartup Error", "ERROR", MB_OK | MB_ICONSTOP);
            return FALSE;
        }
        sprintf_s(mess, sizeof(mess), "Використовується %s \r\nСтатус: %s\r\n",
            wsaData.szDescription, wsaData.szSystemStatus);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        break;

    case WM_SIZE: // Масштабуємо обидва поля при зміні розміру вікна
        MoveWindow(hwndInput, 5, 40, LOWORD(lParam) - 10, 28, TRUE);
        MoveWindow(hwndEdit, 5, 73, LOWORD(lParam) - 10, HIWORD(lParam) - 78, TRUE);
        break;

    case WM_COMMAND: // Обробляємо кліки по кнопках
        switch (LOWORD(wParam))
        {
        case ID_CLIENT_SETCONNECTION: SetConnection(hWnd); break;
        case ID_CLIENT_SENDMSG:       SendMsg(hWnd); break;
        }
        break;

    case WSA_NETEVENT: // Обробляємо події від сервера
        // Прийом повідомлення від сервера (FD_READ):
        if (WSAGETSELECTEVENT(lParam) == FD_READ)
        {
            int rc = recv(cln_socket, szBuf, sizeof(szBuf) - 1, 0);
            if (rc > 0)
            {
                szBuf[rc] = '\0';
                char block[1200];
                sprintf_s(block, sizeof(block), "\r\nДані від сервера: %s\r\n", szBuf);
                strcat_s(mess, sizeof(mess), block);
                SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
            }
        }
        // Розрив з'єднання з боку сервера (FD_CLOSE):
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            closesocket(cln_socket);
            cln_socket = INVALID_SOCKET;
            MessageBoxA(hWnd, "Сервер закритий або відключив клієнта", "Server", MB_OK);
        }
        break;

    case WM_DESTROY: // Коректне вивільнення ресурсів при закритті вікна
        if (cln_socket != INVALID_SOCKET) closesocket(cln_socket);
        WSACleanup();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}