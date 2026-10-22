// Вимикаємо попередження компілятора про застарілі функції Winsock та C-рядків
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h> // Головна бібліотека для роботи з мережевими сокетами Windows
#include <windows.h>  // Бібліотека WinAPI для створення вікон, кнопок та повідомлень
#include <stdio.h>    // Бібліотека для форматування тексту (sprintf_s)
#include <string>     // Бібліотека для роботи з рядками (strlen, strcat_s)

// Вказуємо лінкеру Visual Studio підключити системний файл реалізації сокетів WS2_32.lib
#pragma comment(lib, "WS2_32.lib")

#define SERV_PORT 5000  // Номер мережевого порту, який буде прослуховувати сервер
#define MAX_CLIENTS 3   // Максимальна кількість клієнтів (Клієнт 1, Клієнт 2 + Клієнт 3 із зірочкою)

// Власні коди повідомлень для асинхронної роботи сокетів через вікно Windows:
#define WSA_ACCEPT   (WM_USER + 0) // Сигнал: "Новий клієнт хоче підключитися!"
#define WSA_NETEVENT (WM_USER + 1) // Сигнал: "Клієнт надіслав дані або відключився!"

// Унікальні номери (ID) для кожної кнопки у вікні сервера:
#define ID_SERVER_START   1001 // Кнопка "Старт сервера"
#define ID_SERVER_STOP    1002 // Кнопка "Стоп сервера"
#define ID_SENDTOCLIENT_1 1003 // Кнопка "Надіслати Клієнту 1"
#define ID_SENDTOCLIENT_2 1004 // Кнопка "Надіслати Клієнту 2"
#define ID_SENDTOCLIENT_3 1005 // Кнопка "Надіслати Клієнту 3 (*)"
#define ID_CLIENTOFF_1    1006 // Кнопка "Відключити Клієнта 1"
#define ID_CLIENTOFF_2    1007 // Кнопка "Відключити Клієнта 2"
#define ID_CLIENTOFF_3    1008 // Кнопка "Відключити Клієнта 3 (*)"

// --- ГЛОБАЛЬНІ ЗМІННІ ---
HINSTANCE hInst;       // Дескриптор екземпляра нашої програми в пам'яті
HWND hWindow;          // Дескриптор головного вікна сервера
HWND hwndEdit;         // Дескриптор багаторядкового текстового поля для виведення логів

char szBuf[1024];      // Буфер для тимчасового зберігання одного повідомлення
char mess[4096] = "";  // Буфер для накопичення всієї історії повідомлень на екрані
DWORD cbWritten;       // Змінна для збереження довжини зчитаного тексту

WSADATA wsaData;                           // Структура з інформацією про версію та стан Winsock
WORD wVersionRequested = MAKEWORD(2, 2);   // Запитуємо версію бібліотеки Winsock 2.2
SOCKET srv_socket = INVALID_SOCKET;        // Головний ("прослуховуючий") сокет сервера
// Масив із 3-х сокетів для спілкування з кожним підключеним клієнтом окремо:
SOCKET sock[MAX_CLIENTS] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
SOCKADDR_IN client_addr[MAX_CLIENTS];      // Масив структур, де зберігаються IP та порти клієнтів

// Попереднє оголошення головної функції обробки подій вікна
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// --- ТОЧКА ВХОДУ ПРОГРАМИ (БАЗОВИЙ ШАБЛОН ВІКНА) ---
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    hInst = hInstance; // Зберігаємо дескриптор програми

    // Налаштовуємо властивості класу головного вікна
    WNDCLASSEXA wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;               // Перемальовувати вікно при зміні розміру
    wcex.lpfnWndProc = WndProc;                         // Вказуємо функцію-обробник подій
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);    // Стандартна іконка програми
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);      // Стандартний курсор-стрілочка
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);   // Сірий колір фону вікна під кнопки
    wcex.lpszClassName = "ServerWindowClass";           // Ім'я класу вікна
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wcex); // Реєструємо клас вікна в ОС Windows

    // Створюємо саме головне вікно сервера на екрані
    HWND hWnd = CreateWindowA("ServerWindowClass", "serv (Сервер)", WS_OVERLAPPEDWINDOW,
        150, 120, 450, 380, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    hWindow = hWnd;
    ShowWindow(hWnd, nCmdShow); // Робимо вікно видимим
    UpdateWindow(hWnd);         // Оновлюємо вміст вікна

    // Головний цикл обробки повідомлень Windows (працює, доки вікно не закриють)
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg); // Перетворює натискання клавіш у символи
        DispatchMessage(&msg);  // Відправляє повідомлення у функцію WndProc
    }
    return (int)msg.wParam;
}

