#include <iostream>
#include <vector>
#include <set>
#include <thread>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <limits>

class ProcessClass {
public:

    int pid;
    int upper_memory_limit = std::numeric_limits<int>::max();
    int lower_memory_limit = 0;

    ProcessClass(int pid_) : pid(pid_){
    }

    ProcessClass() {
        this->pid = 0;
    }

    ProcessClass(int pid_, int lower, int upper) : pid(pid_), lower_memory_limit(lower), upper_memory_limit(upper) {
    }

    int GetPid() {
        return pid;
    }

    void SetLowerMemoryLimit(int lower) {
        this->lower_memory_limit = lower;
    }

    void SetUpperMemoryLimit(int upper) {
        this->upper_memory_limit = upper;
    }

    void SetMemoryLimits(int lower, int upper) {
        this->lower_memory_limit = lower;
        this->upper_memory_limit = upper;
    }

    std::set<int> GetAllSystemPIDs();
    bool isAccessible();
    int GetMemoryUsage();
    std::string GetName(int pid);
    void Terminate();

    bool operator<(const ProcessClass& other) const {
        return pid < other.pid;
    }

    bool operator==(const ProcessClass& other) const {
        return pid == other.pid;
    }

    int GetMemoryUsage(int pid);

    std::string GetName();
};

std::unordered_map<int, ProcessClass> monitoredProcesses;
bool RUNNING = true;

#ifdef __linux__


#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>

std::set<int> ProcessClass::GetAllSystemPIDs() {
    DIR* dir;
    struct dirent* ent;
    std::set<int> pids;

    if ((dir = opendir("/proc")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            if (std::regex_match(ent->d_name, std::regex("[0-9]+"))) {
                pids.insert(std::stoi(ent->d_name));
            }
        }
        closedir(dir);
    } else {
        std::perror("Could not open /proc");
    }

    return pids;
}

bool ProcessClass::isAccessible() {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    return (access(path.c_str(), R_OK) != -1);
}

int ProcessClass::GetMemoryUsage() {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    std::string line;
    int mem = 0;
    while (std::getline(file, line)) {
        if (line.find("VmRSS:") != std::string::npos) {
            std::istringstream iss(line);
            std::string tmp;
            iss >> tmp >> mem;
            break;
        }
    }
    return mem / 1024;  // Convert from KB to MB
}

int ProcessClass::GetMemoryUsage(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    std::string line;
    int mem = 0;
    while (std::getline(file, line)) {
        if (line.find("VmRSS:") != std::string::npos) {
            std::istringstream iss(line);
            std::string tmp;
            iss >> tmp >> mem;
            break;
        }
    }
    return mem / 1024;  // Convert from KB to MB
}

std::string ProcessClass::GetName() {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream file(path);
    std::string name;
    std::getline(file, name);
    file.close();
    return name;
}

std::string ProcessClass::GetName(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream file(path);
    std::string name;
    std::getline(file, name);
    file.close();
    return name;
}

void ProcessClass::Terminate() {
    kill(pid, SIGTERM); // Sends the terminate signal to the process
}



#elif _WIN32

#include <windows.h>
#include <psapi.h>


std::set<int> ProcessClass::GetAllSystemPIDs() {
    int pList[1024], pListBytesSize;
    std::set<int> pVector;
    if (!EnumProcesses(reinterpret_cast<DWORD *>(pList), sizeof(pList), reinterpret_cast<DWORD *>(&pListBytesSize)))
        return pVector;
    int pListSize = pListBytesSize / sizeof(int);
    for (int i = 0; i < pListSize; ++i) {
        int pid = pList[i];
        pVector.insert(pid);
    }
    return pVector;
}

bool ProcessClass::isAccessible() {
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid);
    if (handle == nullptr)
        return false;

    int exitcode = 0;
    GetExitCodeProcess(handle, reinterpret_cast<LPDWORD>(&exitcode));
    CloseHandle(handle);
    return exitcode == STILL_ACTIVE;
}

int ProcessClass::GetMemoryUsage() {
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, this->pid);
    if (handle == nullptr)
        return 0;
    PROCESS_MEMORY_COUNTERS pmc;
    int memory;
    if (GetProcessMemoryInfo(handle, &pmc, sizeof(pmc)) == NULL)
        return 0;
    memory = pmc.WorkingSetSize / (1024 * 1024); // шоб в мегабайтах а не в байтах
    CloseHandle(handle);
    return memory;
}

