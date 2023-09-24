#ifndef BES_STAGEINFO_H
#define BES_STAGEINFO_H
#include "types.h"

struct currentstage_t {
    const wchar_t *name;
    double bpm;
    u32 stagemodelistbase;
    u32 keytablebase;
    u32 buttondatabase;
    u32 buttondataend;
};

struct regioninfo_t {
    u32 stagemodelistbase;
    u32 keytablebase;
    u32 keytablesize;
};

struct stageinfo_t {
    const wchar_t *name;
    double bpm;
    regioninfo_t regions[3];
};

extern stageinfo_t stages[18];

#endif // BES_STAGEINFO_H