// --- ДОДАНА ФУНКЦІЯ 1: ЗАПУСК СЕРВЕРА ТА СТВОРЕННЯ СОКЕТА ---
void ServerStart(HWND hWnd)
{
    // Перевіряємо, чи сервер вже не запущений
    if (srv_socket != INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Сервер вже запущено. Socket створено", "Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // КРОК 1: Створюємо потоковий сокет (AF_INET = IPv4, SOCK_STREAM = протокол TCP)
    srv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Помилка створення сокету!!!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // КРОК 2: Заповнюємо структуру адреси сервера
    SOCKADDR_IN srv_address;
    srv_address.sin_family = AF_INET;                // Сімейство адрес IPv4
    srv_address.sin_port = htons(SERV_PORT);         // Переводимо порт 5000 у мережевий порядок байтів
    srv_address.sin_addr.s_addr = INADDR_ANY;        // Приймаємо підключення на будь-яку IP-адресу ПК

    // КРОК 3: Прив'язуємо створений сокет до порту 5000 (функція bind)
    if (SOCKET_ERROR == bind(srv_socket, (LPSOCKADDR)&srv_address, sizeof(srv_address)))
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Помилка зв'язування з портом", "Error", MB_OK | MB_ICONSTOP);
        return;
    }

    // КРОК 4: Переводимо сокет у режим прослуховування вхідних з'єднань (чергою до 5 запитів)
    if (listen(srv_socket, 5) == SOCKET_ERROR)
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Помилка очікування встановлення зв'язку", "Error", MB_OK);
        return;
    }

    // КРОК 5: Вмикаємо неблокуючий (асинхронний) режим.
    // Коли клієнт стукає (FD_ACCEPT), Windows надішле вікну hWnd повідомлення WSA_ACCEPT
    if (WSAAsyncSelect(srv_socket, hWnd, WSA_ACCEPT, FD_ACCEPT))
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "WSAAsyncSelect error", "Error", MB_OK);
        return;
    }

    // Виводимо у текстове поле повідомлення про успішний старт
    strcpy_s(mess, sizeof(mess), "Сервер запущено\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ДОДАНА ФУНКЦІЯ 2: ЗУПИНКА СЕРВЕРА ---
void ServerStop(HWND hWnd)
{
    // Вимикаємо відстеження мережевих подій для головного сокета
    WSAAsyncSelect(srv_socket, hWnd, 0, 0);
    if (srv_socket != INVALID_SOCKET)
    {
        closesocket(srv_socket);     // Закриваємо головний сокет
        srv_socket = INVALID_SOCKET; // Позначаємо його як неактивний
    }
    // Також закриваємо з'єднання з усіма підключеними клієнтами
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sock[i] != INVALID_SOCKET)
        {
            closesocket(sock[i]);
            sock[i] = INVALID_SOCKET;
        }
    }
    strcpy_s(mess, sizeof(mess), "Сервер зупинено\r\n");
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ДОДАНА ФУНКЦІЯ 3: ПРИЙНЯТТЯ З'ЄДНАННЯ ВІД КОНКРЕТНОГО КЛІЄНТА ---
BOOL AcceptClient(int j)
{
    int sockaddr_len = sizeof(client_addr[j]);
    // Функція accept() приймає клієнта і створює для нього окремий робочий сокет sock[j]
    sock[j] = accept(srv_socket, (LPSOCKADDR)&client_addr[j], &sockaddr_len);
    if (sock[j] != INVALID_SOCKET)
    {
        // Налаштовуємо асинхронне отримання даних (FD_READ) та відключення (FD_CLOSE) для клієнта j
        if (!WSAAsyncSelect(sock[j], hWindow, WSA_NETEVENT, FD_READ | FD_CLOSE))
            return TRUE;
        closesocket(sock[j]);
        sock[j] = INVALID_SOCKET;
    }
    return FALSE;
}

// --- ДОДАНА ФУНКЦІЯ 4: ЗАКРИТТЯ СОКЕТА КЛІЄНТА ---
void DisconnectClient(int j)
{
    if (sock[j] != INVALID_SOCKET)
    {
        WSAAsyncSelect(sock[j], hWindow, 0, 0); // Скасовуємо отримання подій від цього клієнта
        closesocket(sock[j]);                   // Закриваємо сокет
        sock[j] = INVALID_SOCKET;               // Звільняємо слот у масиві для нових підключень!
    }
}

