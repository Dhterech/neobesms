#include "conscr.h"
#include "suggest.h"
#include "pcsx2reader.h"
#include "ss.h"
#include "pcsx2util.h"
#include "stageinfo.h"
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <array>
#include <fstream>

#include "config.h"

std::vector<e_suggestrecord_t> records;
std::vector<std::vector<commandbuffer_t>> commands;
std::vector<scenemode_t> modes;

bool isVSMode;
uint32_t modelen = 9;
int oopslen = 0;
byte *oopsdat = nullptr;

std::vector<e_soundboard_t> soundboards;
soundenv_t soundenv;
suggestbutton_t button_clipboard;
currentstage_t currentstage;
std::fstream rawFile;

#define currentrecord records[current_record]

bool menu_options = true;

int current_stage = 0;
int current_record = 0;
int current_variant = 0;

int cursorpos = 0;
int cursorowner = 0;

int paramcursorx = 0;
int paramcursory = 0;
int paramoverride = 0;

int precisioncursor = 0;
int infocursor = 0;

int owners[4] = {0x2, 0x4, 0x8};
int numowners = 3;

#define NGAME_FRAMERATE 60
#define NRESOURCE_LIST_BASE 0x1C49480
#define NCURRENT_STAGE 0x000386930

#define JRESOURCE_LIST_BASE 0x1C59880
#define JCURRENT_STAGE 0x396DCC

#define PGAME_FRAMERATE 50
#define PRESOURCE_LIST_BASE 0x1C46100
#define PCURRENT_STAGE 0x1C63A74

int curReg = DEFAULT_REGION;
int subcount = (DEFAULT_REGION != 1 ? 4 : 7);
int GAME_FRAMERATE[3] = {NGAME_FRAMERATE, PGAME_FRAMERATE, NGAME_FRAMERATE};
int RESOURCE_LIST_BASE[3] = {NRESOURCE_LIST_BASE, PRESOURCE_LIST_BASE, JRESOURCE_LIST_BASE};
int CURRENT_STAGE[3] = {NCURRENT_STAGE, PCURRENT_STAGE, JCURRENT_STAGE};
bool neosubtitles = DEFAULT_SUBS;

wchar_t gbuf[80];
int lastsbload = -1;

const wchar_t *uiRegions[] = { L"NTSC-U", L"PAL", L"NTSC-J" };

const wchar_t *uiVariations[] = {
    L"Orange Hat", 
    L"Lowest",
    L"Awful",
    L"",
    L"Awful", 
    L"Bad", 
    L"Bad +1", 
    L"Bad +2", 
    L"Bad +3", 
    L"Blue Hat", 
    L"Pink Hat", 
    L"Yellow Hat", 
    L"Yellow Hat +1",
    L"Yellow Hat +2",
    L"Yellow Hat +3",
    L"",
    L"Highest",
};

#define FOREGROUND_GRAY (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
#define FOREGROUND_WHITE (FOREGROUND_GRAY | FOREGROUND_INTENSITY)
#define FG_AQUA (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_GREEN (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_PINK (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define FG_RED (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define FG_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_BLUE (FOREGROUND_BLUE | FOREGROUND_INTENSITY)

wchar_t uiButtonChars[] = {
    '?',
    'T',
    'O',
    'X',
    'S',
    'L',
    'R',
    'H'
};

WORD uiButtonColors[] = {
    FOREGROUND_GRAY,
    FG_GREEN,
    FG_RED,
    FG_AQUA,
    FG_PINK,
    FG_YELLOW,
    FG_BLUE,
    FG_YELLOW
};

void loadSoundboard(int id) {
    if(lastsbload == id || id >= soundboards.size()) return;
    soundenv.load(id == -1 ? soundboards[soundboards.size() - 1] : soundboards[id]);
    lastsbload = id;
}

double getTime() {
    LARGE_INTEGER freq;
    LARGE_INTEGER current;                                                                     

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&current);

    LONGLONG numerator = current.QuadPart / freq.QuadPart;
    LONGLONG remainder = current.QuadPart % freq.QuadPart;

    double frac = double(remainder) / double(freq.QuadPart);
    return double(numerator) + frac;
}

bool getButFromSubdot(e_suggestvariant_t &variant, int owner, u32 subdot, suggestbutton_t &button) {
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(!line.containssubdot(subdot)) continue;
        
        for(suggestbutton_t &b : line.buttons) {
            if(b.timestamp + line.timestamp_start == subdot) { button = b; return true; }
        }
    }
    return false;
}

bool getButRefFromSubdot(e_suggestvariant_t &variant, int owner, u32 subdot, suggestbutton_t **button) {
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(!line.containssubdot(subdot)) continue;

        for(suggestbutton_t &b : line.buttons) {
            if(b.timestamp + line.timestamp_start == subdot) { *button = &b; return true; }
        }
    }
    return false;
}

bool getLineRefFromSubdot(e_suggestvariant_t &variant, int owner, u32 subdot, e_suggestline_t **line) {
    for(e_suggestline_t &l : variant.lines) {
        if(!(l.owner & u32(owner))) continue;
        if(l.containssubdot(subdot)) { *line = &l; return true; }
    }
    return false;
}

