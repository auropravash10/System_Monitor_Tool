#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <signal.h>
#include <cstring>
#include <iomanip>

using namespace std;

/*
MEMORY READING FROM /proc/meminfo
---------------------------------
Reads MemTotal and MemAvailable
*/
void readMemoryInfo(long &totalMem, long &freeMem) {
    ifstream file("/proc/meminfo");
    string key, unit;
    long value;
    totalMem = freeMem = 0;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:")
            totalMem = value / 1024;     // KB → MB
        if (key == "MemAvailable:")
            freeMem = value / 1024;
    }
}

/*
CPU READING FROM /proc/stat
---------------------------
Reads user, nice, system, idle
*/
long long lastTotal = 0, lastIdle = 0;

float readCpuUsage() {
    ifstream file("/proc/stat");
    string cpu;
    long long user, nice, system, idle;

    file >> cpu >> user >> nice >> system >> idle;

    long long total = user + nice + system + idle;
    long long totalDiff = total - lastTotal;
    long long idleDiff = idle - lastIdle;

    float cpuPercent = 0;
    if (totalDiff != 0)
        cpuPercent = (float)(totalDiff - idleDiff) * 100.0 / totalDiff;

    lastTotal = total;
    lastIdle = idle;

    return cpuPercent;
}

struct Process {
    int pid;
    string name;
    long memoryKB;
    float cpuPercent;
};

/* Check if folder name is number → means PID */
bool isNumber(const string &s) {
    for (char c : s)
        if (!isdigit((unsigned char)c)) return false;
    return true;
}

/* Get process list */
vector<Process> getProcesses() {
    vector<Process> result;
    DIR *dir = opendir("/proc");
    if (!dir) return result;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        string dirname = entry->d_name;
        if (!isNumber(dirname)) continue;

        int pid = stoi(dirname);

        string pname;
        {
            string path = "/proc/" + dirname + "/comm";
            ifstream f(path);
            if (f.good()) getline(f, pname);
        }

        long mem = 0;
        {
            string path = "/proc/" + dirname + "/statm";
            ifstream f(path);
            if (f.good()) {
                long pages = 0;
                f >> pages;
                mem = pages * 4;   // pages → KB
            }
        }

        result.push_back({pid, pname, mem, 0.0f});
    }

    closedir(dir);
    return result;
}

/* Kill Process */
void killProcess(int pid) {
    if (kill(pid, SIGKILL) == 0)
        cout << "Process " << pid << " killed.\n";
    else
        cerr << "Failed: " << strerror(errno) << "\n";
}

/* Sorting mode */
char sortMode = 'n';

/* Display everything once */
void display() {
    long totalMem = 0, freeMem = 0;
    readMemoryInfo(totalMem, freeMem);

    float cpu = readCpuUsage();
    auto plist = getProcesses();

    if (sortMode == 'm') {
        sort(plist.begin(), plist.end(), [](auto &a, auto &b){
            return a.memoryKB > b.memoryKB;
        });
    }

    system("clear");

    cout << "=================== SYSTEM MONITOR ===================\n";
    cout << fixed << setprecision(1);
    cout << "CPU Usage: " << cpu << "%\n";
    cout << "Memory: " << (totalMem - freeMem) << " MB / " << totalMem << " MB\n";

    cout << "------------------------------------------------------\n";
    cout << left << setw(8) << "PID"
         << setw(14) << "Memory(KB)"
         << setw(20) << "Name" << "\n";
    cout << "------------------------------------------------------\n";

    int limit = 120;
    for (auto &p : plist) {
        cout << left << setw(8) << p.pid
             << setw(14) << p.memoryKB
             << setw(20) << p.name << "\n";
        if (--limit <= 0) break;
    }

    cout << "------------------------------------------------------\n";
    cout << "Enter = Refresh\n"
         << "m = Sort by memory\n"
         << "n = No sort\n"
         << "k <PID> = Kill process\n"
         << "q = Quit\n";
}


/* MAIN LOOP: Refresh manually via ENTER */
int main() {
    readCpuUsage();   // initialize CPU baseline

    while (true) {
        display();

        cout << "\nCommand: ";
        string input;
        getline(cin, input);

        if (input == "") continue;     // ENTER → refresh
        if (input == "q") break;       // quit
        if (input == "m") { sortMode = 'm'; continue; }
        if (input == "n") { sortMode = 'n'; continue; }

        // Kill format: k 1234
        if (input[0] == 'k') {
            try {
                int pid = stoi(input.substr(2));
                killProcess(pid);
            } catch (...) {
                cout << "Invalid format. Use: k 1234\n";
            }
        }
    }

    cout << "Exiting System Monitor.\n";
    return 0;
}
}
