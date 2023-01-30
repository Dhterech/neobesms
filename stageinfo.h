#ifndef BES_STAGEINFO_H
#define BES_STAGEINFO_H
#include <array>
#include "types.h"
struct stageinfo_t {
    const wchar_t *name;
    double bpm;
    //std::array<u32, 4> recordlistbases;
    u32 stagemodelistbase;
    u32 keytablebase;
    u32 buttondatabase;
    u32 buttondataend;
	
	// PAL
	u32 stagemodelistbaseP;
	u32 keytablebaseP;
    u32 buttondatabaseP;
    u32 buttondataendP;
};

extern stageinfo_t stages[8];

#endif // BES_STAGEINFO_H
