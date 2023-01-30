#include "types.h"
#include <windows.h>
#include <tlhelp32.h>

namespace pcsx2reader {

static HANDLE ps2_handle = NULL;
#define PCSX2_BASE 0x20000000

bool pcsx2opened() {
    DWORD ec;
    if(ps2_handle != NULL) {
        if(GetExitCodeProcess(ps2_handle, &ec)) {
            if(ec == STILL_ACTIVE) {
                return true;
            } else {
                ps2_handle = NULL;
                return false;
            }
        }
    }
    return false;
}

static DWORD findprocessid(LPCWSTR name) {
    PROCESSENTRY32W pe;
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );
    if(INVALID_HANDLE_VALUE == snapshot) {
        return 0;
    }

    pe.dwSize = sizeof(pe);

    BOOL success = Process32FirstW(
        snapshot,
        &pe
    );

    if(!success) {
        return 0;
    }

    do {
        if(lstrcmpW(pe.szExeFile, name) == 0) {
            CloseHandle(snapshot);
            return pe.th32ProcessID;
        }
        success = Process32NextW(snapshot, &pe);
    } while (success);

    return 0;
}

bool openpcsx2() {
    if(pcsx2opened()) {
        return true;
    }

    DWORD pid = findprocessid(L"pcsx2-parappa.exe");
	DWORD pid2 = findprocessid(L"pcsx2.exe");
    if(pid == 0) {//209448
		if(pid2 != 0) {pid = pid2;}
        else {return false;}
    }

    ps2_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if(ps2_handle == NULL) {
        return false;
    }
    return true;
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
    if(!success) return false;
    return true;
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
    if(!success) return false;
    return true;
}

template<typename T>
T readT(u32 addr) {
    T tmp;
    read(addr, (void*)(&tmp), sizeof(tmp));
    return tmp;
}

};
