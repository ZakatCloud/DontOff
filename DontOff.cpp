#include <iostream>
#include <windows.h>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <sstream>
#include <powrprof.h> 

#pragma comment(lib, "PowrProf.lib")

std::string active_scheme_guid;

std::string ExecuteCommandAndGetOutput(const std::string& command)
{
    std::string result = "";
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

void ExecuteCommand(const std::string& command)
{
    system(("cmd /c " + command + " > nul 2>&1").c_str());
}

void SetupLidCloseOverride()
{
    std::cout << "Retrieving active power scheme GUID via powercfg..." << std::endl;
    std::string output = ExecuteCommandAndGetOutput("powercfg -getactivescheme");


    size_t start = output.find("GUID: ");
    if (start != std::string::npos)
    {
        start += 6; 
        active_scheme_guid = output.substr(start, 36); 
        std::cout << "Active Scheme GUID: " << active_scheme_guid << std::endl;
    }
    else
    {
        active_scheme_guid = "{381B4222-F694-41F0-9685-FF5BB260DF2E}";
        std::cerr << "Warning: Failed to parse active power scheme GUID. Using default Balanced GUID." << std::endl;
    }


    std::string setting_guid = "5CA83367-6E16-4B55-A512-9DEE90FCE8D7"; 
    std::string subgroup_guid = "4F971E89-EBDC-440C-864B-FBCF2C967F1E"; 

    std::cout << "Setting lid close action to 'Do Nothing' (AC power)..." << std::endl;
    std::string command = "powercfg /SETACVALUEINDEX " + active_scheme_guid + " " + subgroup_guid + " " + setting_guid + " 0";
    ExecuteCommand(command);

    command = "powercfg /SETACTIVE " + active_scheme_guid;
    ExecuteCommand(command);
}

void RestoreLidCloseOverride()
{
    std::cout << "Releasing power requests for HardKeepAwake.exe..." << std::endl;
    ExecuteCommand("powercfg /REQUESTS /DELETE /SYSTEM \"HardKeepAwake.exe\"");
    ExecuteCommand("powercfg /REQUESTS /DELETE /DISPLAY \"HardKeepAwake.exe\"");

    std::string setting_guid = "5CA83367-6E16-4B55-A512-9DEE90FCE8D7";
    std::string subgroup_guid = "4F971E89-EBDC-440C-864B-FBCF2C967F1E";

    std::cout << "Restoring lid close action to 'Sleep' (AC power)..." << std::endl;
    std::string command = "powercfg /SETACVALUEINDEX " + active_scheme_guid + " " + subgroup_guid + " " + setting_guid + " 1";
    ExecuteCommand(command);

    command = "powercfg /SETACTIVE " + active_scheme_guid;
    ExecuteCommand(command);

    SetThreadExecutionState(ES_CONTINUOUS);
}

BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    if (dwType == CTRL_C_EVENT || dwType == CTRL_CLOSE_EVENT || dwType == CTRL_BREAK_EVENT)
    {
        RestoreLidCloseOverride();
        std::cout << "\nSystem Keeper terminated. Default power settings RESTORED." << std::endl;
        return TRUE; 
    }
    return FALSE;
}


int main()
{
    SetConsoleTitleA("System Keeper: Running - ADMIN REQUIRED");

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        std::cerr << "Error: Could not set control handler. Settings restoration may fail!" << std::endl;
    }

    SetupLidCloseOverride();

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

    while (true)
    {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

        ExecuteCommand("powercfg /REQUESTS /OVERRIDE /SYSTEM \"HardKeepAwake.exe\" System");

        std::this_thread::sleep_for(std::chrono::seconds(120));
    }

    RestoreLidCloseOverride();
    return 0;
}