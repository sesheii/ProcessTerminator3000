#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <set>
#include <thread>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef __linux__
//linux code goes here
#elif _WIN32
// windows code goes here
#else

#endif


class ProcessClass {
public:
    DWORD pid;
    DWORD upper_memory_limit = 0;
    DWORD lower_memory_limit = MAXDWORD;

    ProcessClass(DWORD pid_) : pid(pid_){
    }

    ProcessClass() {
        this->pid = 0;
    }

    ProcessClass(DWORD pid_, DWORD lower, DWORD upper) : pid(pid_), lower_memory_limit(lower), upper_memory_limit(upper) {
    }

    DWORD GetPid() {
        return pid;
    }

    void SetLowerMemoryLimit(DWORD lower) {
        this->lower_memory_limit = lower;
    }

    void SetUpperMemoryLimit(DWORD upper) {
        this->upper_memory_limit = upper;
    }

    void SetMemoryLimits(DWORD lower, DWORD upper) {
        this->lower_memory_limit = lower;
        this->upper_memory_limit = upper;
    }

    bool isActive() {
        HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid);
        if (handle == NULL)
            return 0;

        DWORD exitcode = 0;
        GetExitCodeProcess(handle, &exitcode);
        CloseHandle(handle);
        return exitcode == STILL_ACTIVE;
    }

    DWORD GetMemoryUsage() {
        HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid);
        if (handle == NULL)
            return 0;
        PROCESS_MEMORY_COUNTERS pmc;
        DWORD memory;
        if (GetProcessMemoryInfo(handle, &pmc, sizeof(pmc)) == NULL)
            return 0;
        memory = pmc.WorkingSetSize / (1024 * 1024); // шоб в мегабайтах а не в байтах
        CloseHandle(handle);
        return memory;
    }

    std::wstring GetName(DWORD pid) {
        HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (handle == NULL)
            return L"";

        std::wstring Name(MAX_PATH, L'\0');
        DWORD length = K32GetModuleBaseNameW(handle, NULL, &Name[0], MAX_PATH);
        CloseHandle(handle);

        if (length == 0)
            return L"";
        else
            return Name.substr(0, length); // Only keep the characters up to the actual length of the name
    }


    void Terminate() {
        HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
        if (handle == NULL)
            return;
        TerminateProcess(handle, 0);
        CloseHandle(handle);
    }

    bool operator<(const ProcessClass& other) const {
        return pid < other.pid;
    }

    bool operator==(const ProcessClass& other) const {
        return pid == other.pid;
    }

    friend std::wostream& operator<<(std::wostream& os, ProcessClass& pc) {
        os << L"PID:\t" << pc.GetPid() << L"\tNAME:\t" << pc.GetName(pc.pid) << L" | MEM: " << pc.GetMemoryUsage() << L" MB";
        return os;
    }

    std::wstring to_wstring() {
        std::wstringstream ws;
        ws << L"PID:\t" << GetPid() << L"\tNAME:\t" << GetName(this->pid) << L"  | MEM: " << GetMemoryUsage() << L" MB";
        return ws.str();
    }

    static bool sortbysec(const std::pair<DWORD, ProcessClass>& a,
                          const std::pair<DWORD, ProcessClass>& b)
    {
        return (a.first < b.first);
    }


    static std::vector<ProcessClass> GetProcessesList() {
        DWORD pList[1024], pListBytesSize;
        std::vector<ProcessClass> res;
        std::vector<std::pair<DWORD, ProcessClass>> pVector;
        if (!EnumProcesses(pList, sizeof(pList), &pListBytesSize))
            return res;
        DWORD pListSize = pListBytesSize / sizeof(DWORD);
        for (DWORD i = 0; i < pListSize; ++i) {
            DWORD pid = pList[i];
            ProcessClass process(pid);
            if (process.isActive()) {
                pVector.push_back({ process.GetMemoryUsage(), process });
            }
        }

        std::sort(pVector.begin(), pVector.end(), sortbysec);


        for (const auto& i : pVector) {
            res.emplace_back(i.second);
        }
        return res;
    }

    static std::set<DWORD> GetProcessIDs() {
        DWORD pList[1024], pListBytesSize;
        std::set<DWORD> pVector;
        if (!EnumProcesses(pList, sizeof(pList), &pListBytesSize))
            return pVector;
        DWORD pListSize = pListBytesSize / sizeof(DWORD);
        for (DWORD i = 0; i < pListSize; ++i) {
            DWORD pid = pList[i];
            pVector.insert(pid);
        }
    }

};

void PrintProcessesList(std::vector<ProcessClass>& pList) {
    for (auto i : pList) {
        std::wcout << i.to_wstring() << L"\n";
    }
}

DWORD MonitorProcess(ProcessClass& process) {
    if (!process.isActive())
        return process.GetPid();
    DWORD memory = process.GetMemoryUsage();
    DWORD pid = process.GetPid();
    if (memory < process.lower_memory_limit || memory > process.upper_memory_limit) {
        process.Terminate();
        return pid;
    }
    return 0;
}


// опа, багатопоточечка

bool RUNNING = true;

std::unordered_map<DWORD, ProcessClass> monitoredProcesses;

