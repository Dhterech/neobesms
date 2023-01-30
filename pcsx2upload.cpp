#include "suggest.h"
#include "pcsx2reader.h"
#include <vector>
#include <memory.h>
#include <Windows.h>

u32 pcsx2calcsize(std::vector<e_suggestrecord_t> &records, std::vector<commandbuffer_t> *buffers, int oopslen) {
    u32 acc = 0;
    acc += sizeof(suggestrecord_t) * records.size();

    for(int i = 0; i < records.size(); i += 1) {
        e_suggestrecord_t &record = records[i];
        for(int k = 0; k < 17; k += 1) {
            e_suggestvariant_t &variant = record.variants[k];
            if(variant.islinked == false) {
                acc += sizeof(suggestline_t) * variant.lines.size();
                for(int m = 0; m < variant.lines.size(); m += 1) {
                    e_suggestline_t &line = variant.lines[m];
                    acc += sizeof(suggestbutton_t) * line.buttons.size();
                }
            }
        }
    }

    for(int i = 0; i < 9; i += 1) acc += buffers[i].size() * 0x10;
    return acc + oopslen;
}

#define UPLOAD_NOMOVE(y,x) UPLOAD_SIZE(y,x,sizeof(x))
#define UPLOAD(y, x) UPLOAD_NOMOVE(y,x); y += sizeof(x);

#define UPLOAD_SIZE(y,x,s) pcsx2reader::write(y, &(x), s)
#define DOWNLOAD_SIZE(y,x,s) pcsx2reader::read(y, &(x), s)

u32 applymetafixup(u32 start, u32 fixup, bool skip) {
    u32 caddr = start;
    u32 ptrinmem = 0;
    u32 oldaddr = 0;

    for(;;) {
        pcsx2reader::read(caddr, &ptrinmem, 4);

        if((ptrinmem & 0x1000000) == 0) {caddr += 4; continue;}

        if(oldaddr == 0) oldaddr = ptrinmem;

        if(ptrinmem != oldaddr) return caddr;

        if(!skip) UPLOAD_NOMOVE(caddr, fixup);
        caddr += 4;
    }
}

void getmetabases(u32 stagemodelistbase, u32 *mb) {
    scenemode_t modes[9];
    pcsx2reader::read(stagemodelistbase, modes, sizeof(modes));
    for(int i = 0; i < 9; i += 1) {
        pcsx2reader::read(modes[i].ptr_scenecommands, mb + i, 4);
    }
    return;
}

