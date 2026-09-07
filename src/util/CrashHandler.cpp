#include "util/CrashHandler.h"

#include <Windows.h>

#include <exception>

namespace overlaydesk {
namespace {

// The handler runs in a process that is already broken, so everything below obeys three
// rules: no heap allocation, no locks, and no CRT call that might take either. That rules
// out std::format, std::ofstream and the application's own logger - the logger holds a
// mutex, and a fault while that mutex is held would deadlock instead of reporting.
//
// Only fixed buffers, wsprintfA (which formats into a caller-supplied buffer) and raw
// Win32 file APIs are used.

wchar_t g_crashDirectory[MAX_PATH] = {};
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

void AppendPath(wchar_t (&destination)[MAX_PATH], const wchar_t* fileName) {
    lstrcpynW(destination, g_crashDirectory, MAX_PATH);
    const int length = lstrlenW(destination);
    if (length > 0 && destination[length - 1] != L'\\') {
        lstrcatW(destination, L"\\");
    }
    lstrcatW(destination, fileName);
}

const char* DescribeExceptionCode(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case 0xE06D7363: return "unhandled C++ exception";
        default: break;
    }
    return "unknown";
}

void WriteCrashRecord(EXCEPTION_POINTERS* exceptionInfo) {
    wchar_t path[MAX_PATH];
    AppendPath(path, L"crash.log");

    const HANDLE file = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME now{};
    ::GetSystemTime(&now);

    DWORD code = 0;
    void* address = nullptr;
    if (exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr) {
        code = exceptionInfo->ExceptionRecord->ExceptionCode;
        address = exceptionInfo->ExceptionRecord->ExceptionAddress;
    }

    // The module and offset are what makes the address useful: an absolute address means
    // nothing once ASLR has moved the image.
    wchar_t moduleName[MAX_PATH] = L"<unknown>";
    uintptr_t offset = 0;
    HMODULE module = nullptr;
    if (address != nullptr &&
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             static_cast<LPCWSTR>(address), &module) != 0 &&
        module != nullptr) {
        ::GetModuleFileNameW(module, moduleName, MAX_PATH);
        offset = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module);
    }

    char record[1024];
    wsprintfA(record,
              "\r\n"
              "---- Overlay Desk crash ----\r\n"
              "when      : %04d-%02d-%02d %02d:%02d:%02dZ\r\n"
              "version   : %s\r\n"
              "code      : 0x%08X (%s)\r\n"
              "address   : 0x%p\r\n"
              "module    : %S\r\n"
              "offset    : 0x%IX\r\n"
              "process   : %u   thread: %u\r\n",
              now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
              OVERLAYDESK_VERSION, code, DescribeExceptionCode(code), address, moduleName,
              offset, ::GetCurrentProcessId(), ::GetCurrentThreadId());

    DWORD written = 0;
    ::WriteFile(file, record, static_cast<DWORD>(lstrlenA(record)), &written, nullptr);

    // An access violation carries the operation and the faulting address, which is usually
    // the single most useful line in the whole record.
    if (code == EXCEPTION_ACCESS_VIOLATION && exceptionInfo != nullptr &&
        exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR operation = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
        const ULONG_PTR target = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
        wsprintfA(record, "access    : %s at 0x%IX\r\n",
                  operation == 0 ? "read" : (operation == 1 ? "write" : "execute"), target);
        ::WriteFile(file, record, static_cast<DWORD>(lstrlenA(record)), &written, nullptr);
    }

    ::CloseHandle(file);
}

void WriteMiniDump(EXCEPTION_POINTERS* exceptionInfo) {
    // dbghelp is loaded here rather than linked, so a machine without it still gets the
    // text record above instead of failing to start at all.
    const HMODULE dbghelp = ::LoadLibraryW(L"dbghelp.dll");
    if (dbghelp == nullptr) {
        return;
    }

    using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
    const auto writeDump =
        reinterpret_cast<MiniDumpWriteDumpFn>(
            reinterpret_cast<void*>(::GetProcAddress(dbghelp, "MiniDumpWriteDump")));
    if (writeDump == nullptr) {
        ::FreeLibrary(dbghelp);
        return;
    }

    SYSTEMTIME now{};
    ::GetSystemTime(&now);
    wchar_t fileName[64];
    wsprintfW(fileName, L"crash-%04d%02d%02d-%02d%02d%02d.dmp", now.wYear, now.wMonth, now.wDay,
              now.wHour, now.wMinute, now.wSecond);

    wchar_t path[MAX_PATH];
    AppendPath(path, fileName);

    const HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        // MINIDUMP_EXCEPTION_INFORMATION, declared inline to avoid pulling in dbghelp.h.
        struct {
            DWORD ThreadId;
            EXCEPTION_POINTERS* ExceptionPointers;
            BOOL ClientPointers;
        } information{::GetCurrentThreadId(), exceptionInfo, FALSE};

        // MiniDumpWithIndirectlyReferencedMemory (0x40) | MiniDumpScanMemory (0x10): enough
        // context to see what the faulting code was looking at, without dumping the whole
        // address space.
        constexpr int kDumpType = 0x00000040 | 0x00000010;
        writeDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, kDumpType, &information,
                  nullptr, nullptr);
        ::CloseHandle(file);
    }

    ::FreeLibrary(dbghelp);
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exceptionInfo) {
    WriteCrashRecord(exceptionInfo);
    WriteMiniDump(exceptionInfo);

    // Hand back to whatever was installed before us - usually Windows Error Reporting, which
    // the user may well have configured to collect this too.
    if (g_previousFilter != nullptr) {
        return g_previousFilter(exceptionInfo);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void OnTerminate() {
    // std::terminate does not go through the SEH filter, so it needs its own route to the
    // same record.
    WriteCrashRecord(nullptr);
    ::TerminateProcess(::GetCurrentProcess(), 3);
}

}  // namespace

void InstallCrashHandler(const std::filesystem::path& crashDirectory) {
    if (crashDirectory.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(crashDirectory, ec);
    if (ec) {
        return;
    }

    // Copied into a fixed buffer now, while allocation is still safe. The handler must not
    // touch a std::filesystem::path.
    lstrcpynW(g_crashDirectory, crashDirectory.c_str(), MAX_PATH);

    g_previousFilter = ::SetUnhandledExceptionFilter(&OnUnhandledException);
    std::set_terminate(&OnTerminate);
}

}  // namespace overlaydesk
