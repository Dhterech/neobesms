#ifndef BES_CONSCR_H
#define BES_CONSCR_H

#include "types.h"
#include <Windows.h>

namespace conscr {
    void init();
    void refresh();
    void putch(int x, int y, wchar_t c);
    void putcol(int x, int y, WORD attr);
    void putchcol(int x, int y, wchar_t c, WORD attr);
    void clearchars(wchar_t c);
    void clearcol(WORD a);
    void writes(int x, int y, LPCWSTR s);
    void writescol(int x, int y, LPCWSTR s, WORD attr);
    bool read(INPUT_RECORD &ir);
    bool hasinput();
    int query_decimal(const wchar_t *msg);
    int query_hex(const wchar_t *msg);
    int query_string(const wchar_t *msg, wchar_t *out, int maxchars);
    void flushinputs();

};

#endif // BES_CONSCR_H
