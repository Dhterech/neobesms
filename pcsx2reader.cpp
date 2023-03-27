#include "types.h"
#include "config.h"
#include <windows.h>
#include <tlhelp32.h>

namespace pcsx2reader {

static HANDLE ps2_handle = NULL;
UINT64 PCSX2_BASE = 0x20000000;

void setBaseAddr(UINT64 newBase) {
    if(newBase > 0) PCSX2_BASE = newBase;
}

bool pcsx2opened() {
    DWORD ec;
    if(ps2_handle == NULL) return false;
    if(GetExitCodeProcess(ps2_handle, &ec)) {
        if(ec == STILL_ACTIVE) return true;
    }
    ps2_handle = NULL;
    return false;
}

static DWORD findprocessid(LPCWSTR name) {
    PROCESSENTRY32W pe;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(INVALID_HANDLE_VALUE == snapshot) return 0;

    pe.dwSize = sizeof(pe);
    if(!Process32FirstW(snapshot, &pe)) return 0;

    do {
        if(!lstrcmpW(pe.szExeFile, name)) {
            CloseHandle(snapshot);
            return pe.th32ProcessID;
        }
    } while (Process32NextW(snapshot, &pe));
    return 0;
}

bool openpcsx2() {
    if(pcsx2opened()) return true;

    wchar_t *procs[] = DEFAULT_PROC;
    DWORD pid;
    for(wchar_t *p : procs) {
        pid = findprocessid(p);
        if(pid != 0) break;
    }
    if(pid == 0) return false;

    ps2_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    return ps2_handle != NULL;
}

#define checkpcsx2() if(!openpcsx2()) { return false; }

bool read(u32 addr, void *out, u32 size) {
    SIZE_T readin;
    checkpcsx2();
    BOOL success = ReadProcessMemory(
        ps2_handle,
        LPCVOID(UINT_PTR(addr) + PCSX2_BASE),
        LPVOID(out),
        SIZE_T(size),
        &readin
    );
    return success;
}

bool write(u32 addr, void *out, u32 size) {
    SIZE_T written;
    checkpcsx2();
    BOOL success = WriteProcessMemory(
        ps2_handle,
        LPVOID(UINT_PTR(addr) + PCSX2_BASE),
        LPCVOID(out),
        SIZE_T(size),
        &written
    );
    return success;
}

template<typename T>
T readT(u32 addr) {
    T tmp;
    read(addr, (void*)(&tmp), sizeof(tmp));
    return tmp;
}

};