void playSound(int soundid) {
    loadSoundboard(currentrecord.soundboardid - 1);
    soundenv.play(soundid);
}

struct playtoken_t { double when; int soundid; };
double bpmToSpsd(double bpm) { return (15.0 / bpm) / 24.0; }
bool tokenSorter(playtoken_t a, playtoken_t b) { return (a.when < b.when); }

void playVariant(const e_suggestvariant_t &variant, double bpm, bool tick) {
    std::vector<playtoken_t> tokens; playtoken_t token;
    double spsd = bpmToSpsd(bpm);
    for(const e_suggestline_t &line : variant.lines) {
        for(const suggestbutton_t &button : line.buttons) {
            for(const soundentry_t &se : button.sounds) {
                if(se.soundid == 0xFFFF) continue;
                token.when =
                    (spsd * double(button.timestamp + line.timestamp_start)) +
                    (double(se.relativetime) / double(GAME_FRAMERATE[curReg]));
                token.soundid = se.soundid;
                tokens.push_back(token);
            }
        }
    }
    std::sort(tokens.begin(), tokens.end(), tokenSorter);

    double lineend = spsd * currentrecord.lengthinsubdots;
    int i = 0;
    double rtime = 0;
    double nexttick = 0.0;
    INPUT_RECORD ir;

    conscr::writes(0,0,L" Press any key to stop playback"); conscr::refresh();

    double secondsperbeat = 60.0/bpm;
    double timebase = getTime();
    while(rtime <= lineend + 0.35) {
        rtime = getTime() - timebase;
        if(rtime >= nexttick && tick) { playticker(); nexttick += secondsperbeat; }
        if(tokens.size() != 0 && rtime >= tokens[i].when && i < tokens.size()) { playSound(tokens[i].soundid); i++; }
        if(conscr::hasinput() && conscr::read(ir)) {
            if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) break;
        }
    }
    soundenv.stopAll();
}

void importStageInfo() {
    int tmpReg = (curReg == 2 ? 0 : curReg); // NTSC-J == NTSC
    currentstage.name = stages[current_stage].name;
    currentstage.bpm = stages[current_stage].bpm;
    currentstage.stagemodelistbase = stages[current_stage].regions[tmpReg].stagemodelistbase;
    currentstage.keytablebase = stages[current_stage].regions[tmpReg].keytablebase;
    currentstage.buttondatabase = currentstage.keytablebase + stages[current_stage].regions[tmpReg].keytablesize;
    currentstage.buttondataend = currentstage.stagemodelistbase - 1;
    // Versus Mode Compatibility
    isVSMode = current_stage > 9;
    numowners = (isVSMode) ? 1 : 3;
    oopslen = ((isVSMode) ? 3 : 2) * 6 * sizeof(suggestbutton_t);
    // Modelist Size
    modelen = isVSMode ? 9 : 9; // FIXME: VS Wrong number, black screen
    if(current_stage == 7 || current_stage == 6) modelen = 27; // Stage 8 & 7 Extra data
    if(current_stage == 5) modelen = 24; // Stage 6 Extra data
}

void waitKey() {
    conscr::flushinputs();
    INPUT_RECORD ir;
    while(conscr::read(ir)) {
        if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) break; 
    }
}

void logInfo(const wchar_t *msg) {
    for(int i = 0; i < 80; i++) conscr::putch(i,0,L' ');
    conscr::writes(0,0,msg);
    conscr::refresh();
}

void logWarn(const wchar_t *msg) {
    logInfo(msg);
    waitKey();
}

int importFromEmulator() {
    if(!pcsx2reader::openpcsx2()) return 1;
    logInfo(L" Status: Reading current stage...");
	
    pcsx2reader::read(CURRENT_STAGE[curReg], &current_stage, 1); current_stage--;
    records.clear();

    logInfo(L" Status: Getting stage info & sound database...");
    importStageInfo();
    u32 hdlistbase = findhdbase(RESOURCE_LIST_BASE[curReg]);
    u32 bdlistbase = findbdbase(RESOURCE_LIST_BASE[curReg]);
    int numhd = getnumhd(hdlistbase);

    logInfo(L" Status: Reading game lines...");
    try {
        pcxs2GetModelist(currentstage.stagemodelistbase, modelen, modes);
        pcsx2GetComBuffers(modes, commands);
        pcsx2ParseComRecords(records, commands, isVSMode);
        pcsx2GetSoundboards(hdlistbase, bdlistbase, numhd, soundboards);
        pcsx2GetKeytables(currentstage.keytablebase, numhd, 0, soundboards);
    } catch(...) { return 2; }
    return 0;
}

void drawButton(int x, int y, int buttonid) { conscr::putchcol(x, y, uiButtonChars[buttonid], uiButtonColors[buttonid]); }
void drawCursor(int x, int y) { conscr::putchcol(x, y, L'^', FOREGROUND_GRAY); }

bool isDotOwned(int dot, int owner, e_suggestvariant_t &variant) {
    int subdot = dot * 24;
    for(e_suggestline_t &line : variant.lines) {
        if(line.owner & u32(owner)) if(line.containssubdot(subdot)) return true;
    }
    return false;
}

