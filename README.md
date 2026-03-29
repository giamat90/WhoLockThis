<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white" />
  <img src="https://img.shields.io/badge/language-C%2B%2B17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/build-CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white" />
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge" />
</p>

<h1 align="center">WhoLockThis</h1>

<p align="center">
  <i>Find out who's locking your files — and show them the door.</i>
</p>

<p align="center">
  <code>wholock.exe path\to\locked_file.txt</code>
</p>

---

## What is WhoLockThis?

Ever tried to delete, move, or edit a file only to get the dreaded **"The process cannot access the file because it is being used by another process"**?

**WhoLockThis** is a lightweight, zero-dependency Windows console tool that instantly tells you *which* process is holding your file hostage — and lets you terminate it on the spot.

It works on **individual files** and **entire directories** (recursively), showing you full process details so you can make an informed decision before pulling the trigger.

---

## Features

| Feature | Description |
|---|---|
| **File & Folder support** | Pass a single file or a directory — WhoLockThis recursively scans all files inside |
| **Restart Manager API** | Uses the official Windows Restart Manager for reliable lock detection |
| **Rich process details** | PID, name, full executable path, owner, app type, memory usage, start time |
| **Interactive kill menu** | Kill a specific process, all of them, or walk away — your choice |
| **Confirmation prompts** | Always asks before terminating, so no accidents |
| **Static binary** | Single `.exe`, no DLL dependencies, runs anywhere on Windows |

---

## Demo

```
> wholock.exe C:\Projects\myapp\data

Scanning directory: C:\Projects\myapp\data ...
Found 42 file(s) to check.

Searching for processes locking the target...

Found 2 process(es) locking "C:\Projects\myapp\data":

  [1] PID: 14320
      Name:       node.exe
      Path:       C:\Program Files\nodejs\node.exe
      Owner:      DESKTOP-ABC\john
      Type:       Console Application
      Memory:     85.3 MB
      Started:    2026-03-29 10:15:42

  [2] PID: 8876
      Name:       Code.exe
      Path:       C:\Users\john\AppData\Local\Programs\Microsoft VS Code\Code.exe
      Owner:      DESKTOP-ABC\john
      Type:       GUI Application
      Memory:     210.7 MB
      Started:    2026-03-29 09:01:18

Options:
  Enter a number (1-2) to kill that process
  Enter 'a' to kill ALL listed processes
  Enter 'q' to quit without killing anything

> 1
Kill PID 14320 (node.exe)? (y/n): y
  Successfully killed PID 14320.
```

---

## Build

### Prerequisites

- **CMake** 3.15+
- A C++17 compiler (MinGW-w64, MSVC, or Clang)

### With MinGW

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### With MSVC

```bash
cmake -B build
cmake --build build --config Release
```

The compiled binary will be at `build/wholock.exe`.

---

## Usage

```
wholock.exe <file_or_folder_path>
```

| Argument | Description |
|---|---|
| `file_or_folder_path` | Path to the file or directory to check for locks |

### Examples

```bash
# Check a specific file
wholock.exe "C:\Users\me\report.xlsx"

# Scan an entire project folder
wholock.exe "D:\Projects\webapp"

# Check a DLL that won't update
wholock.exe "C:\Windows\System32\some.dll"
```

> **Tip:** Run as **Administrator** for full visibility into system and service processes.

---

## How It Works

```
                 +------------------+
                 |  wholock.exe     |
                 +--------+---------+
                          |
              +-----------v-----------+
              |  Collect file paths   |
              |  (single or recursive)|
              +-----------+-----------+
                          |
              +-----------v-----------+
              | Windows Restart       |
              | Manager API           |
              |                       |
              | RmStartSession()      |
              | RmRegisterResources() |
              | RmGetList()           |
              +-----------+-----------+
                          |
              +-----------v-----------+
              | Enrich with details:  |
              | - Full image path     |
              | - Process owner/SID   |
              | - Memory (WS size)    |
              | - Start time          |
              +-----------+-----------+
                          |
              +-----------v-----------+
              | Display + interactive |
              | kill prompt           |
              +------------------------+
```

**Why Restart Manager?** It's the same API that Windows Installer uses to detect which apps need to be closed before updating files. It's more reliable than brute-force handle enumeration and doesn't require driver-level access.

---

## Process Information

WhoLockThis retrieves the following details for each locking process:

| Field | Source | Description |
|---|---|---|
| **PID** | Restart Manager | Process identifier |
| **Name** | Restart Manager | Application display name |
| **Path** | `QueryFullProcessImageName` | Full path to the executable |
| **Owner** | `OpenProcessToken` + `LookupAccountSid` | DOMAIN\User running the process |
| **Type** | Restart Manager | GUI App, Console, Service, Explorer, Critical |
| **Memory** | `GetProcessMemoryInfo` | Working set size in MB |
| **Started** | `GetProcessTimes` | Process creation timestamp |

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
  <b>WhoLockThis</b> — because you deserve to know who's locking your files.
</p>
