#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

namespace conscr {

#define SCR_WIDTH 80
#define SCR_HEIGHT 25
#define SCR_SIZE (SCR_WIDTH * SCR_HEIGHT)

CHAR_INFO scr[SCR_SIZE];
HANDLE stdOutH;
HANDLE stdInH;

void init() {
    FreeConsole();
    AllocConsole();
    stdOutH = GetStdHandle(STD_OUTPUT_HANDLE);
    stdInH = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleScreenBufferSize(stdOutH, {SCR_WIDTH, SCR_HEIGHT});
    SetConsoleTitleA("NeoBESMS");
}

void refresh() {
    SMALL_RECT region = {0, 0, SCR_WIDTH - 1, SCR_HEIGHT - 1};
    WriteConsoleOutputW(stdOutH, scr, {SCR_WIDTH, SCR_HEIGHT}, {0,0}, &region);
}

void clearcol(WORD attr) {
    for(CHAR_INFO &ci : scr) ci.Attributes = attr;
}

void clearchars(wchar_t c) {
    for(CHAR_INFO &ci : scr) ci.Char.UnicodeChar = c;
}

int postoindex(int x, int y) {
    return (y * SCR_WIDTH) + x;
}

void putch(int x, int y, wchar_t c) {
    int p = postoindex(x,y);
    scr[p].Char.UnicodeChar = c;
}

void putcol(int x, int y, WORD attr) {
    int p = postoindex(x,y);
    scr[p].Attributes = attr;
}

void putchcol(int x, int y, wchar_t c, WORD attr) {
    putch(x,y,c);
    putcol(x,y,attr);
}

void writes(int x, int y, LPCWSTR s) {
    int len = lstrlenW(s);
    for(int i = 0; i < len; i += 1) putch(x+i, y, s[i]);
}

void writecol(int x, int y, int len, WORD attr) {
    for(int i = 0; i < len; i += 1) putcol(x+i, y, attr);
}

void writescol(int x, int y, LPCWSTR s, WORD attr) {
    writes(x, y, s);
    writecol(x, y, lstrlenW(s), attr);
}

bool hasinput() {
    DWORD nevents;
    INPUT_RECORD ir;
    PeekConsoleInputW(stdInH, &ir, 1, &nevents);
    if(nevents > 0) return true;
    return false;
}

bool read(INPUT_RECORD &ir) {
    DWORD nevents;
    ReadConsoleInputW(stdInH, &ir, 1, &nevents);
    if(nevents == 0) return false;
    return true;
}

void flushinputs() {
	FlushConsoleInputBuffer(stdInH);
}

static void cprint(const wchar_t *msg) {
    DWORD written;
    WriteConsoleW(stdOutH, LPCVOID(msg), lstrlenW(msg), &written, NULL);
}

static char mygetchA() {
    DWORD readn;
    CHAR buf[4];
    do {
        ReadConsoleA(stdInH, LPVOID(buf), 1, &readn, NULL);
    } while(readn <= 0);
    return buf[0];
}

static wchar_t mygetchW() {
    DWORD readn;
    WCHAR buf[4];
    do {
        ReadConsoleW(stdInH, LPVOID(buf), 1, &readn, NULL);
    } while(readn <= 0);
    return wchar_t(buf[0]);
}

static int mygetsA(char *buf, int n) {
    char *cbuf = buf;
    int c = 0;
    while(c < (n-1)) {
        char a = mygetchA();
	    if(a == '\r') continue;
	    if(a == '\n') break;
	    *cbuf = a;
	    cbuf += 1;
    }
    *cbuf = char(0);
    return c;
}

static int mygetsW(wchar_t *buf, int n) {
    wchar_t *cbuf = buf;
    int c = 0;
    while(c < n) {
        char w = mygetchW();
	    if(w == L'\r') continue;
	    if(w == L'\n') break;
	    *cbuf = w;
	    cbuf += 1;
    }
    *cbuf = wchar_t(0);
    return c;
}

int query_decimal(const wchar_t *msg) {
    SetConsoleCursorPosition(stdOutH, {0,0});

    char buf[16];

    cprint(msg);
    flushinputs();
    mygetsA(buf, 16);
    buf[15] = char(0);

    int x = atoi(buf);
    SetConsoleCursorPosition(stdOutH, {0,0});
    return x;
}

int query_hex(const wchar_t *msg) {
    SetConsoleCursorPosition(stdOutH, {0,0});

    char buf[16];

    cprint(msg);
    flushinputs();
    mygetsA(buf, 16);
    buf[15] = char(0);

    int x = strtol(buf, NULL, 0x10);
    SetConsoleCursorPosition(stdOutH, {0,0});
    return x;
}

UINT64 query_hexU(const wchar_t *msg) {
    SetConsoleCursorPosition(stdOutH, {0,0});

    char buf[16];

    cprint(msg);
    flushinputs();
    mygetsA(buf, 16);
    buf[15] = char(0);

    unsigned long long x = strtoull(buf, NULL, 0x10);
    SetConsoleCursorPosition(stdOutH, {0,0});
    return x;
}

int query_string(const wchar_t *msg, wchar_t *out, int maxchars) {
    SetConsoleCursorPosition(stdOutH, {0,0});

    cprint(msg);
    flushinputs();
    mygetsW(out, maxchars);
    out[maxchars-1] = wchar_t(0);

    int slen = lstrlenW(out);
    if(out[slen-1] == L'\n') out[slen-1] = wchar_t(0);

    SetConsoleCursorPosition(stdOutH, {0,0});
    return lstrlenW(out);
}

};
