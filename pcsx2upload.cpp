#include "suggest.h"
#include "pcsx2reader.h"
#include "stageinfo.h"
#include <vector>
#include <fstream>
#include <memory.h>

u32 pcsx2calcsize(std::vector<e_suggestrecord_t> &records, std::vector<std::vector<commandbuffer_t>> &commands, int oopslen, int modelen, bool isPAL) {
    u32 size = sizeof(suggestrecord_t) * records.size();

    for(e_suggestrecord_t &record : records) {
        for(e_suggestvariant_t &variant : record.variants) {
            if(variant.islinked) continue;
            int linesize = (isPAL ? sizeof(suggestline_t_pal) : sizeof(suggestline_t));
            size += linesize * variant.lines.size();
            for(e_suggestline_t &line : variant.lines) {
                size += sizeof(suggestbutton_t) * line.buttons.size();
            }
        }
    }

    for(int i = 0; i < modelen; i++) size += sizeof(commandbuffer_t) * commands[i].size();
    return size + oopslen;
}

#define UPLOAD_NOMOVE(y,x) UPLOAD_SIZE(y,x,sizeof(x))
#define UPLOAD(y, x) UPLOAD_NOMOVE(y,x); y += sizeof(x);

#define UPLOAD_SIZE(y,x,s) pcsx2reader::write(y, &(x), s)
#define DOWNLOAD_SIZE(y,x,s) pcsx2reader::read(y, &(x), s)

void updateRecordScCommands(int type, int endtype, u32 oldPointer, u32 newPointer, std::vector<std::vector<commandbuffer_t>> &commands) {
    for(int i = type; i <= endtype; i++) {
        for(commandbuffer_t &buffer : commands[i]) {
            if(buffer.cmd_id != SCENECMD_SETRECORD && buffer.cmd_id != SCENECMD_ACTIVATE) continue;
            if(buffer.cmd_id == SCENECMD_ACTIVATE && buffer.arg1 != 0xE) continue;

            if(buffer.arg2 == oldPointer) buffer.arg2 = newPointer;
            if(buffer.arg4 == oldPointer) buffer.arg4 = newPointer;
        }
    }
}

bool pcsx2upload(
    std::vector<e_suggestrecord_t> &records,
    std::vector<std::vector<commandbuffer_t>> &commands,
    std::vector<scenemode_t> &modes,
    byte *oopsdat, int oopslen, int modelen,
    currentstage_t currentstage,
    bool isPAL, bool kSubs, bool isVSMode) {
    u32 uploadPos = currentstage.buttondatabase;
    
    uploadPos += oopslen; // skip oops buttons

    for(int i = 0; i < records.size(); i++) {
        e_suggestrecord_t &record = records[i];
        suggestrecord_t ps2record;

        if(i > 1 && record.type == 0) continue; // do not upload ptr2besms repeated cool
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

            for(e_suggestline_t &line : variant.lines) {
                suggestline_t ps2line;
                suggestline_t_pal ps2lineP;

                if(!isPAL) {
                    ps2line.buttoncount = line.buttons.size();
                    ps2line.ptr_buttons = buttonbase;
                    for(int n = 0; n < line.buttons.size(); n++) { UPLOAD(buttonbase, line.buttons[n]);}
                    ps2line.vs_count = line.vs_count;
                    ps2line.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 4; t++) {ps2line.localisations[t] = (kSubs) ? line.localisations[t] : u32(~0);}
                    ps2line.owner = line.owner;
                    ps2line.timestamp_start = line.timestamp_start;
                    ps2line.timestamp_end = line.timestamp_end;
                    ps2line.ptr_oops = line.ptr_oops;
                    ps2line.oopscount = line.oopscount;
                    UPLOAD(linebase, ps2line);
                } else {
                    ps2lineP.buttoncount = line.buttons.size();
                    ps2lineP.ptr_buttons = buttonbase;
                    for(int n = 0; n < line.buttons.size(); n++) { UPLOAD(buttonbase, line.buttons[n]); }
                    ps2lineP.vs_count = line.vs_count;
                    ps2lineP.coolmodethreshold = line.coolmodethreshold;
                    for(int t = 0; t < 7; t++) {ps2lineP.localisations[t] = (kSubs) ? line.localisations[t] : u32(~0);}
                    ps2lineP.owner = line.owner;
                    ps2lineP.timestamp_start = line.timestamp_start;
                    ps2lineP.timestamp_end = line.timestamp_end;
                    ps2lineP.ptr_oops = line.ptr_oops;
                    ps2lineP.oopscount = line.oopscount;
                    UPLOAD(linebase, ps2lineP);
                }
            }
            uploadPos = linebase; /* linebase is actually after all lines */
        }
        for(int k = 0; k < 17; k++) {
            e_suggestvariant_t &variant = record.variants[k];
            suggestvariant_t &ps2variant = ps2record.variants[k];
            if(!variant.islinked) continue;

            suggestvariant_t &linkedto = ps2record.variants[variant.linknum];
            ps2variant.numlines = linkedto.numlines;
            ps2variant.ptr_lines = linkedto.ptr_lines;
        }
        ps2record.lengthinsubdots = record.lengthinsubdots;
        ps2record.soundboardid = record.soundboardid;
        memcpy(ps2record.vs_data, record.vs_data, sizeof(ps2record.vs_data));

        if(record.type == 1 && !isVSMode) { // Good, Bad & Awful
            updateRecordScCommands(1, 3, record.address, uploadPos, commands);
        } else { // Other scenes
            updateRecordScCommands(record.type, record.type, record.address, uploadPos, commands);
        }
        UPLOAD(uploadPos, ps2record);
    }
 
    for(int i = 0; i < modelen; i++) {
        modes[i].ptr_scenecommands = uploadPos;
        for(int k = 0; k < commands[i].size(); k++) {UPLOAD(uploadPos, commands[i][k]);}
        UPLOAD_NOMOVE(currentstage.stagemodelistbase + (i * sizeof(scenemode_t)), modes[i]);
    }
    
    return true;
}

#undef UPLOAD_SIZE
#undef DOWNLOAD_SIZE

bool olmupload(wchar_t *filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    u32 fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    char* buffer = new char[fileSize];
    if (!file.read(buffer, fileSize)) { delete[] buffer; return false; }
    int result = pcsx2reader::write(OLM_LINK_ADDRESS, buffer, fileSize);

    delete[] buffer;
    return result;
}