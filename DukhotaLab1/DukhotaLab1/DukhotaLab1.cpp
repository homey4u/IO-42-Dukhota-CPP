// DukhotaLab1.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DukhotaLab1.h"
#include <string>
using namespace std;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


// Точка входу — wWinMain
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DUKHOTALAB1, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DUKHOTALAB1));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
// Ця функція описує шаблон (клас) майбутнього вікна
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT; // CS_BYTEALIGNCLIENT - вирівнювання клієнтської області по горизонталі, CS_HREDRAW - оновлення вікна при зміні горизонтального розміру, CS_VREDRAW - оновлення вікна при зміні вертикального розміру
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(nullptr, IDI_QUESTION); // системний знак питання
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW); // стандартна стрілка
    wcex.hbrBackground  = (HBRUSH)CreateSolidBrush(RGB(128, 128,128)); // Колір фону вікна - сірий
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DUKHOTALAB1);
    wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm = LoadIcon(nullptr, IDI_QUESTION); // системний знак питання

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(
	   szWindowClass, // ім'я класу вікна
	   szTitle, // заголовок вікна
	   WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL, // стиль вікна - звичайне вікно з горизонтальною та вертикальною смугами прокрутки
	   GetSystemMetrics(SM_CXSCREEN) - 450, // Дізнаємось ширину монітора і віднімаємо ширину нашого вікна (450) (зліва на право)
	   0, // висота вікна - 0, тобто вікно буде розташоване у верхньому лівому куті екрану
	   450, // ширина вікна 
	   440, // висота вікна
	   nullptr, // no menu
	   nullptr, // no parent window
       hInstance, 
	   nullptr  // no additional parameters
   );

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, SW_SHOWMINIMIZED); // SW_SHOWMINIMIZED - вікно буде відображене у згорнутому стані
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
				return DefWindowProcW(hWnd, message, wParam, lParam); // якщо ми не обробили повідомлення, то передаємо його стандартній процедурі обробки повідомлень
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            // 1. Отримуємо розміри прямокутника для подвійного кліку
            int DoubleClickX = GetSystemMetrics(SM_CXDOUBLECLK);
            int DoubleClickY = GetSystemMetrics(SM_CYDOUBLECLK);

            // 2. Отримуємо відносну ширину точки екрана
            int PixelWidth = GetDeviceCaps(hdc, ASPECTX);

            // 3. Формуємо текст для виведення. 
            // Буква L перед текстом потрібна, бо Win32 використовує широкі символи (Unicode).
            // std::to_wstring перетворює наші числа в текст, щоб їх можна було надрукувати.
            wstring Text = L"Прямокутник DoubleCl: по X " + to_wstring(DoubleClickX) + L" по Y " + to_wstring(DoubleClickY) + L"\n"
                L"Відносна ширина пікселю (точки): " + to_wstring(PixelWidth);

            // 4. Дізнаємося розмір нашого вікна, щоб знати, де малювати текст
            RECT DrawRect;
            GetClientRect(hWnd, &DrawRect);

            // 5. Друкуємо текст на екрані. 
            DrawTextW(
                hdc,  // Наш "допуск + полотно"
                Text.c_str(), 
                -1,
                &DrawRect,
                DT_CENTER | DT_TOP
            );

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
		return DefWindowProcW(hWnd, message, wParam, lParam); // Використовуємо DefWindowProcW (з літерою W), щоб система правильно обробляла широкі символи (Unicode)
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