// --- ДОДАНА ФУНКЦІЯ 5: ОБРОБНИК ПОДІЇ ПІДКЛЮЧЕННЯ КЛІЄНТА (WSA_ACCEPT) ---
void WndProc_OnWSAAccept(HWND hWnd, LPARAM lParam)
{
    // Перевіряємо, чи не виникла помилка під час спроби підключення
    if (WSAGETSELECTERROR(lParam))
    {
        MessageBoxA(hWnd, "accept error", "Error", MB_OK);
        WSAAsyncSelect(srv_socket, hWnd, 0, 0);
        return;
    }

    // Шукаємо перше вільне місце у масиві сокетів sock[0..2]
    int freeSlot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sock[i] == INVALID_SOCKET)
        {
            freeSlot = i;
            break;
        }
    }

    // Якщо всі 3 місця зайняті — відхиляємо з'єднання
    if (freeSlot == -1)
    {
        SOCKET temp = accept(srv_socket, NULL, NULL);
        closesocket(temp);
        MessageBoxA(hWnd, "Число клієнтів > 3\r\n", "З'єднання неприпустимо!", MB_OK);
        return;
    }

    // Приймаємо клієнта у знайдений вільний слот
    if (!AcceptClient(freeSlot))
    {
        MessageBoxA(hWnd, "Помилка з'єднання з клієнтом", "Error", MB_OK);
        return;
    }

    // Формуємо рядок з IP-адресою (inet_ntoa) та портом (htons) клієнта і виводимо на екран
    sprintf_s(szBuf, sizeof(szBuf), "Доданий клієнт %i (IP=%s Port=%u)\r\n",
        freeSlot + 1,
        inet_ntoa(client_addr[freeSlot].sin_addr),
        htons(client_addr[freeSlot].sin_port));
    strcat_s(mess, sizeof(mess), szBuf);
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
}

// --- ДОДАНА ФУНКЦІЯ 6: ПРИЙОМ ДАНИХ ВІД КЛІЄНТІВ ТА ЇХ ВІДКЛЮЧЕННЯ (WSA_NETEVENT) ---
void WndProc_OnWSANetEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    char szTemp[512];
    int Number = -1;

    // Визначаємо, який саме з 3-х клієнтів надіслав подію (порівнюємо сокет із wParam)
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (sock[i] == (SOCKET)wParam)
        {
            Number = i;
            break;
        }
    }
    if (Number == -1) return;

    // Якщо подія FD_READ — клієнт надіслав нам дані!
    if (WSAGETSELECTEVENT(lParam) == FD_READ)
    {
        // Функція recv() зчитує байти з сокета клієнта у буфер szTemp
        int rc = recv((SOCKET)wParam, szTemp, sizeof(szTemp) - 1, 0);
        if (rc > 0)
        {
            szTemp[rc] = '\0'; // Ставимо символ кінця рядка після отриманих байтів
            char block[1024];
            sprintf_s(block, sizeof(block), "\r\nДані від Клієнта %i:\r\n%s", Number + 1, szTemp);
            strcat_s(mess, sizeof(mess), block); // Додаємо у загальний лог
            SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess); // Відображаємо у текстовому полі
        }
    }

    // Якщо подія FD_CLOSE — клієнт закрив своє вікно
    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
    {
        DisconnectClient(Number); // Закриваємо його сокет і звільняємо слот
        sprintf_s(szTemp, sizeof(szTemp), "Клієнт %i завершив роботу\r\n", Number + 1);
        strcat_s(mess, sizeof(mess), szTemp);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
}