bool isLineStartingAt(int dot, int owner, e_suggestvariant_t &variant) {
    int mindot = dot * 24;
    int maxdot = mindot + 23;
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if((line.timestamp_start >= mindot) && (line.timestamp_start <= maxdot)) return true;
    }
    return false;
}

e_suggestvariant_t &getCurVariant() {
    int variantId = (currentrecord.variants[current_variant].islinked ? currentrecord.variants[current_variant].linknum : current_variant);
    return currentrecord.variants[variantId];
}

u32 getCurSubdot() { return (cursorpos * 24) + precisioncursor; }
int getCurOwner() { return owners[cursorowner]; }

bool lineSorter(e_suggestline_t &line1, e_suggestline_t &line2) {
    return (line1.timestamp_start < line2.timestamp_start);
}

bool buttonSorter(suggestbutton_t &button1, suggestbutton_t &button2) {
    return (button1.timestamp < button2.timestamp);
}

void doleftexpand(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    int chosen = -1;
    for(int i = 0; i < variant.lines.size(); i++) {
        e_suggestline_t &line = variant.lines[i];
        if(!(line.owner & u32(owner))) continue;
        if(line.timestamp_start < subdot) {
            if(chosen == -1) chosen = i;
            else if(line.timestamp_start > variant.lines[chosen].timestamp_start) chosen = i;
        }
    }
    if(chosen == -1) return;
    e_suggestline_t &line = variant.lines[chosen];
    line.timestamp_end = subdot;
    for(int i = line.buttons.size() - 1; i >= 0; i--) {
        if(!line.containssubdot(line.timestamp_start + line.buttons[i].timestamp)) {
            line.buttons.erase(line.buttons.begin() + i);
        }
    }
}

void dorightexpand(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    e_suggestline_t *pline = nullptr;
    if(!getLineRefFromSubdot(variant, owner, subdot, &pline)) {
        pline = nullptr;
        for(int i = 0; i < variant.lines.size(); i++) {
            e_suggestline_t &line = variant.lines[i];
            if(!(line.owner & u32(owner))) continue;
            if(line.timestamp_start > subdot) {
                if(pline == nullptr) pline = &line;
                else if(line.timestamp_start < pline->timestamp_start) pline = &line;
            }
        }
    }
    if(pline != nullptr) {
        e_suggestline_t &line = *pline;
        std::vector<suggestbutton_t> newbuttons;
        suggestbutton_t tmpbutton;
        int delta = int(subdot) - int(line.timestamp_start);
        line.timestamp_start = subdot;
        for(int i = 0; i < line.buttons.size(); i++) {
            tmpbutton = line.buttons[i];
            tmpbutton.timestamp -= delta;
            if(line.containssubdot(tmpbutton.timestamp + line.timestamp_start)) newbuttons.push_back(tmpbutton);
        }
        line.buttons = newbuttons;
    }
}

void domoveleftline(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    int chosen = -1;
    for(int i = 0; i < variant.lines.size(); i++) {
        e_suggestline_t &line = variant.lines[i];
        if(!(line.owner & u32(owner))) continue;
        if(line.timestamp_start < subdot) {
            if(chosen == -1) chosen = i;
            else if(line.timestamp_start > variant.lines[chosen].timestamp_start) chosen = i;
        }
    }
    if(chosen != -1) {
        e_suggestline_t &line = variant.lines[chosen];
        int delta = int(subdot) - int(line.timestamp_start);
        line.timestamp_start = subdot;
        line.timestamp_end += delta;
    }
}

void domoverightline(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    e_suggestline_t *pline = nullptr;
    if(!getLineRefFromSubdot(variant, owner, subdot, &pline)) {
        pline = nullptr;
        for(int i = 0; i < variant.lines.size(); i++) {
            e_suggestline_t &line = variant.lines[i];
            if(!(line.owner & u32(owner))) continue;
            if(line.timestamp_start > subdot) {
                if(pline == nullptr) pline = &line;
                else if(line.timestamp_start < pline->timestamp_start) pline = &line;
            }
        }
    }
    if(pline != nullptr) {
        e_suggestline_t &line = *pline;
        int delta = int(subdot) - int(line.timestamp_start);
        line.timestamp_start = subdot;
        line.timestamp_end += delta;
    }
}

void createLine(u32 subdot_start, u32 subdot_end) {
    e_suggestvariant_t &variant = getCurVariant();
    e_suggestline_t *line;
    if(getLineRefFromSubdot(variant, getCurOwner(), subdot_start, &line)) return;
    
    e_suggestline_t newline;
    newline.owner = getCurOwner();
    newline.timestamp_start = subdot_start;
    newline.timestamp_end = subdot_end;
    newline.coolmodethreshold = (newline.owner == owners[2] ? -1 : 150);
    for(int i = 0; i < subcount; i++) newline.localisations[i] = NULL;
    newline.vs_count = 0;

    variant.lines.push_back(newline);
    std::sort(variant.lines.begin(), variant.lines.end(), lineSorter);
}

