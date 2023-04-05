#include "pcsx2util.h"
#include "pcsx2reader.h"

bool isPAL = false;

u32 findhdbase(u32 RESOURCE_LIST_BASE) {
    u32 caddr = RESOURCE_LIST_BASE;
    u32 raddr;
    u32 sig;
    int timeout = 20000; //NOTE: 20,000 resources is plenty

    do {
        pcsx2reader::read(caddr, &raddr, 4);
        pcsx2reader::read(raddr, &sig, 4);
        if(sig == HD_SIGNATURE) return caddr;
        caddr += 4;
    } while(timeout--);
    return 0;
}

int getnumhd(u32 hdbase) {
    u32 caddr = hdbase;
    u32 raddr;
    u32 sig;
    int count = 0;
    for(;;) {
        pcsx2reader::read(caddr, &raddr, 4);
        caddr += 4;

        pcsx2reader::read(raddr, &sig, 4);
        if(sig != HD_SIGNATURE) return count;
        count++;
    }
}

u32 findbdbase(u32 RESOURCE_LIST_BASE) {
    u32 hdbase = findhdbase(RESOURCE_LIST_BASE);
    return hdbase + (getnumhd(hdbase) * 4);
}

u32 getvhbasefromhd(u32 hdbase) {
    return hdbase + 0x50; //FIXME: Assuming 0x50 offset
}

u32 getheadfromhd(u32 hdbase) {
    return hdbase + 0x10; //FIXME: Assuming 0x10 offset
}

u32 getsmplfromhd(u32 hdbase) {
    u32 vhbase = getvhbasefromhd(hdbase);
    u32 len;
    pcsx2reader::read(vhbase + 0x8, &len, 4);

    return vhbase + len;
}

u32 getssetfromhd(u32 hdbase) {
    u32 smplbase = getsmplfromhd(hdbase);
    u32 len;
    pcsx2reader::read(smplbase + 0x8, &len, 4);

    return smplbase + len;
}

u32 getprogfromhd(u32 hdbase) {
    u32 ssetbase = getssetfromhd(hdbase);
    u32 len;
    pcsx2reader::read(ssetbase + 0x8, &len, 4);

    return ssetbase + len;
}

int installprograms(u32 hdbase, e_soundboard_t &sb) {
    sb.keys.clear();
    u32 numprograms;
    u32 smplbase = getsmplfromhd(hdbase);
    u32 ssetbase = getssetfromhd(hdbase);
    u32 progbase = getprogfromhd(hdbase);

    u32 smplindexbase = smplbase + 0x10;
    u32 ssetindexbase = ssetbase + 0x10;

    pcsx2reader::read(progbase + 0xC,&numprograms, 4);
    numprograms++;

    u32 pindexbase = progbase + 0x10;

    for(int i = 0; i < numprograms; i++) {
        u32 index;
        pcsx2reader::read(pindexbase + (i * 4), &index, 4);

        u32 progentrybase = progbase + index;
        u32 progentrylen;
        i8 numtokens;

        pcsx2reader::read(progentrybase, &progentrylen, 4);
        pcsx2reader::read(progentrybase + 4, &numtokens, 1);
        u32 progtokenbase = progentrybase + progentrylen;

        for(int k = 0; k < numtokens; k++) {
            u16 ssetindex;
            u32 ssetoffset;
            u16 smplindex;
            u32 smploffset;
            u16 bdindex;
            u8 keyid;

            pcsx2reader::read(progtokenbase + (k * 0x14), &ssetindex, 2);
            pcsx2reader::read(progtokenbase + (k * 0x14) + 2, &keyid, 1);
            pcsx2reader::read(ssetindexbase + (ssetindex * 4), &ssetoffset, 4);
            pcsx2reader::read((ssetbase + ssetoffset) + 4, &smplindex, 2);
            pcsx2reader::read(smplindexbase + (smplindex * 4), &smploffset, 4);
            pcsx2reader::read((smplbase + smploffset), &bdindex, 2);

            sb.prog[i][keyid] = bdindex;
        }
    }
	return sb.keys.size();
}

