#ifndef BES_PCSX2UTIL_H
#define BES_PCSX2UTIL_H

#include "ss.h"
#include "types.h"
#include "suggest.h"
#include "stageinfo.h"

#define HD_SIGNATURE 0x53434549

u32 findhdbase(u32 resource_list_base);
u32 findbdbase(u32 resource_list_base);
int getnumhd(u32 hdbase);

void pcsx2DwnlRecord(u32 record_addr, e_suggestrecord_t &editor_record);
void pcsx2ParseComRecords(std::vector<e_suggestrecord_t> &records, std::vector<std::vector<commandbuffer_t>> &commands, bool isVS);

void pcsx2GetComBuffers(std::vector<scenemode_t> &modes, std::vector<std::vector<commandbuffer_t>> &commands);
void pcxs2GetModelist(u32 stagemode_start, int count, std::vector<scenemode_t> &modes);

void pcsx2GetSoundboards(u32 hdlistbase, u32 bdlistbase, u32 count, std::vector<e_soundboard_t> &sblist);
void pcsx2GetKeytables(u32 keylistbase, int count, int sbbase, std::vector<e_soundboard_t> &sblist);

u32 pcsx2calcsize(std::vector<e_suggestrecord_t> &records, std::vector<std::vector<commandbuffer_t>> &commands, int oopslen, int modelen, bool isPAL);
bool pcsx2upload(std::vector<e_suggestrecord_t> &records, std::vector<std::vector<commandbuffer_t>> &commands, std::vector<scenemode_t> &modes, byte *oopsdat, int oopslen, int modelen, currentstage_t currentstage, bool isPAL, bool kSubs, bool isVSMode);
bool olmupload(wchar_t *filename);

void getProjectRecordAddresses(std::vector<e_suggestrecord_t> &records, std::vector<std::vector<commandbuffer_t>> &commands, int isVS);

#endif // BES_PCSX2UTIL_H