bool pcsx2upload(
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *buffers,
    byte *oopsdat, int oopslen,
    u32 datastart, u32 dataend,
    u32 stagemodelistbase, bool isPAL) {
    u32 a = datastart;
    u32 ptr_oops_parap;
    u32 ptr_oops_teach;
    u32 mb[9];
    scenemode_t modes[9];
    //getmetabases(stagemodelistbase, mb);
    u32 count = dataend - datastart + 1;
    if(pcsx2calcsize(records, buffers, oopslen) > count) return false;
    DOWNLOAD_SIZE(stagemodelistbase, modes[0], sizeof(modes));
    for(int i = 0; i < 9; i += 1) {
        mb[i] = a;
        modes[i].ptr_scenecommands = mb[i];
        for(int k = 0; k < buffers[i].size(); k += 1) {UPLOAD(a, buffers[i][k]);}
    }
    UPLOAD_NOMOVE(stagemodelistbase, modes);
    UPLOAD_SIZE(a, oopsdat[0], oopslen);
    ptr_oops_teach = a;
    ptr_oops_parap = ptr_oops_teach + (sizeof(suggestbutton_t) * 6);
    a += oopslen;
    for(int i = 0; i < records.size(); i += 1) {
        e_suggestrecord_t &record = records[i];
        /* FIXME
            Injecting cool record breaks getting worse/better
            and cool mode itself
        */
        //if(record.iscool) continue;
        suggestrecord_t ps2record;
        u32 linebase;

        for(int k = 0; k < 17; k += 1) {
            e_suggestvariant_t &variant = record.variants[k];
            suggestvariant_t &ps2variant = ps2record.variants[k];
            if(variant.islinked) continue;

            int nbuttons = 0;
            for(e_suggestline_t &line : variant.lines) nbuttons += line.buttons.size();
            u32 buttonbase = a;
            u32 linebase = buttonbase + (nbuttons * sizeof(suggestbutton_t));

            ps2variant.numlines = variant.lines.size();
            ps2variant.ptr_lines = linebase;

            for(int m = 0; m < variant.lines.size(); m += 1) {
                e_suggestline_t &line = variant.lines[m];
                suggestline_t ps2line;
                suggestline_t_pal ps2lineP;

                for(int n = 0; n < line.buttons.size(); n += 1) {
                    suggestbutton_t &button = line.buttons[n];
                    UPLOAD(buttonbase, button);
                }
                if(!isPAL) {
                    ps2line.buttoncount = line.buttons.size();
                    ps2line.ptr_buttons = buttonbase;
                    ps2line.always_zero = 0;
                    ps2line.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 4; t += 1) {ps2line.localisations[t] = u32(~0);}
                    ps2line.owner = line.owner;
                    ps2line.timestamp_start = line.timestamp_start;
                    ps2line.timestamp_end = line.timestamp_end;
                    ps2line.ptr_oops = (ps2line.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2line.oopscount = 6;
                    UPLOAD(linebase, ps2line);
                } else {
                    ps2lineP.buttoncount = line.buttons.size();
                    ps2lineP.ptr_buttons = buttonbase;
                    ps2lineP.always_zero = 0;
                    ps2lineP.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 7; t += 1) {ps2lineP.localisations[t] = u32(~0);}
                    ps2lineP.owner = line.owner;
                    ps2lineP.timestamp_start = line.timestamp_start;
                    ps2lineP.timestamp_end = line.timestamp_end;
                    ps2lineP.ptr_oops = (ps2lineP.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2lineP.oopscount = 6;
                    UPLOAD(linebase, ps2lineP);
                }
            }
            a = linebase; /* linebase is actually after all lines */

        }
        for(int k = 0; k < 17; k += 1) {
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

        if((record.type == 1)) {
            for(int k = 1; k < 4; k += 1) {
                mb[k] = applymetafixup(mb[k], a, false);
            }
        } else {
            mb[record.type] = applymetafixup(mb[record.type], a, false);
        }
        UPLOAD(a, ps2record);
    }

    return true;
}

#undef UPLOAD_SIZE
#undef DOWNLOAD_SIZE

#define OLM_LINK_ADDRESS 0x1CA0000
#define UPLOAD_SIZE(y,x,s) SetFilePointer(hfile, y - OLM_LINK_ADDRESS, NULL, FILE_BEGIN); WriteFile(hfile, LPCVOID(&(x)), s, &written, NULL)
#define DOWNLOAD_SIZE(y,x,s) SetFilePointer(hfile, y - OLM_LINK_ADDRESS, NULL, FILE_BEGIN); ReadFile(hfile, LPVOID(&(x)), s, &written, NULL)

u32 olm_applymetafixup(HANDLE hfile, u32 start, u32 fixup, bool skip) {
    u32 caddr = start;
    u32 ptrinmem = 0;
    u32 oldaddr = 0;
    DWORD written;

    for(;;) {
        SetFilePointer(hfile, caddr - OLM_LINK_ADDRESS, NULL, FILE_BEGIN);
        ReadFile(hfile, LPVOID(&ptrinmem), 4, &written, NULL);

        if((ptrinmem & 0x1000000) == 0) {caddr += 4; continue;}

        if(oldaddr == 0) {oldaddr = ptrinmem;}

        if(ptrinmem != oldaddr) {return caddr;}

        if(!skip) {UPLOAD_NOMOVE(caddr, fixup);}
        caddr += 4;
    }
}

bool olmupload(wchar_t *filename,
    std::vector<e_suggestrecord_t> &records,
    std::vector<commandbuffer_t> *buffers,
    byte *oopsdat, int oopslen,
    u32 datastart, u32 dataend,
    u32 stagemodelistbase, bool isPAL) {
    HANDLE hfile = CreateFileW(
        LPCWSTR(filename),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    DWORD written;

    if(hfile == INVALID_HANDLE_VALUE) return false;

    u32 a = datastart;
    u32 ptr_oops_parap;
    u32 ptr_oops_teach;
    u32 mb[9];
    scenemode_t modes[9];
    //getmetabases(stagemodelistbase, mb);
    u32 count = dataend - datastart + 1;
    if(pcsx2calcsize(records, buffers, oopslen) > count) return false;
    DOWNLOAD_SIZE(stagemodelistbase, modes[0], sizeof(modes));
    for(int i = 0; i < 9; i += 1) {
        mb[i] = a;
        modes[i].ptr_scenecommands = mb[i];
        for(int k = 0; k < buffers[i].size(); k += 1) {UPLOAD(a, buffers[i][k]);}
    }
    UPLOAD_NOMOVE(stagemodelistbase, modes);
    UPLOAD_SIZE(a, oopsdat[0], oopslen);
    ptr_oops_teach = a;
    ptr_oops_parap = ptr_oops_teach + (sizeof(suggestbutton_t) * 6);
    a += oopslen;
    for(int i = 0; i < records.size(); i += 1) {
        e_suggestrecord_t &record = records[i];
        /* FIXME
            Injecting cool record breaks getting worse/better
            and cool mode itself
        */
        //if(record.iscool) continue;
        suggestrecord_t ps2record;
        u32 linebase;
        for(int k = 0; k < 17; k += 1) {
            e_suggestvariant_t &variant = record.variants[k];
            suggestvariant_t &ps2variant = ps2record.variants[k];
            if(variant.islinked) continue;

            int nbuttons = 0;
            for(e_suggestline_t &line : variant.lines) {nbuttons += line.buttons.size();}
            u32 buttonbase = a;
            u32 linebase = buttonbase + (nbuttons * sizeof(suggestbutton_t));

            ps2variant.numlines = variant.lines.size();
            ps2variant.ptr_lines = linebase;

            for(int m = 0; m < variant.lines.size(); m += 1) {
                e_suggestline_t &line = variant.lines[m]; // editor
                suggestline_t ps2line; // ps2 line
                suggestline_t_pal ps2lineP; // ps2 line PAL

                //if(pal) generateLineP(line, ps2line);
                //else generateLine(line, ps2line);
                //in
                for(int n = 0; n < line.buttons.size(); n += 1) {
                    suggestbutton_t &button = line.buttons[n];
                    UPLOAD(buttonbase, button);
                }
                if(!isPAL) {
                    ps2line.buttoncount = line.buttons.size();
                    ps2line.ptr_buttons = buttonbase;
                    ps2line.always_zero = 0;
                    ps2line.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 4; t += 1) {ps2line.localisations[t] = u32(~0);}
                    ps2line.owner = line.owner;
                    ps2line.timestamp_start = line.timestamp_start;
                    ps2line.timestamp_end = line.timestamp_end;
                    ps2line.ptr_oops = (ps2line.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2line.oopscount = 6;
                    UPLOAD(linebase, ps2line);
                } else {
                    ps2lineP.buttoncount = line.buttons.size();
                    ps2lineP.ptr_buttons = buttonbase;
                    ps2lineP.always_zero = 0;
                    ps2lineP.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 7; t += 1) {ps2lineP.localisations[t] = u32(~0);}
                    ps2lineP.owner = line.owner;
                    ps2lineP.timestamp_start = line.timestamp_start;
                    ps2lineP.timestamp_end = line.timestamp_end;
                    ps2lineP.ptr_oops = (ps2lineP.owner & 0x4) ? ptr_oops_parap : ptr_oops_teach;
                    ps2lineP.oopscount = 6;
                    UPLOAD(linebase, ps2lineP);
                }
            }
            a = linebase; /* linebase is actually after all lines */
        }
        for(int k = 0; k < 17; k += 1) {
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

        if((record.type == 1)) {
            for(int k = 1; k < 4; k += 1) {mb[k] = applymetafixup(mb[k], a, false);}
        } else {
            mb[record.type] = applymetafixup(mb[record.type], a, false);
        }
        UPLOAD(a, ps2record);
    }

    CloseHandle(hfile);
    return true;
}