void pcsx2DwnlKeytable(u32 keybase, int numkeys, e_soundboard_t &sb) {
    sb.keys.clear();
    sb.keys.resize(numkeys);

    for(int i = 0; i < numkeys; i++) pcsx2reader::read(keybase + (6 * i), &sb.keys[i], 6);
}

void pcsx2DwnlSoundboard(u32 hdbase, u32 bdbase, u32 keybase, int numkeys, e_soundboard_t &sb) {
    sb.clear();
    u32 head = getheadfromhd(hdbase);
    u32 vhbase = getvhbasefromhd(hdbase);
    u32 vhcount;
    u32 bdlen;
    e_sound_t vhentry;

    pcsx2reader::read(head + 0x10, &bdlen, 4);

    sb.bd.len = bdlen;
    sb.bd.bytes = (byte*)(malloc(sb.bd.len));
    pcsx2reader::read(bdbase, sb.bd.bytes, sb.bd.len);

    pcsx2reader::read(vhbase + 0xC, &vhcount, 4);
    vhcount++;

    u32 vhindices = vhbase + 0x10;
    u32 vhentries = vhindices + (vhcount * 4);

    sb.sounds.resize(vhcount);
    for(int i = 0; i < vhcount; i++) {
        u32 vhoffset;
        pcsx2reader::read(vhindices + (i * 4), &vhoffset, 4);
        pcsx2reader::read(vhbase + vhoffset, &sb.sounds[i], 8);
    }

    installprograms(hdbase, sb);
    if(keybase) pcsx2DwnlKeytable(keybase, numkeys, sb);
}

void pcsx2GetKeytables(u32 keylistbase, int count, int sbbase, std::vector<e_soundboard_t> &sblist) {
    u32 numkeys;
    u32 ptrkeytable;
    for(int i = 0; i < count; i++) {
        u32 keylistentry = keylistbase + (i * 0x10);
        pcsx2reader::read(keylistentry + 0x8, &numkeys, 4);
        pcsx2reader::read(keylistentry + 0xC, &ptrkeytable, 4);
        pcsx2DwnlKeytable(ptrkeytable, int(numkeys), sblist[sbbase + i]);
    }
}

void pcsx2GetSoundboards(u32 hdlistbase, u32 bdlistbase, u32 count, std::vector<e_soundboard_t> &sblist) {
    sblist.clear();
    u32 hdbase;
    u32 bdbase;
    sblist.resize(count);
    for(int i = 0; i < count; i++) {
        pcsx2reader::read(hdlistbase + (i * 4), &hdbase, 4);
        pcsx2reader::read(bdlistbase + (i * 4), &bdbase, 4);
        pcsx2DwnlSoundboard(hdbase, bdbase, 0,0, sblist[i]);
    }
}

void ps2LineToEditor(const suggestline_t &ps2, const suggestline_t_pal &ps2p, e_suggestline_t &editor) {
    u32 buttoncount = (isPAL ? ps2p.buttoncount : ps2.buttoncount);
    editor.owner = (isPAL ? ps2p.owner : ps2.owner);
    editor.buttons.clear();
    editor.buttons.resize(buttoncount);

    u32 ptrbuttons = (isPAL ? ps2p.ptr_buttons : ps2.ptr_buttons);
    for(int i = 0; i < buttoncount; i++) {
        int loc = ptrbuttons + (sizeof(suggestbutton_t) * i);
        pcsx2reader::read(loc, &editor.buttons[i], sizeof(suggestbutton_t));
    }

    editor.coolmodethreshold = (isPAL ? ps2p.coolmodethreshold : ps2.coolmodethreshold);
    for(int s = 0; s < (isPAL ? 7 : 4); s++) {editor.localisations[s] = (isPAL ? ps2p.localisations[s] : ps2.localisations[s]);}
    editor.timestamp_start = (isPAL ? ps2p.timestamp_start : ps2.timestamp_start);
    editor.timestamp_end = (isPAL ? ps2p.timestamp_end : ps2.timestamp_end);
    editor.always_zero = 0;
}