void createButton(u32 subdot, int owner, int buttonid) {
    e_suggestvariant_t &variant = getCurVariant();
    suggestbutton_t newbutton;

    /* First check if the button exists, if yes, just change button id and leave */
    suggestbutton_t *bref;
    if(getButRefFromSubdot(variant, owner, subdot, &bref)) { bref->buttonid = buttonid; return; }

    newbutton.buttonid = buttonid;
    for(soundentry_t &e : newbutton.sounds) {
        e.always_zero = 0;
        e.soundid = ~0;
        e.animationid = ~0;
        e.unk = ~0;
        e.relativetime = ~0;
    }

    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(!line.containssubdot(subdot)) continue;

        newbutton.timestamp = subdot - line.timestamp_start;
        line.buttons.push_back(newbutton);
        std::sort(line.buttons.begin(), line.buttons.end(), buttonSorter);
    }
}

void deleteButton(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(!line.containssubdot(subdot)) continue;

        u32 rsubdot = subdot - line.timestamp_start;
        for(int i = 0; i < line.buttons.size(); i++) {
            if(line.buttons[i].timestamp == rsubdot) {
                line.buttons.erase(line.buttons.begin() + i);
                return;
            }
        }
    }
}

void changeButParameter(int amount, bool setMode) {
    suggestbutton_t *button;
    if(getButRefFromSubdot(getCurVariant(), getCurOwner(), getCurSubdot(), &button)) {
        soundentry_t &se = button->sounds[paramcursory];
        switch(paramcursorx) {
        case 0:
            if(setMode) se.soundid = conscr::query_hex(L" Input: Type new SND (hex): ");
            else se.soundid += amount;
            playSound(se.soundid);
            break;
        case 1:
            if(setMode) se.animationid = conscr::query_hex(L" Input: Type new ANIM (hex): ");
            else se.animationid += amount;
            break;
        case 2:
            if(setMode) se.relativetime = conscr::query_decimal(L" Input: Type new TIME (dec): ");
            else se.relativetime += amount;
            break;
        }
    }
}

void setRecSoundboard() {
    int sbid = conscr::query_decimal(L" Input: Enter a new soundboard number: ");
    if(sbid > soundboards.size() - 1 || sbid < 0) { logWarn(L" Error: Soundboard number too high/low"); return;}
    currentrecord.soundboardid = sbid;
}

void copyButton(u32 subdot) {
    suggestbutton_t button;
    if(getButFromSubdot(getCurVariant(), getCurOwner(), subdot, button)) button_clipboard = button;
}

void cutButton(u32 subdot) {
    copyButton(subdot);
    deleteButton(subdot, getCurOwner());
}

void pasteButton(u32 subdot) {
    suggestbutton_t *pbutton;
    deleteButton(subdot, getCurOwner());
    createButton(subdot, getCurOwner(), 1);

    if(getButRefFromSubdot(getCurVariant(), getCurOwner(), subdot, &pbutton)) {
        pbutton->buttonid = button_clipboard.buttonid;
        for(int i = 0; i < 4; i++) pbutton->sounds[i] = button_clipboard.sounds[i];
    }
}

void deleteLine(int subdot, int owner) {
    e_suggestvariant_t &variant = getCurVariant();
    for(int i = 0; i < variant.lines.size(); i++) {
        e_suggestline_t &line = variant.lines[i];
        if(!(line.owner & u32(owner))) continue;
        if(!line.containssubdot(subdot)) continue;

        variant.lines.erase(variant.lines.begin() + i);
        return;
    }
}

void setInfoLineCool(int amount) {
    e_suggestline_t *line;
    if(!getLineRefFromSubdot(getCurVariant(), getCurOwner(), getCurSubdot(), &line)) return;
    line->coolmodethreshold = amount;
}

void setInfoRecordLink(int linkid) {
    if(linkid >= 17) {
        swprintf(gbuf, 80, L" Error: Link identifier too high (%d)", linkid);
        logWarn(gbuf);
        return;
    } else if(linkid == current_variant || linkid < 0) {
        e_suggestvariant_t &variant = currentrecord.variants[current_variant];
        variant.islinked = false;
        variant.linknum = -1;
        return;
    }
    if(currentrecord.variants[linkid].islinked) {
        logWarn(L" Error: You can't link to a linked variant");
        return;
    }
    e_suggestvariant_t &variant = currentrecord.variants[current_variant];
    variant.islinked = true;
    variant.linknum = linkid;
}

void adjustInfo() {
    switch(infocursor) {
    case 0:
        setInfoRecordLink(conscr::query_decimal(L" Input: Type variant to link, or -1 to delete: "));
        break;
    case 1:
        setInfoLineCool(conscr::query_decimal(L" Input: Type new threshold: "));
        break;
    }
}

void linkEverything() {
    for(int i = 1; i < 17; i++) {
        currentrecord.variants[i].islinked = true;
        currentrecord.variants[i].linknum = 0;
    }
}

