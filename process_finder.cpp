#include "process_finder.h"

#include <windows.h>
#include <restartmanager.h>
#include <psapi.h>
#include <shlobj.h>
#include <commdlg.h>

#include <filesystem>
#include <set>
#include <vector>

namespace fs = std::filesystem;

// ── helpers ──────────────────────────────────────────────────────────────────

static std::wstring to_wide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
    return w;
}

static std::string to_narrow(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                  nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                        s.data(), len, nullptr, nullptr);
    return s;
}

static std::string format_filetime(const FILETIME& ft)
{
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::string get_process_owner(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return "<unknown>";

    HANDLE hToken = nullptr;
    std::string result = "<unknown>";

    if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
        DWORD tokenLen = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenLen);

        std::vector<BYTE> buf(tokenLen);
        if (GetTokenInformation(hToken, TokenUser, buf.data(), tokenLen, &tokenLen)) {
            auto* tokenUser = reinterpret_cast<TOKEN_USER*>(buf.data());
            wchar_t name[256] = {}, domain[256] = {};
            DWORD nameLen = 256, domainLen = 256;
            SID_NAME_USE sidType;
            if (LookupAccountSidW(nullptr, tokenUser->User.Sid,
                                  name, &nameLen, domain, &domainLen, &sidType)) {
                result = to_narrow(domain) + "\\" + to_narrow(name);
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProc);
    return result;
}

static std::string get_full_image_path(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return "<access denied>";

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::string result = "<unknown>";
    if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
        result = to_narrow(path);
    }
    CloseHandle(hProc);
    return result;
}

static std::string get_memory_usage(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) return "N/A";

    PROCESS_MEMORY_COUNTERS pmc = {};
    std::string result = "N/A";
    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
        double mb = pmc.WorkingSetSize / (1024.0 * 1024.0);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
        result = buf;
    }
    CloseHandle(hProc);
    return result;
}

static std::string get_process_start_time(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return "N/A";

    FILETIME creation, exitTime, kernel, user;
    std::string result = "N/A";
    if (GetProcessTimes(hProc, &creation, &exitTime, &kernel, &user)) {
        FILETIME local;
        FileTimeToLocalFileTime(&creation, &local);
        result = format_filetime(local);
    }
    CloseHandle(hProc);
    return result;
}

static std::string rm_app_type_str(UINT type)
{
    switch (type) {
    case RmUnknownApp:  return "Unknown";
    case RmMainWindow:  return "GUI Application";
    case RmOtherWindow: return "GUI (other window)";
    case RmService:     return "Windows Service";
    case RmExplorer:    return "Explorer";
    case RmConsole:     return "Console Application";
    case RmCritical:    return "Critical Process";
    default:            return "Other";
    }
}

// ── public API ───────────────────────────────────────────────────────────────

std::vector<std::wstring> collect_files(const std::string& path,
                                        std::function<void(size_t)> progress)
{
    std::vector<std::wstring> files;
    fs::path p(path);
    std::error_code ec;

    if (fs::is_regular_file(p, ec)) {
        files.push_back(fs::canonical(p).wstring());
        if (progress) progress(files.size());
    } else if (fs::is_directory(p, ec)) {
        for (auto& entry : fs::recursive_directory_iterator(
                 p, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec)) {
                files.push_back(entry.path().wstring());
                if (progress) progress(files.size());
            }
        }
    }
    return files;
}

std::vector<std::wstring> collect_files(const std::string& path)
{
    return collect_files(path, nullptr);
}

std::vector<ProcessInfo> find_locking_processes(const std::vector<std::wstring>& files)
{
    std::vector<ProcessInfo> result;

    DWORD session = 0;
    wchar_t sessionKey[CCH_RM_SESSION_KEY + 1] = {};
    DWORD err = RmStartSession(&session, 0, sessionKey);
    if (err != ERROR_SUCCESS) return result;

    std::vector<LPCWSTR> ptrs;
    ptrs.reserve(files.size());
    for (auto& f : files) ptrs.push_back(f.c_str());

    err = RmRegisterResources(session, (UINT)ptrs.size(), ptrs.data(),
                              0, nullptr, 0, nullptr);
    if (err != ERROR_SUCCESS) {
        RmEndSession(session);
        return result;
    }

    UINT procInfoNeeded = 0, procInfoCount = 0;
    DWORD rebootReason = 0;

    err = RmGetList(session, &procInfoNeeded, &procInfoCount, nullptr, &rebootReason);
    if (err == ERROR_SUCCESS && procInfoNeeded == 0) {
        RmEndSession(session);
        return result;
    }
    if (err != ERROR_MORE_DATA && err != ERROR_SUCCESS) {
        RmEndSession(session);
        return result;
    }

    std::vector<RM_PROCESS_INFO> procs(procInfoNeeded + 4);
    procInfoCount = (UINT)procs.size();

    err = RmGetList(session, &procInfoNeeded, &procInfoCount, procs.data(), &rebootReason);
    if (err != ERROR_SUCCESS) {
        RmEndSession(session);
        return result;
    }

    std::set<DWORD> seen;
    for (UINT i = 0; i < procInfoCount; ++i) {
        DWORD pid = procs[i].Process.dwProcessId;
        if (!seen.insert(pid).second) continue;

        ProcessInfo info;
        info.pid       = pid;
        info.name      = to_narrow(procs[i].strAppName);
        info.fullPath  = get_full_image_path(pid);
        info.owner     = get_process_owner(pid);
        info.memory    = get_memory_usage(pid);
        info.startTime = get_process_start_time(pid);
        info.type      = rm_app_type_str(procs[i].ApplicationType);
        result.push_back(std::move(info));
    }

    RmEndSession(session);
    return result;
}

bool kill_process(uint32_t pid)
{
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProc) return false;
    BOOL ok = TerminateProcess(hProc, 1);
    CloseHandle(hProc);
    return ok != 0;
}

std::string browse_for_folder()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IFileOpenDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                 IID_IFileOpenDialog, (void**)&pDialog);
    std::string result;

    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        pDialog->GetOptions(&options);
        pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        pDialog->SetTitle(L"WhoLockThis - Select folder to scan");

        hr = pDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR path = nullptr;
                pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
                if (path) {
                    result = to_narrow(path);
                    CoTaskMemFree(path);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }
    CoUninitialize();
    return result;
}

std::string browse_for_file()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IFileOpenDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                 IID_IFileOpenDialog, (void**)&pDialog);
    std::string result;

    if (SUCCEEDED(hr)) {
        pDialog->SetTitle(L"WhoLockThis - Select file to check");

        hr = pDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR path = nullptr;
                pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
                if (path) {
                    result = to_narrow(path);
                    CoTaskMemFree(path);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }
    CoUninitialize();
    return result;
}