// --- ДОДАНА ФУНКЦІЯ 7: ВІДПРАВКА ДАНИХ ВІД СЕРВЕРА КЛІЄНТУ ---
void SendToClient(int j)
{
    if (sock[j] == INVALID_SOCKET)
    {
        MessageBoxA(hWindow, "Цей клієнт зараз не підключений!", "Увага", MB_OK | MB_ICONWARNING);
        return;
    }

    // Зчитуємо текст, який зараз знаходиться у полі hwndEdit сервера
    cbWritten = SendMessageA(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (cbWritten >= sizeof(szBuf)) cbWritten = sizeof(szBuf) - 1;
    SendMessageA(hwndEdit, WM_GETTEXT, (WPARAM)(cbWritten + 1), (LPARAM)szBuf);
    szBuf[cbWritten] = '\0';

    // Функція send() відправляє байти рядка szBuf клієнту через сокет sock[j]
    if (send(sock[j], szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        char block[1200];
        sprintf_s(block, sizeof(block), "\r\nДані надіслані клієнту %d: %s\r\n", j + 1, szBuf);
        strcat_s(mess, sizeof(mess), block);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
    }
    else
    {
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Помилка надсилання повідомлення\r\n");
    }
}

// --- ДОДАНА ФУНКЦІЯ 8: ПРИМУСОВЕ ВІДКЛЮЧЕННЯ КЛІЄНТА СЕРВЕРОМ ---
void ClientOff(HWND hWnd, int j)
{
    if (sock[j] == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Цей клієнт вже відключений!", "Увага", MB_OK | MB_ICONINFORMATION);
        return;
    }

    sprintf_s(szBuf, sizeof(szBuf), "Відключити клієнта %i ?", j + 1);
    if (IDYES == MessageBoxA(hWnd, szBuf, "Question", MB_YESNO | MB_ICONQUESTION))
    {
        sprintf_s(szBuf, sizeof(szBuf), "Відключено клієнта %i\r\n", j + 1);
        send(sock[j], szBuf, (int)strlen(szBuf), 0); // Повідомляємо клієнта перед відключенням
        strcat_s(mess, sizeof(mess), szBuf);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        DisconnectClient(j); // Розриваємо з'єднання
    }
}

// --- ГОЛОВНА ФУНКЦІЯ ОБРОБКИ ПОВІДОМЛЕНЬ ВІКНА ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: // Виконується автоматично при старті вікна
        // Створюємо кнопки керування сервером (клас "BUTTON")
        CreateWindowA("BUTTON", "Старт сервера", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 5, 205, 28, hWnd, (HMENU)ID_SERVER_START, hInst, NULL);
        CreateWindowA("BUTTON", "Стоп сервера", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            215, 5, 205, 28, hWnd, (HMENU)ID_SERVER_STOP, hInst, NULL);

        CreateWindowA("BUTTON", "Надісл. Кл. 1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 38, 135, 28, hWnd, (HMENU)ID_SENDTOCLIENT_1, hInst, NULL);
        CreateWindowA("BUTTON", "Надісл. Кл. 2", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            145, 38, 135, 28, hWnd, (HMENU)ID_SENDTOCLIENT_2, hInst, NULL);
        CreateWindowA("BUTTON", "Надісл. Кл. 3 (*)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            285, 38, 135, 28, hWnd, (HMENU)ID_SENDTOCLIENT_3, hInst, NULL);

        CreateWindowA("BUTTON", "Відкл. Кл. 1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 71, 135, 28, hWnd, (HMENU)ID_CLIENTOFF_1, hInst, NULL);
        CreateWindowA("BUTTON", "Відкл. Кл. 2", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            145, 71, 135, 28, hWnd, (HMENU)ID_CLIENTOFF_2, hInst, NULL);
        CreateWindowA("BUTTON", "Відкл. Кл. 3 (*)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            285, 71, 135, 28, hWnd, (HMENU)ID_CLIENTOFF_3, hInst, NULL);

        // Створюємо текстове поле для виведення отриманих даних (клас "EDIT")
        hwndEdit = CreateWindowA("EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            5, 105, 415, 225, hWnd, NULL, hInst, NULL);

        // Ініціалізуємо бібліотеку сокетів Winsock у системі
        if (WSAStartup(wVersionRequested, &wsaData) != 0)
        {
            MessageBoxA(hWnd, "WSAStartup Error", "ERROR", MB_OK | MB_ICONSTOP);
            return FALSE;
        }
        // Виводимо інформацію про версію та статус Winsock
        sprintf_s(mess, sizeof(mess), "Використовується %s \r\nСтатус: %s\r\n",
            wsaData.szDescription, wsaData.szSystemStatus);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)mess);
        break;

    case WM_SIZE: // Масштабуємо текстове поле при зміні розміру вікна
        MoveWindow(hwndEdit, 5, 105, LOWORD(lParam) - 10, HIWORD(lParam) - 110, TRUE);
        break;

    case WM_COMMAND: // Обробка натискань на створені кнопки
        switch (LOWORD(wParam))
        {
        case ID_SERVER_START:   ServerStart(hWnd); break;
        case ID_SERVER_STOP:    ServerStop(hWnd); break;
        case ID_SENDTOCLIENT_1: SendToClient(0); break;
        case ID_SENDTOCLIENT_2: SendToClient(1); break;
        case ID_SENDTOCLIENT_3: SendToClient(2); break;
        case ID_CLIENTOFF_1:    ClientOff(hWnd, 0); break;
        case ID_CLIENTOFF_2:    ClientOff(hWnd, 1); break;
        case ID_CLIENTOFF_3:    ClientOff(hWnd, 2); break;
        }
        break;

    case WSA_ACCEPT: // Прийшов сигнал про підключення нового клієнта
        WndProc_OnWSAAccept(hWnd, lParam);
        break;

    case WSA_NETEVENT: // Прийшов сигнал про дані від клієнта або його відключення
        WndProc_OnWSANetEvent(hWnd, wParam, lParam);
        break;

    case WM_DESTROY: // При закритті вікна вивільняємо ресурси Winsock
        WSACleanup();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}