void onEditorKey(int k, wchar_t uc, bool shiftmod) {
    // Line Cursor
    if(k == 'D') {
        int maxindex = (currentrecord.lengthinsubdots / 24) - 1;
        if(cursorpos < maxindex) cursorpos++;
        if(shiftmod) cursorpos = (cursorpos + 3) & (~3);
    }
    else if(k == 'A') {
        if(cursorpos > 0) cursorpos--;
        if(shiftmod) cursorpos &= (~3);
    }
    else if(k == 'S') {
        if(shiftmod) {
            if(owners[cursorowner] == 0) owners[cursorowner] = 1;
            else owners[cursorowner] = int(u32(owners[cursorowner]) << 1);
        } else {
            if(cursorowner < numowners - 1) cursorowner++;
        }
    }
    else if(k == 'W') {
        if(shiftmod) {
            if(owners[cursorowner] == 0) owners[cursorowner] = int(u32(0x80000000));
            else owners[cursorowner] = int(u32(owners[cursorowner]) >> 1);
        } else {
            if(cursorowner > 0) cursorowner--;
        }
    }
    else if(k == 'Q') {
        if(precisioncursor > 0) precisioncursor--;
        if(shiftmod) precisioncursor &= (~3);
    }
    else if(k == 'E') {
        if(precisioncursor < 23) precisioncursor++;
        if(shiftmod) precisioncursor = (precisioncursor + 3) & (~3);
    }
	// Param cursor
    else if(k == 'I') {
        if(paramcursory > 0) paramcursory--;
    }
    else if(k == 'K') {
        if(paramcursory < 3) paramcursory++;
    }
    else if(k == 'J') {
        if(paramcursorx > 0) paramcursorx--;
    }
    else if(k == 'L') {
        if(paramcursorx < 2) paramcursorx++;
    }
    else if(k == 'O') { changeButParameter(shiftmod ? 0x100 : 1, false); }
    else if(k == 'U') { changeButParameter((shiftmod ? 0x100 : 1) * -1, false); }
    else if(k == 'M') { doleftexpand(cursorpos * 24 + 24, getCurOwner()); }
    else if(uc == L'[' || uc == L'{') {
        if(shiftmod) domoveleftline(cursorpos * 24, getCurOwner());
		else doleftexpand(cursorpos * 24 + 24, getCurOwner());
    }
    else if(uc == L']' || uc == L'}') {
        if(shiftmod) domoverightline(cursorpos * 24, getCurOwner());
        else dorightexpand(cursorpos * 24, getCurOwner());
    }
    // Variant/Difficulty
    else if(k == VK_DOWN) {
        if(current_variant < 16) current_variant++;
    }
    else if(k == VK_UP) {
        if(current_variant > 0) current_variant--;
    }
    else if(k == VK_LEFT) {
        if(current_record > 0) current_record--;
    }
    else if(k == VK_RIGHT) {
        if(current_record < records.size() - 1) current_record++;
    }
	// Line Creation/Deleting
    else if(k == VK_SPACE) { createLine(cursorpos * 24, cursorpos * 24 + 24); }
    else if(k == VK_BACK) { deleteLine(getCurSubdot(), getCurOwner()); }
	// Button management
    #define dcb(x) createButton(getCurSubdot(), getCurOwner(), x)
    else if(k == '1') { dcb(1); }
    else if(k == '2') { dcb(2); }
    else if(k == '3') { dcb(3); }
    else if(k == '4') { dcb(4); }
    else if(k == '5') { dcb(5); }
    else if(k == '6') { dcb(6); }
    else if(k == '7') { dcb(0); }
    else if(k == '0') { deleteButton(getCurSubdot(), getCurOwner()); }
    else if(k == 'X') { cutButton(getCurSubdot()); }
    else if(k == 'C') { copyButton(getCurSubdot()); }
    else if(k == 'V') { pasteButton(getCurSubdot()); }
    else if(k == 'B') { setRecSoundboard(); }
    else if(k == 'P') { playVariant(getCurVariant(), stages[current_stage].bpm, !shiftmod); }
    else if(k == VK_TAB) {
        infocursor++;
        if(infocursor > 1) infocursor = 0;
    }
    else if(uc == '`' || uc == '~') { adjustInfo(); }
    else if(k == VK_RETURN) { changeButParameter(0, true); }
    else if(k == VK_ESCAPE) { menu_options = true; }
    else if(k == VK_F8) {
        linkEverything();
    }
    else if(k == VK_F5) {
        importStageInfo();
        u32 totalsize = pcsx2calcsize(records, commands, oopslen, modelen, (curReg == 1));
        u32 origsize = currentstage.buttondataend - currentstage.buttondatabase + 1;
        if(totalsize > origsize) { logWarn(L" Error: Data too large! Link some lines to spare space."); return; }
        logInfo(L" Status: Uploading to PCSX2...");
        if(modes.size() == 0) pcxs2GetModelist(currentstage.stagemodelistbase, modelen, modes);
        bool result = pcsx2upload(records, commands, modes, oopsdat, oopslen, modelen, currentstage, (curReg == 1), neosubtitles, isVSMode);
        if(result) logWarn(L" Info: Current lines are injected in PCSX2.");
        else logWarn(L" Error: There was an error injecting in PCSX2.");
    }
}

u32 tmpu32;
i32 tmpi32;

