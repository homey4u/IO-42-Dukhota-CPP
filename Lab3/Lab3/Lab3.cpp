#include <iostream>
#include <vector>
#include <windows.h>
#include <ctime>

// Глобальні об'єкти для синхронізації
HANDLE hMutex;
std::vector<int> buffer;
const size_t N = 5;

int currentNumber = 0;
double currentAverage = 0.0;
bool isRunning = true;

// Змінна для визначення строгої черги: 0 - Генератор, 1 - Обчислювач, 2 - Виведення
int nextAction = 0;

// Потік 1: Генерує числа
DWORD WINAPI GeneratorThread(LPVOID lpParam) {
    srand(static_cast<unsigned>(time(nullptr)));

    while (isRunning) {
        WaitForSingleObject(hMutex, INFINITE); // Блокуємо доступ

        // Перевіряємо, чи зараз наша черга
        if (nextAction == 0) {
            int num = rand() % 100;
            buffer.push_back(num);
            currentNumber = num;

            if (buffer.size() > N) {
                buffer.erase(buffer.begin());
            }

            nextAction = 1; // Передаємо естафету Обчислювачу
        }

        ReleaseMutex(hMutex); // Звільняємо м'ютекс
        Sleep(10); // Маленька затримка, щоб не вантажити процесор на 100%
    }
    return 0;
}

// Потік 2: Обчислює середнє
DWORD WINAPI CalculatorThread(LPVOID lpParam) {
    while (isRunning) {
        WaitForSingleObject(hMutex, INFINITE);

        if (nextAction == 1) { // Якщо наша черга
            if (!buffer.empty()) {
                double sum = 0.0;
                for (int val : buffer) {
                    sum += val;
                }
                currentAverage = sum / buffer.size();
            }

            nextAction = 2; // Передаємо естафету Виведенню
        }

        ReleaseMutex(hMutex);
        Sleep(10);
    }
    return 0;
}

// Потік 3: Виводить на екран
DWORD WINAPI DisplayThread(LPVOID lpParam) {
    while (isRunning) {
        WaitForSingleObject(hMutex, INFINITE);

        if (nextAction == 2) { // Якщо наша черга
            std::cout << "[Потік 3] Поточне згенероване число: " << currentNumber
                << " | Середнє останніх " << buffer.size() << " чисел: "
                << currentAverage << std::endl;

            nextAction = 0; // Передаємо естафету назад Генератору

            ReleaseMutex(hMutex);
            Sleep(1000); // Робимо паузу 1 секунду між циклами, щоб зручно читати
        }
        else {
            ReleaseMutex(hMutex);
            Sleep(10);
        }
    }
    return 0;
}

int main() {
    // Налаштування правильного кодування для української мови в консолі Windows
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    std::cout << "Програма запущена. Натисніть клавішу ENTER для її завершення...\n\n";

    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
        std::cerr << "Помилка створення м'ютекса!\n";
        return 1;
    }

    HANDLE hThreads[3];
    hThreads[0] = CreateThread(NULL, 0, GeneratorThread, NULL, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, CalculatorThread, NULL, 0, NULL);
    hThreads[2] = CreateThread(NULL, 0, DisplayThread, NULL, 0, NULL);

    std::cin.get();

    std::cout << "\nЗавершення роботи потоків...\n";
    isRunning = false;

    WaitForMultipleObjects(3, hThreads, TRUE, INFINITE);

    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);
    CloseHandle(hThreads[2]);
    CloseHandle(hMutex);

    std::cout << "Всі потоки успішно завершені. Програма завершує роботу.\n";

    return 0;
}