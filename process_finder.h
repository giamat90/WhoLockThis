#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ProcessInfo {
    uint32_t    pid;
    std::string name;
    std::string fullPath;
    std::string owner;
    std::string memory;
    std::string startTime;
    std::string type;
};

// Collect all regular files under `path` (or just the file itself).
std::vector<std::wstring> collect_files(const std::string& path);

// Find every process that holds a lock on any of the given files.
std::vector<ProcessInfo> find_locking_processes(const std::vector<std::wstring>& files);

// Terminate a process by PID.  Returns true on success.
bool kill_process(uint32_t pid);

// Open a native folder-picker dialog.  Returns "" if cancelled.
std::string browse_for_folder();

// Open a native file-picker dialog.  Returns "" if cancelled.
std::string browse_for_file();