void MonitorProcesses() {
    while (RUNNING) {
        std::vector<DWORD> toBeRemoved;
        for (auto& pair : monitoredProcesses) {
            auto& process = pair.second;
            DWORD exitcode = MonitorProcess(process);

            if (exitcode) {
                toBeRemoved.push_back(exitcode);
            }
        }

        for (auto& pid : toBeRemoved) {
            monitoredProcesses.erase(pid);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}




void PrintCMD() {
    std::vector<std::wstring> text = {
            L"---**COMMANDS**---",
            L"add PID LOWER(MB) UPPER(MB)   |   start monitoring a process. LOWER and UPPER - memory limits",
            L"setupper PID MEM(MB)          |   set upper memory limit of a monitored process",
            L"setlower PID MEM(MB)          |   set lower memory limit of a monitored process",
            L"del PID                       |   stop monitoring a certain monitored process",
            L"info PID                      |   print pid, name and memory usage of a certain monitored process",
            L"infoall                       |   print pid, name and memory usage of all monitored processes",
            L"list                          |   just print a list of all processes",
            L"cmdlist                       |   just print a list of all commands (what you see rn)",
            L"exit                          |   exit the program"
    };

    std::wcout << L"\n";

    for (auto const& i : text) {
        std::wcout << L"\t|" + i + L"\n";
    }
}


void HandleCommands() {
    std::wstring input;

    while (RUNNING) {
        std::getline(std::wcin, input);
        std::wistringstream stream(input);
        std::wstring cmd;
        stream >> cmd;
        std::wcout << L"\n>";
        if (L"add" == cmd) {
            DWORD pid, lower, upper;
            if (!(stream >> pid >> lower >> upper)) {
                std::wcerr << L"Error: Incomplete 'add' command. Usage: add <pid> <lower> <upper>\n";
                continue;
            }
            auto id = ProcessClass::GetProcessIDs();
            if (id.find(pid) == id.end()) {
                std::wcout << L"Process " << pid << L" not found.\n";
                continue;
            }

            ProcessClass process(pid, lower, upper);

            if (monitoredProcesses.find(pid) != monitoredProcesses.end()) {
                std::wcout << L"Process " << pid << L" is already being monitored.\n";
            }
            else {
                monitoredProcesses.insert({ pid, process });
                std::wcout << L"Process " << pid << L" is now being monitored.\n";
            }

        }
        else if (L"setupper" == cmd) {
            int pid, upper;
            if (!(stream >> pid >> upper)) {
                std::wcerr << L"Error: Incomplete 'setupper' command. Usage: setupper <pid> <upper>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::wcout << L"PID is not being monitored.\n";
            }
            else {
                monitoredProcesses[pid].upper_memory_limit = upper;
                std::wcout << L"Upper memory limit for process " << pid << L" has been set to " << upper << L"\n";
            }

        }
        else if (L"setlower" == cmd) {
            DWORD pid, lower;
            if (!(stream >> pid >> lower)) {
                std::wcerr << L"Error: Incomplete 'setlower' command. Usage: setlower <pid> <lower>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::wcout << L"PID is not being monitored.\n";
            }
            else {
                monitoredProcesses[pid].lower_memory_limit = lower;
                std::wcout << L"Lower memory limit for process " << pid << L" has been set to " << lower << L"\n";
            }

        }
        else if (L"del" == cmd) {
            DWORD pid;
            if (!(stream >> pid)) {
                std::wcerr << L"Error: Incomplete 'del' command. Usage: del <pid>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::wcout << L"PID is not being monitored.\n";
            }
            else {
                monitoredProcesses.erase(pid);
                std::wcout << L"Process " << pid << L" is no longer being monitored\n";
            }

        }
        else if (L"info" == cmd) {
            DWORD pid;
            if (!(stream >> pid)) {
                std::wcerr << L"Error: Incomplete 'info' command. Usage: info <pid>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::wcout << L"PID is not being monitored.\n";
            }
            else {
                std::wcout << monitoredProcesses[pid] << L"\n";
            }

        }
        else if (L"infoall" == cmd) {
            if (monitoredProcesses.empty()) {
                std::wcout << L"No processes are being monitored right now. use add <pid> <lower> <upper>\n";
            }
            for (auto i : monitoredProcesses) {
                std::wcout << i.second << L"\n";
            }

        }
        else if (L"list" == cmd) {
            auto list = ProcessClass::GetProcessesList();
            PrintProcessesList(list);
            std::wcout << L"\n>";

        }
        else if (L"cmdlist" == cmd) {
            std::wcout << L"\n";
            PrintCMD();
            std::wcout << L"\n";
            std::wcout << L"\n>";

        }
        else if (L"exit" == cmd) {
            RUNNING = false;
        }
        else {
            std::wcerr << L"Invalid command.\n";

        }
    }
}



int main()
{

    auto pl = ProcessClass::GetProcessesList();
    PrintProcessesList(pl);
    PrintCMD();
    std::wcout << L"\n>";

    std::thread monitor_thread(MonitorProcesses);
    std::thread command_thread(HandleCommands);

    monitor_thread.join();
    command_thread.join();
}
