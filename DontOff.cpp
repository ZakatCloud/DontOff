#include <iostream>
#include <windows.h>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <sstream>
#include <powrprof.h> 

#pragma comment(lib, "PowrProf.lib")

// Глобальные переменные для сохранения и восстановления настроек
std::string active_scheme_guid;

// Функция для выполнения команд через cmd.exe и возврата вывода
std::string ExecuteCommandAndGetOutput(const std::string& command)
{
    std::string result = "";
    // Используем popen для выполнения команды и захвата вывода
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) return "";

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        result += buffer;
    }
    _pclose(pipe);
    return result;
}

// Функция для выполнения команд через cmd.exe без возврата вывода
void ExecuteCommand(const std::string& command)
{
    // Используем system для скрытого запуска команд, которые не требуют анализа вывода
    system(("cmd /c " + command + " > nul 2>&1").c_str());
}

// 1. Сохраняет текущее действие при закрытии крышки и устанавливает "Ничего не делать"
void SetupLidCloseOverride()
{
    // **НОВОЕ НАДЕЖНОЕ ПОЛУЧЕНИЕ GUID АКТИВНОЙ СХЕМЫ**
    std::cout << "Retrieving active power scheme GUID via powercfg..." << std::endl;
    std::string output = ExecuteCommandAndGetOutput("powercfg -getactivescheme");

    // Вывод будет выглядеть примерно так: "Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced)"

    size_t start = output.find("GUID: ");
    if (start != std::string::npos)
    {
        start += 6; // Смещаемся после "GUID: "
        active_scheme_guid = output.substr(start, 36); // GUID всегда 36 символов
        std::cout << "Active Scheme GUID: " << active_scheme_guid << std::endl;
    }
    else
    {
        // GUID Сбалансированной схемы по умолчанию
        active_scheme_guid = "{381B4222-F694-41F0-9685-FF5BB260DF2E}";
        std::cerr << "Warning: Failed to parse active power scheme GUID. Using default Balanced GUID." << std::endl;
    }


    // Параметр действия при закрытии крышки (питание от сети - AC)
    std::string setting_guid = "5CA83367-6E16-4B55-A512-9DEE90FCE8D7"; // Действие при закрытии крышки
    std::string subgroup_guid = "4F971E89-EBDC-440C-864B-FBCF2C967F1E"; // Кнопки питания и крышка

    // Установка "Ничего не делать" (0)
    std::cout << "Setting lid close action to 'Do Nothing' (AC power)..." << std::endl;
    std::string command = "powercfg /SETACVALUEINDEX " + active_scheme_guid + " " + subgroup_guid + " " + setting_guid + " 0";
    ExecuteCommand(command);

    // Активируем измененную схему, чтобы изменения вступили в силу немедленно
    command = "powercfg /SETACTIVE " + active_scheme_guid;
    ExecuteCommand(command);
}

// 2. Восстанавливает предыдущие настройки системы
void RestoreLidCloseOverride()
{
    // Снимаем запрос на непрерывную работу, используя имя файла
    std::cout << "Releasing power requests for HardKeepAwake.exe..." << std::endl;
    ExecuteCommand("powercfg /REQUESTS /DELETE /SYSTEM \"HardKeepAwake.exe\"");
    ExecuteCommand("powercfg /REQUESTS /DELETE /DISPLAY \"HardKeepAwake.exe\"");

    // Восстановление действия "Сон" (1) при закрытии крышки (питание от сети - AC)
    std::string setting_guid = "5CA83367-6E16-4B55-A512-9DEE90FCE8D7";
    std::string subgroup_guid = "4F971E89-EBDC-440C-864B-FBCF2C967F1E";

    std::cout << "Restoring lid close action to 'Sleep' (AC power)..." << std::endl;
    std::string command = "powercfg /SETACVALUEINDEX " + active_scheme_guid + " " + subgroup_guid + " " + setting_guid + " 1";
    ExecuteCommand(command);

    // Активируем измененную схему
    command = "powercfg /SETACTIVE " + active_scheme_guid;
    ExecuteCommand(command);

    // Восстановление предыдущего состояния ES (если оно было отлично от текущего)
    SetThreadExecutionState(ES_CONTINUOUS);
}

// Функция-обработчик для перехвата Ctrl+C и закрытия окна
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    if (dwType == CTRL_C_EVENT || dwType == CTRL_CLOSE_EVENT || dwType == CTRL_BREAK_EVENT)
    {
        RestoreLidCloseOverride();
        std::cout << "\nSystem Keeper terminated. Default power settings RESTORED." << std::endl;
        return TRUE; // Сообщаем, что мы обработали событие
    }
    return FALSE;
}


int main()
{
    SetConsoleTitleA("System Keeper: Running - ADMIN REQUIRED");

    // Установка обработчика для гарантированного восстановления настроек при закрытии
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        std::cerr << "Error: Could not set control handler. Settings restoration may fail!" << std::endl;
    }

    // 1. Применяем настройки "Ничего не делать" при закрытии крышки
    SetupLidCloseOverride();

    // 2. Создаем постоянный запрос на работу системы через SetThreadExecutionState
    EXECUTION_STATE previousState = SetThreadExecutionState(
        ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED
    );

    if (previousState == 0)
    {
        std::cerr << "Error: Failed to set execution state." << std::endl;
        RestoreLidCloseOverride();
        return 1;
    }

    std::cout << "\n-------------------------------------------------" << std::endl;
    std::cout << "System Keeper is now **FULLY ACTIVE**." << std::endl;
    std::cout << "Lid close action is temporarily set to 'Do Nothing'." << std::endl;
    std::cout << "Your bot will continue working when the lid is closed." << std::endl;
    std::cout << "-------------------------------------------------" << std::endl;
    std::cout << "Press Ctrl+C to terminate and RESTORE default power settings." << std::endl;

    // 3. Бесконечный цикл для поддержания запросов
    while (true)
    {
        // Повторный вызов SetThreadExecutionState для подтверждения
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

        // Создаем явный Power Request (отображается в powercfg /REQUESTS)
        ExecuteCommand("powercfg /REQUESTS /OVERRIDE /SYSTEM \"HardKeepAwake.exe\" System");

        // Пауза 2 минуты (120 секунд)
        std::this_thread::sleep_for(std::chrono::seconds(120));
    }

    RestoreLidCloseOverride();
    return 0;
}