#define WRITE(x) rawFile.write(reinterpret_cast<char *>(&x), sizeof(x));
void saveScene(std::vector<commandbuffer_t> &buffer) {
    tmpu32 = buffer.size(); WRITE(tmpu32);
    for(commandbuffer_t &cmd : buffer) { WRITE(cmd); }
}
int saveProject(wchar_t *name) {
    rawFile.open(name, std::ios_base::out | std::ios_base::binary);
    if(!rawFile.is_open()) return GetLastError();

    WRITE(current_stage);
    for(int i = 0; i < 9; i++) saveScene(commands[i]);

    tmpu32 = records.size(); WRITE(tmpu32);
    for(e_suggestrecord_t &record : records) {
        WRITE(record.type);
        WRITE(record.lengthinsubdots);
        WRITE(record.soundboardid);
        for(e_suggestvariant_t &variant : record.variants) {
            tmpi32 = variant.islinked ? variant.linknum : -1; WRITE(tmpi32);
            tmpu32 = variant.lines.size(); WRITE(tmpu32);
            for(e_suggestline_t &line : variant.lines) {
                WRITE(line.vs_count);
                WRITE(line.coolmodethreshold);
				if(neosubtitles) {for(int susb = 0; susb < subcount; susb++) {
					WRITE(line.localisations[susb]);
				}}
                WRITE(line.owner);
                WRITE(line.timestamp_start);
                WRITE(line.timestamp_end);
                WRITE(line.oopscount);
                WRITE(line.ptr_oops);
                
                tmpu32 = line.buttons.size();
                WRITE(tmpu32);
                for(suggestbutton_t &button : line.buttons) WRITE(button);
            }
        }
        if(isVSMode) WRITE(record.vs_data);
    }

    tmpu32 = soundboards.size(); WRITE(tmpu32);
    for(e_soundboard_t &sb : soundboards) {
        WRITE(sb.bd.len);
        rawFile.write(reinterpret_cast<char *>(sb.bd.bytes), sb.bd.len);

        tmpu32 = sb.keys.size(); WRITE(tmpu32);
        for(ptrkey_t &key : sb.keys) WRITE(key);
        WRITE(sb.prog);
        tmpu32 = sb.sounds.size(); WRITE(tmpu32);
        for(e_sound_t &sound : sb.sounds) WRITE(sound);
    }

    if(modelen > 9) { // NeoBesms Extra Data
        WRITE(modelen);
        for(int i = 9; i < modelen; i++) saveScene(commands[i]); // Stage 8 / VS
    }
    
    rawFile.close();
    return 0;
}
#undef WRITE

#define READ(x) rawFile.read(reinterpret_cast<char *>(&x), sizeof(x));
void loadScene() {
    std::vector<commandbuffer_t> buffer;
    READ(tmpu32); buffer.resize(tmpu32);
    for(commandbuffer_t &cmd : buffer) READ(cmd);
    commands.push_back(buffer);
}

int loadProject(wchar_t *name) {
    rawFile.open(name, std::ios_base::in | std::ios_base::binary);
    if(!rawFile.is_open()) return GetLastError();

    commands.clear();
    records.clear();
    soundboards.clear();
	
    READ(tmpu32); current_stage = tmpu32;
    importStageInfo();

    for(int i = 0; i < 9; i++) loadScene();

    READ(tmpu32); records.resize(tmpu32);
    for(e_suggestrecord_t &record : records) {
        READ(record.type);
        READ(record.lengthinsubdots);
        READ(record.soundboardid);
        for(e_suggestvariant_t &variant : record.variants) {
            READ(tmpi32); variant.islinked = bool(tmpi32 >= 0);
            variant.linknum = tmpi32;

            READ(tmpu32); variant.lines.resize(tmpu32);
            for(e_suggestline_t &line : variant.lines) {
                READ(line.vs_count);
                READ(line.coolmodethreshold);
				if(neosubtitles) {for(int susb = 0; susb < subcount; susb++) {
					READ(line.localisations[susb]);
				}}
                READ(line.owner);
                READ(line.timestamp_start);
                READ(line.timestamp_end);
                READ(line.oopscount);
                READ(line.ptr_oops);
                READ(tmpu32);
                line.buttons.resize(tmpu32);
                for(suggestbutton_t &button : line.buttons) READ(button);
            }
        }
        if(isVSMode) READ(record.vs_data);
    }

    READ(tmpu32); soundboards.resize(tmpu32);
    for(e_soundboard_t &sb : soundboards) {
        READ(sb.bd.len);
        sb.bd.bytes = (byte *)malloc(sb.bd.len);
        rawFile.read(reinterpret_cast<char*>(sb.bd.bytes), sb.bd.len);
		
        READ(tmpu32); sb.keys.resize(tmpu32);
        for(int k = 0; k < sb.keys.size(); k++) READ(sb.keys[k]);

        READ(sb.prog);
        READ(tmpu32); sb.sounds.resize(tmpu32);
        for(int k = 0; k < sb.sounds.size(); k++) READ(sb.sounds[k]);
    };

    if(rawFile.tellg() != EOF) { // NeoBesms Extra data
        READ(tmpu32); modelen = tmpu32;
        if(modelen > 9) { // Extra scenes
            for(int i = 9; i < modelen; i++) loadScene(); // Not loading this fixes the issue
        }
    } else {
        modelen = 9;
    }

    rawFile.close();

    getProjectRecordAddresses(records, commands, isVSMode);
    return 0;
}
#undef READ

// menu