int ProcessClass::GetMemoryUsage(int pid) {
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid);
    if (handle == nullptr)
        return 0;
    PROCESS_MEMORY_COUNTERS pmc;
    int memory;
    if (GetProcessMemoryInfo(handle, &pmc, sizeof(pmc)) == NULL)
        return 0;
    memory = pmc.WorkingSetSize / (1024 * 1024); // шоб в мегабайтах а не в байтах
    CloseHandle(handle);
    return memory;
}

std::string ProcessClass::GetName() {
    std::string name = "UNKNOWN";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->pid);
    if (hProcess != nullptr) {
        char buffer[MAX_PATH] = { 0 };
        if (GetProcessImageFileNameA(hProcess, buffer, MAX_PATH)) {
            std::string fullPath(buffer);

            size_t pos = fullPath.find_last_of("\\");
            if (pos != std::string::npos) {
                name = fullPath.substr(pos + 1);
            }
        }
        CloseHandle(hProcess);
    }
    return name;
}

std::string ProcessClass::GetName(int pid) {
    std::string name = "UNKNOWN";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != nullptr) {
        char buffer[MAX_PATH] = { 0 };
        if (GetProcessImageFileNameA(hProcess, buffer, MAX_PATH)) {
            std::string fullPath(buffer);

            size_t pos = fullPath.find_last_of("\\");
            if (pos != std::string::npos) {
                name = fullPath.substr(pos + 1);
            }
        }
        CloseHandle(hProcess);
    }
    return name;
}

void ProcessClass::Terminate() {
    HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
    if (handle == nullptr)
        return;
    TerminateProcess(handle, 0);
    CloseHandle(handle);
}


#else

#endif


void PrintCMD() {
    std::vector<std::string> text = {
            "---**COMMANDS**---",
            "add PID LOWER(MB) UPPER(MB)   |   start monitoring a process. LOWER and UPPER - memory limits",
            "setupper PID MEM(MB)          |   set upper memory limit of a monitored process",
            "setlower PID MEM(MB)          |   set lower memory limit of a monitored process",
            "del PID                       |   stop monitoring a certain monitored process",
            "info PID                      |   print pid, name and memory usage of a certain monitored process",
            "infoall                       |   print pid, name and memory usage of all monitored processes",
            "list                          |   just print a list of all processes",
            "cmdlist                       |   just print a list of all commands (what you see rn)",
            "exit                          |   exit the program"
    };

    std::cout << "\n";

    for (auto const& i : text) {
        std::cout << "\t|" + i + "\n";
    }
}

void PrintProcessesList() {
    ProcessClass temp;
    std::string tempname;
    int tempmemory;
    std::vector<std::pair<int, std::string>> processlist;

    for (auto i : temp.GetAllSystemPIDs()) {
        tempname = temp.GetName(i);
        tempmemory = temp.GetMemoryUsage(i);
        if (tempname != "UNKNOWN" && tempmemory != 0) {
            processlist.push_back({tempmemory, std::to_string(i) + '\t' + temp.GetName(i) + " | MEM: " + std::to_string(temp.GetMemoryUsage(i)) + " MB\n"});
        }
    }

    std::sort(processlist.begin(), processlist.end());

    for (auto i : processlist) {
        std::cout << i.second;
    }
}