void ps2VariantToEditor(const suggestvariant_t &ps2, e_suggestvariant_t &editor) {
    editor.lines.clear();
    editor.lines.resize(ps2.numlines);

    suggestline_t ps2line;
    suggestline_t_pal ps2lineP;
    for(int i = 0; i < ps2.numlines; i++) {
        if(!isPAL) pcsx2reader::read(ps2.ptr_lines + (sizeof(suggestline_t) * i), &ps2line, sizeof(suggestline_t));
        else pcsx2reader::read(ps2.ptr_lines + (sizeof(suggestline_t_pal) * i), &ps2lineP, sizeof(suggestline_t_pal));
        
        ps2LineToEditor(ps2line, ps2lineP, editor.lines[i]);
    }
}

void ps2RecordToEditor(const suggestrecord_t &ps2, e_suggestrecord_t &editor) {
    editor.lengthinsubdots = ps2.lengthinsubdots;
    editor.soundboardid = ps2.soundboardid;
    memcpy(editor.unk, ps2.unk, sizeof(editor.unk));

    suggestvariant_t ps2variant;
    for(int i = 0; i < 17; i++) {
        ps2VariantToEditor(ps2.variants[i], editor.variants[i]);
        editor.variants[i].islinked = false;
        editor.variants[i].linknum = -1;
    }

    for(int i = 1; i < 17; i++) {
        for(int k = 0; k < i; k++) {
            if(ps2.variants[i].ptr_lines == ps2.variants[k].ptr_lines) {
                editor.variants[i].islinked = true;
                editor.variants[i].linknum = k;
                k = i; // End "k" loop
            }
        }
    }
}

void pcsx2DwnlRecord(u32 record_addr, e_suggestrecord_t &editor_record) {
    suggestrecord_t ps2_record;
    pcsx2reader::read(record_addr, &ps2_record, sizeof(ps2_record));
    ps2RecordToEditor(ps2_record, editor_record);
}

int pcsx2ReadRecords(u32 stagecmd_start, int count, u32 type, std::vector<e_suggestrecord_t> &records) {
    u32 caddr = stagecmd_start;
    u32 lastrecord = 0;
    e_suggestrecord_t record;
    u32 raddr;
    int nrecords = 0;

    for(int i = 0; i < count; i++, caddr += 0x10) {
        u16 cmd;
        pcsx2reader::read(caddr, &cmd, 2);

        if(cmd == 0x1) {
            pcsx2reader::read(caddr + 0xC, &raddr, 4);

            if((raddr >= 0x1000000) && (raddr != lastrecord)) {
                pcsx2DwnlRecord(raddr, record);
                record.type = type;
                records.push_back(record);
                nrecords++;
                lastrecord = raddr;
            }
        } else if(cmd == 0x9) {
            u16 subcmd;
            pcsx2reader::read(caddr + 0x2, &subcmd, 2);
            if(subcmd == 0xE) { // Check for preload record command
                pcsx2reader::read(caddr + 0x4, &raddr, 4);
                if((raddr >= 0x1000000) && (raddr != lastrecord)) {
                    pcsx2DwnlRecord(raddr, record);
                    record.type = type;
                    records.push_back(record);
                    nrecords++;
                    lastrecord = raddr;
                }
            }
        }
    }
	return records.size();
}

int pcsx2GetRecFromModelist(u32 stagemode_start, std::vector<e_suggestrecord_t> &records, bool extendedSub) {
    scenemode_t modes[9];
    pcsx2reader::read(stagemode_start, modes, sizeof(modes));

    int acc = 0;
    isPAL = extendedSub;
    for(int i = 0; i < 9; i++) {
        if(i == 2 || i == 3) { continue; }
        acc += pcsx2ReadRecords(modes[i].ptr_scenecommands, modes[i].count_scenecommands, i, records);
    }

    return acc;
}

void pcsx2GetComBuffers(u32 stagemode_start, std::vector<commandbuffer_t> *buffers) {
    scenemode_t modes[9];
    pcsx2reader::read(stagemode_start, modes, sizeof(modes));

    commandbuffer_t cb;
    for(int i = 0; i < 9; i++) {
	    buffers[i].clear();
        for(int k = 0; k < modes[i].count_scenecommands; k++) {
            pcsx2reader::read(modes[i].ptr_scenecommands + (k * 0x10), cb.data, 16);
            buffers[i].push_back(cb);
        }
    }
}