void drawOptions() {
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_GRAY);
    
    conscr::writes(1, 1, L"NeoBESMS 23/09/2023");
    conscr::writes(1, 2, uiRegions[curReg]);

    conscr::writes(1, 4, L"[F01] Save project");
    conscr::writes(1, 5, L"[F03] Load project");

    conscr::writes(1, 7, L"[F05] Upload OLM file");
    //conscr::writes(1, 8, (neosubtitles == false ? L"[F06] Toggle subtitles [Disabled]" : L"[F06] Toggle subtitles [Enabled]"));
    conscr::writes(1, 8, L"[F10] Toggle game region");
    conscr::writes(1, 9, L"[F09] Download from PCSX2");
    if(records.size() != 0) conscr::writes(1, 11, L"[ESC] Return to editor");

    conscr::refresh();
}

void onOptionsKey(int k, bool shiftmod) {
    wchar_t filename[MAX_PATH];
    if(k == VK_F9) {
        current_record = 0; current_variant = 0; lastsbload = -1;
        if(shiftmod) pcsx2reader::setBaseAddr(conscr::query_hexU(L" Input: EE Memory Start Address (hex): "));
        int result = importFromEmulator();
        if(result == 0) menu_options = false;
        else if(result == 1) logWarn(L" Error: PCSX2 process not found!");
        else logWarn(L" Error: Cannot load data correctly. Wrong region maybe?");
    } else if(k == VK_F1) {
        if(records.size() == 0) { logWarn(L" Error: There is nothing loaded to save"); return; }
        if(!conscr::query_string(L" Input: Save as: ", filename, MAX_PATH)) return;
        int result = saveProject(filename);
        if(result == 0) { swprintf(gbuf, 80, L" Info: Saved as %ls", filename); logWarn(gbuf); }
		else { swprintf(gbuf, 80, L" Error: Couldn't save %ls, Error: %hs", filename, strerror(result)); logWarn(gbuf); }
    } else if(k == VK_F3) {
        current_record = 0; current_variant = 0; lastsbload = -1;
        if(!conscr::query_string(L" Input: Load Path: ", filename, MAX_PATH)) return;
        int result = loadProject(filename);
        if(result == 0) { menu_options = false; }
		else { swprintf(gbuf, 80, L" Error: Couldn't open %ls, Error: %hs", filename, strerror(result)); logWarn(gbuf); }
    } //else if(k == VK_F6) { neosubtitles = !neosubtitles; }
    else if(k == VK_F5) {
        if(conscr::query_string(L" Input: OLM File Path: ", filename, MAX_PATH) == 0) return;
        importStageInfo();
        if(olmupload(filename)) { swprintf(gbuf, 80, L" Info: Uploaded OLM file: %ls", filename); logWarn(gbuf); }
        else { swprintf(gbuf, 80, L" Error: Failed to upload OLM file: %ls", filename); logWarn(gbuf); }
	} else if(k == VK_F10) {
		curReg++;
        if(curReg > 2) curReg = 0;
        subcount = (curReg != 1 ? 4 : 7); // NTSC 4 subs, PAL 7 subs
    } else if(k == VK_ESCAPE) {
	    if(records.size() == 0) logWarn(L" Error: There is no data to edit. Please load something from the menu.");
		else menu_options = false;
    }
}

void drawvariant_base(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    int ndots = length / 24;
    for(int i = 0; i < ndots; i++) {
        wchar_t chartoput = !(i % 4) ? L'*' : L'-';
        WORD attrtoput = isDotOwned(i, owner, variant) ? FOREGROUND_GRAY : FOREGROUND_INTENSITY;
        conscr::putchcol(x + i, y, chartoput, attrtoput);
    }
}

void drawlinemarkers(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    int ndots = length / 24;
    for(int i = 0; i < ndots; i++) {
        if(isLineStartingAt(i, owner, variant)) conscr::putchcol(x + i, y, L'$', FOREGROUND_GRAY);
    }
}

void drawprecision_base(int x, int y, int owner, e_suggestvariant_t &variant) {
    WORD attrtoput = isDotOwned(cursorpos, owner, variant) ? FOREGROUND_GRAY : FOREGROUND_INTENSITY;
    for(int i = 0; i < 24; i++) {
        wchar_t chartoput = !(i % 4) ? L'*' : L'-';
        conscr::putchcol(x + i, y, chartoput, attrtoput);
    }
}

void drawprecision_buttons(int x, int y, int owner, e_suggestvariant_t &variant) {
    u32 subdotbase = cursorpos * 24;
    suggestbutton_t button;
    for(int i = 0; i < 24; i++) {
        if(getButFromSubdot(variant, owner, subdotbase + i, button)) drawButton(x + i, y, button.buttonid);
    }
}

void drawvariant(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    suggestbutton_t button;
    for(int i = 0; i < length; i++) {
        if(getButFromSubdot(variant, owner, i, button)) drawButton((i / 24) + x, y, button.buttonid);
    }
}

const wchar_t *ownernames[] = {
    L"None",
    L"???",
    L"Teacher",
    L"PaRappa",
    L"SFX",
    L"Scene"
};

void drawownername(int x, int y, int ownerid) {
    int cid = 0;
    u32 tmp = u32(ownerid);
    while(tmp != 0) { tmp >>= 1; cid++; }

    if(cid >= 6 || cid < 0) { swprintf(gbuf, 80, L"%x", ownerid); conscr::writes(x,y,gbuf); }
    else conscr::writes(x,y,ownernames[cid]);
}