void HandleCommands() {
    std::string input;

    while (RUNNING) {
        std::cout << "\n>";
        std::getline(std::cin, input);
        std::istringstream stream(input);
        std::string cmd;
        stream >> cmd;

        if ("add" == cmd) {
            int pid, lower, upper;
            if (!(stream >> pid >> lower >> upper)) {
                std::cerr << "Error: Incomplete 'add' command. Usage: add <pid> <lower> <upper>\n";
                continue;
            }
            ProcessClass temp;
            auto id = temp.GetAllSystemPIDs();
            if (id.find(pid) == id.end()) {
                std::cout << "Process " << pid << " not found.\n";
                continue;
            }

            ProcessClass process(pid, lower, upper);

            if (monitoredProcesses.find(pid) != monitoredProcesses.end()) {
                std::cout << "Process " << pid << " is already being monitored.\n";
            }
            else {
                monitoredProcesses.insert({ pid, process });
                std::cout << "Process " << pid << " is now being monitored.\n";
            }

        }
        else if ("setupper" == cmd) {
            int pid, upper;
            if (!(stream >> pid >> upper)) {
                std::cerr << "Error: Incomplete 'setupper' command. Usage: setupper <pid> <upper>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::cout << "PID is not being monitored.\n";
            }
            else {
                monitoredProcesses[pid].upper_memory_limit = upper;
                std::cout << "Upper memory limit for process " << pid << " has been set to " << upper << "\n";
            }

        }
        else if ("setlower" == cmd) {
            int pid, lower;
            if (!(stream >> pid >> lower)) {
                std::cerr << "Error: Incomplete 'setlower' command. Usage: setlower <pid> <lower>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::cout << "PID is not being monitored.\n";
            }
            else {
                monitoredProcesses[pid].lower_memory_limit = lower;
                std::cout << "Lower memory limit for process " << pid << " has been set to " << lower << "\n";
            }

        }
        else if ("del" == cmd) {
            int pid;
            if (!(stream >> pid)) {
                std::cerr << "Error: Incomplete 'del' command. Usage: del <pid>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::cout << "PID is not being monitored.\n";
            }
            else {
                monitoredProcesses.erase(pid);
                std::cout << "Process " << pid << " is no longer being monitored\n";
            }

        }
        else if ("info" == cmd) {
            int pid;
            if (!(stream >> pid)) {
                std::cerr << "Error: Incomplete 'info' command. Usage: info <pid>\n";
                continue;
            }

            if (monitoredProcesses.find(pid) == monitoredProcesses.end()) {
                std::cout << "PID is not being monitored.\n";
            }
            else {
                ProcessClass temp;
                std::cout << std::to_string(pid) + '\t' + temp.GetName(pid)
                + " | MEM: " + std::to_string(temp.GetMemoryUsage(pid)) + " MB\t"
                + " | LOWER: " + std::to_string(monitoredProcesses[pid].lower_memory_limit) + '\t'
                + " | UPPER: " + std::to_string(monitoredProcesses[pid].upper_memory_limit) + '\n';
            }

        }
        else if ("infoall" == cmd) {
            if (monitoredProcesses.empty()) {
                std::cout << "No processes are being monitored right now. use add <pid> <lower> <upper>\n";
            }
            ProcessClass temp;
            for (auto i : monitoredProcesses) {
//                std::cout << std::to_string(i.first) + '\t' + temp.GetName(i.first) + " | MEM: " +
//                             std::to_string(temp.GetMemoryUsage(i.first)) + " MB\n";

                std::cout << std::to_string(i.first) + '\t' + temp.GetName(i.first)
                             + " | MEM: " + std::to_string(temp.GetMemoryUsage(i.first)) + " MB\t"
                             + " | LOWER: " + std::to_string(monitoredProcesses[i.first].lower_memory_limit) + '\t'
                             + " | UPPER: " + std::to_string(monitoredProcesses[i.first].upper_memory_limit) + '\n';
            }
        }
        else if ("list" == cmd) {
            PrintProcessesList();

        }
        else if ("cmdlist" == cmd) {
            std::cout << "\n";
            PrintCMD();
            std::cout << "\n";

        }
        else if ("exit" == cmd) {
            RUNNING = false;
        }
        else {
            std::wcerr << "Invalid command.\n";
            //&

        }
    }
}

int MonitorProcess(ProcessClass& process) {
    if (!process.isAccessible())
        return process.GetPid();
    int memory = process.GetMemoryUsage();
    int pid = process.GetPid();
    if (memory < process.lower_memory_limit || memory > process.upper_memory_limit) {
        process.Terminate();
        return pid;
    }
    return 0;
}

void MonitorProcesses() {
    while (RUNNING) {
        std::vector<int> toBeRemoved;
        for (auto& pair : monitoredProcesses) {
            auto& process = pair.second;
            int exitcode = MonitorProcess(process);

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


int main()
{
    PrintProcessesList();
    PrintCMD();

    std::thread monitor_thread(MonitorProcesses);
    std::thread command_thread(HandleCommands);

    monitor_thread.join();
    command_thread.join();
}
