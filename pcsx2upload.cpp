#include "suggest.h"
#include "pcsx2reader.h"
#include "stageinfo.h"
#include <vector>
#include <memory.h>
#include <Windows.h>

u32 pcsx2calcsize(std::vector<e_suggestrecord_t> &records, std::vector<commandbuffer_t> *buffers, int oopslen, bool isPAL) {
    u32 size = sizeof(suggestrecord_t) * records.size();

    for(int i = 0; i < records.size(); i++) {
        e_suggestrecord_t &record = records[i];
        for(int k = 0; k < 17; k++) {
            e_suggestvariant_t &variant = record.variants[k];
            if(variant.islinked) continue;
            int linesize = (isPAL ? sizeof(suggestline_t_pal) : sizeof(suggestline_t));
            size += linesize * variant.lines.size();
            for(int m = 0; m < variant.lines.size(); m++) {
                size += sizeof(suggestbutton_t) * variant.lines[m].buttons.size();
            }
        }
    }

    for(int i = 0; i < 9; i++) size += buffers[i].size() * 0x10;
    return size + oopslen;
}

#define UPLOAD_NOMOVE(y,x) UPLOAD_SIZE(y,x,sizeof(x))
#define UPLOAD(y, x) UPLOAD_NOMOVE(y,x); y += sizeof(x);

#define UPLOAD_SIZE(y,x,s) pcsx2reader::write(y, &(x), s)
#define DOWNLOAD_SIZE(y,x,s) pcsx2reader::read(y, &(x), s)

u32 applymetafixup(u32 start, u32 fixup) {
    u32 caddr = start;
    u32 ptrinmem = 0;
    u32 oldaddr = 0;

    for(;;) {
        pcsx2reader::read(caddr, &ptrinmem, 4);
        if(!(ptrinmem & 0x1000000)) { caddr += 4; continue; }
        if(oldaddr == 0) oldaddr = ptrinmem;
        if(ptrinmem != oldaddr) return caddr;
        UPLOAD_NOMOVE(caddr, fixup);
        caddr += 4;
    }
}

bool pcsx2upload(
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *buffers,
    byte *oopsdat, int oopslen,
    currentstage_t currentstage,
    bool isPAL, bool kSubs) {
    u32 uploadPos = currentstage.buttondatabase;
    u32 ptr_oops_parap;
    u32 ptr_oops_teach;
    u32 mb[9];
    scenemode_t modes[9];

    DOWNLOAD_SIZE(currentstage.stagemodelistbase, modes[0], sizeof(modes));
    for(int i = 0; i < 9; i++) {
        mb[i] = uploadPos;
        modes[i].ptr_scenecommands = mb[i];
        for(int k = 0; k < buffers[i].size(); k++) {UPLOAD(uploadPos, buffers[i][k]);}
    }
    UPLOAD_NOMOVE(currentstage.stagemodelistbase, modes);
    UPLOAD_SIZE(uploadPos, oopsdat[0], oopslen);
    ptr_oops_teach = uploadPos;
    ptr_oops_parap = ptr_oops_teach + (sizeof(suggestbutton_t) * 6);
    uploadPos += oopslen;
    for(int i = 0; i < records.size(); i++) {
        e_suggestrecord_t &record = records[i];
        suggestrecord_t ps2record;

        for(int k = 0; k < 17; k++) {
            e_suggestvariant_t &variant = record.variants[k];
            suggestvariant_t &ps2variant = ps2record.variants[k];
            if(variant.islinked) continue;

            int nbuttons = 0;
            for(e_suggestline_t &line : variant.lines) nbuttons += line.buttons.size();
            u32 buttonbase = uploadPos;
            u32 linebase = buttonbase + (nbuttons * sizeof(suggestbutton_t));

            ps2variant.numlines = variant.lines.size();
            ps2variant.ptr_lines = linebase;

            for(int m = 0; m < variant.lines.size(); m++) {
                e_suggestline_t &line = variant.lines[m];
                suggestline_t ps2line;
                suggestline_t_pal ps2lineP;

                if(!isPAL) {
                    ps2line.buttoncount = line.buttons.size();
                    ps2line.ptr_buttons = buttonbase;
                    for(int n = 0; n < line.buttons.size(); n++) {UPLOAD(buttonbase, line.buttons[n]);}
                    ps2line.always_zero = 0;
                    ps2line.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 4; t++) {ps2line.localisations[t] = (kSubs) ? line.localisations[t] : u32(~0);}
                    ps2line.owner = line.owner;
                    ps2line.timestamp_start = line.timestamp_start;
                    ps2line.timestamp_end = line.timestamp_end;
                    ps2line.ptr_oops = (ps2line.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2line.oopscount = 6;
                    UPLOAD(linebase, ps2line);
                } else {
                    ps2lineP.buttoncount = line.buttons.size();
                    ps2lineP.ptr_buttons = buttonbase;
                    for(int n = 0; n < line.buttons.size(); n++) {UPLOAD(buttonbase, line.buttons[n]);}
                    ps2lineP.always_zero = 0;
                    ps2lineP.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 7; t++) {ps2lineP.localisations[t] = (kSubs) ? line.localisations[t] : u32(~0);}
                    ps2lineP.owner = line.owner;
                    ps2lineP.timestamp_start = line.timestamp_start;
                    ps2lineP.timestamp_end = line.timestamp_end;
                    ps2lineP.ptr_oops = (ps2lineP.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2lineP.oopscount = 6;
                    UPLOAD(linebase, ps2lineP);
                }
            }
            uploadPos = linebase; /* linebase is actually after all lines */
        }
        for(int k = 0; k < 17; k++) {
            e_suggestvariant_t &variant = record.variants[k];
            suggestvariant_t &ps2variant = ps2record.variants[k];
            if(variant.islinked) {
                suggestvariant_t &linkedto = ps2record.variants[variant.linknum];
                ps2variant.numlines = linkedto.numlines;
                ps2variant.ptr_lines = linkedto.ptr_lines;
            }
        }
        ps2record.lengthinsubdots = record.lengthinsubdots;
        ps2record.soundboardid = record.soundboardid;
        memcpy(ps2record.unk, record.unk, sizeof(ps2record.unk));

        if(record.type == 1) {
            for(int k = 1; k < 4; k++) mb[k] = applymetafixup(mb[k], uploadPos);
        } else {
            mb[record.type] = applymetafixup(mb[record.type], uploadPos);
        }
        UPLOAD(uploadPos, ps2record);
    }

    return true;
}

#undef UPLOAD_SIZE
#undef DOWNLOAD_SIZE

#define OLM_LINK_ADDRESS 0x1CA0000

bool olmupload(wchar_t *filename) {
    HANDLE hfile = CreateFileW(
        LPCWSTR(filename), GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    DWORD written = 0;
    if(hfile == INVALID_HANDLE_VALUE) return false;

    DWORD lowFileSiz;
    DWORD higFileSiz;
    lowFileSiz = GetFileSize(hfile, &higFileSiz);
    char* buffer = new char[lowFileSiz + 1];
    ReadFile(hfile, buffer, lowFileSiz, &written, NULL);
    int result = pcsx2reader::write(OLM_LINK_ADDRESS, buffer, (u32)lowFileSiz + 1);

    CloseHandle(hfile);
    return result;
}