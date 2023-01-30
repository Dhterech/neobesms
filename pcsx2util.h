#ifndef BES_PCSX2UTIL_H
#define BES_PCSX2UTIL_H

#include "ss.h"
#include "types.h"
#include "suggest.h"

#define HD_SIGNATURE 0x53434549

u32 findhdbase(u32 resource_list_base);
u32 findbdbase(u32 resource_list_base);
int getnumhd(u32 hdbase);

void pcsx2DwnlRecord(
    u32 record_addr, e_suggestrecord_t &editor_record);

int pcsx2ReadRecords(
    u32 stagecmd_start,
    int count,
    u32 type,
    std::vector<e_suggestrecord_t> &records
);

void pcsx2GetComBuffers(u32 stagemode_start, std::vector<commandbuffer_t> *buffers);

int pcsx2GetRecFromModelist(
    u32 stagemode_start,
    std::vector<e_suggestrecord_t> &records,
    bool isPAL
);

void pcsx2GetSoundboards(
    u32 hdlistbase,
    u32 bdlistbase,
    u32 count,
    std::vector<e_soundboard_t> &sblist
);

void pcsx2DwnlKeytables(
    u32 keylistbase,
    int count,
    int sbbase,
    std::vector<e_soundboard_t> &sblist
);

u32 pcsx2calcsize(
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *commands,
    int oopslen, bool isPAL);
bool pcsx2upload(
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *commands,
    byte *oopsdat, int oopslen,
    u32 datastart, u32 dataend,
    u32 stagemodelistbase, bool isPAL);
bool olmupload(
    wchar_t *filename,
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *commands,
    byte *oopsdat, int oopslen,
    u32 datastart, u32 dataend,
    u32 stagemodelistbase, bool isPAL);

#endif // BES_PCSX2UTIL_H