void drawrecord(e_suggestrecord_t &record, e_suggestvariant_t &variant) {
    for(int i = 0; i < numowners; i++) {
        drawownername(1, 2 + (i*3), owners[i]);
        drawlinemarkers(10, 1 + (i*3), owners[i], variant, record.lengthinsubdots);
        drawvariant_base(10, 2 + (i*3), owners[i], variant, record.lengthinsubdots);
        drawvariant(10, 2 + (i*3), owners[i], variant, record.lengthinsubdots);
    }

    drawprecision_base(14, 11, getCurOwner(), variant);
    drawprecision_buttons(14, 11, getCurOwner(), variant);

    drawCursor(10 + cursorpos, 3 + (cursorowner * 3));
    drawCursor(14 + precisioncursor, 12);
}

void drawbuttonparameters(int x, int y, suggestbutton_t &button) {
    conscr::putchcol(x + (paramcursorx * 5) + 5, y, L'v', FOREGROUND_GRAY);
    conscr::writescol(x + 5, y + 1, L"SND  ANIM TIME", FOREGROUND_GRAY);
    for(int i = 0; i < 4; i++) {
        soundentry_t &se = button.sounds[i];
        swprintf(gbuf, 80, L"[%d] %04x %04x %04d", i, se.soundid, se.animationid, se.relativetime);
        if(paramcursory == i) conscr::putchcol(x, y + (i * 2) + 2, L'>', FOREGROUND_GRAY);
        conscr::writescol(x + 1, y + (i * 2) + 2, gbuf, (se.soundid != 0xFFFF) ? FOREGROUND_GRAY : FOREGROUND_INTENSITY);
    }
}

void drawlineparameters(int x, int y) {
    e_suggestline_t *line;
    if(!getLineRefFromSubdot(getCurVariant(), getCurOwner(), getCurSubdot(), &line)) return;

    swprintf(gbuf, 80, L"COOL: %d", line->coolmodethreshold);
    conscr::writescol(x + 1, y, gbuf, FOREGROUND_GRAY);
    if(infocursor == 1) conscr::putchcol(x - 1, y, L'>', FOREGROUND_GRAY);
}

void drawinfo(int x, int y) {
    swprintf(gbuf, 80, L"Stage: %d  %ls", current_stage + 1, stages[current_stage].name);
    conscr::writescol(x, y, gbuf, FOREGROUND_GRAY);

    swprintf(gbuf, 80, L"Record: %d (%d) SoundB: %d", current_record, records[current_record].type, currentrecord.soundboardid);
    conscr::writescol(x, y+1, gbuf, FOREGROUND_GRAY);

    swprintf(gbuf, 80, L"Variant: %d  %ls", current_variant, uiVariations[current_variant]);
    conscr::writescol(x, y+2, gbuf, FOREGROUND_GRAY);

    if(infocursor == 0) conscr::putchcol(x-2,y+3,L'>', FOREGROUND_GRAY);
    if(currentrecord.variants[current_variant].islinked) {
        swprintf(gbuf, 80, L"Linked: Variant %d", currentrecord.variants[current_variant].linknum);
        conscr::writescol(x, y+3, gbuf, FOREGROUND_WHITE);
    } else conscr::writescol(x, y+3, L"Not linked", FOREGROUND_INTENSITY);

    u32 totalsize = pcsx2calcsize(records, commands, oopslen, modelen, (curReg == 1));
    u32 origsize = currentstage.buttondataend - currentstage.buttondatabase + 1;

    WORD attr = (totalsize > origsize) ? (FG_RED) : (FG_GREEN);

    swprintf(gbuf, 80, L"%d / %d bytes", totalsize, origsize);
    conscr::writescol(x, y+5, gbuf, attr);
}

void drawEditor() {
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);

    drawrecord(currentrecord, getCurVariant());

    suggestbutton_t button;
    bool s = getButFromSubdot(getCurVariant(), getCurOwner(), getCurSubdot(), button);
    if(s) drawbuttonparameters(1, 13, button);

    drawlineparameters(60, 21);
    drawinfo(50, 14);

    conscr::refresh();
}

void sandbox() {
    INPUT_RECORD ir;
    for(;;) {
        SwitchToThread();
		while(conscr::hasinput() && conscr::read(ir)) {
			if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
				if(menu_options) onOptionsKey(ir.Event.KeyEvent.wVirtualKeyCode, ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED);
                else onEditorKey(ir.Event.KeyEvent.wVirtualKeyCode, ir.Event.KeyEvent.uChar.UnicodeChar, ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED);
			}
			if(menu_options) { drawOptions(); } else { drawEditor(); }
		}
    }
}

void initconsole() {
    conscr::init();
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    conscr::refresh();
}

int main() {
    initconsole();
    initsound(GetConsoleWindow());
    loadticker();
    sandbox();
    try { sandbox(); } catch(...) {
        saveProject(L"last-recover");
        logWarn(L" Sorry, a error occurred while running NeoBESMS. The project loaded was exported to 'last-recover'.");
    }
}