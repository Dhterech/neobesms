#include "conscr.h"
#include "suggest.h"
#include "pcsx2reader.h"
#include "ss.h"
#include "adpcm.h"
#include "pcsx2util.h"
#include "stageinfo.h"
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <array>

#include "config.h"

#define snwprintf swprintf

std::vector<e_suggestrecord_t> records;
std::vector<commandbuffer_t> commands[9];

int oopslen;
byte *oopsdat;

std::vector<e_soundboard_t> soundboards;
soundenv_t soundenv;
suggestbutton_t button_clipboard;
stageinfo_t currentstageinfo;

#define currentrecord records[current_record]

bool menu_options = true;

int current_record = 0;
int current_variant = 0;

int cursorpos = 0;
int cursorowner = 0;

int paramcursorx = 0;
int paramcursory = 0;
int paramoverride = 0;

int precisioncursorpos = 0;
int infocursor = 0;

int owners[4] = {0x2, 0x4};
int numowners = 2;

int current_stage = 0;

#define NGAME_FRAMERATE 60
#define NRESOURCE_LIST_BASE 0x1C49480
#define NCURRENT_STAGE 0x000386930

#define PGAME_FRAMERATE 50
#define PRESOURCE_LIST_BASE 0x1C46100
#define PCURRENT_STAGE 0x1C6BB70

int subcount = 2;
bool pal = false;
bool neosubtitles = true;
int GAME_FRAMERATE = 60;
int RESOURCE_LIST_BASE = 0x1C49480;
int CURRENT_STAGE = 0x000386930;

wchar_t gbuf[80];
int lastsbload = -1;

const wchar_t *difficulties[] = {
    L"Orange Hat", // 0
    L"Lowest", // 1 o hor
    L"", // 2 o bad
    L"", // 3 b hor
    L"", // 4 b bad
    L"", // 5 p hor
    L"", // 6 p bad 
    L"", // 7 yhor 
    L"", // 8 ybad
    L"Blue Hat", // 9
    L"Pink Hat", // 10
    L"Yellow Hat", // 11
    L"", // 12 o
    L"", // 13 b 
    L"", // 14 p
    L"", // 15 y
    L"Highest", // 16
};

void loadsoundboard(int id) {
    if(lastsbload == id) return;
    if(id == -1) soundenv.load(soundboards[soundboards.size()-1]);
    else soundenv.load(soundboards[id]);
    lastsbload = id;
}

void playsound(int id) {
    loadsoundboard(records[current_record].soundboardid-1);
    soundenv.play(id);
}

void initconsole() {
    conscr::init();
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    conscr::refresh();
}

double gettime() {
    LARGE_INTEGER freq;
    LARGE_INTEGER current;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&current);

    LONGLONG numerator = current.QuadPart / freq.QuadPart;
    LONGLONG remainder = current.QuadPart % freq.QuadPart;

    double frac = double(remainder) / double(freq.QuadPart);
    return double(numerator) + frac;
}

bool buttonfromsubdot(e_suggestvariant_t &variant, int owner, u32 subdot, suggestbutton_t &button) {
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(line.containssubdot(subdot)) {
            for(suggestbutton_t &b : line.buttons) {
                if(b.timestamp + line.timestamp_start == subdot) { button = b; return true; }
            }
        }
    }
    return false;
}

bool buttonreffromsubdot(e_suggestvariant_t &variant, int owner, u32 subdot, suggestbutton_t **button) {
    for(e_suggestline_t &line : variant.lines) {
        if(!(line.owner & u32(owner))) continue;
        if(line.containssubdot(subdot)) {
            for(suggestbutton_t &b : line.buttons) {
                if(b.timestamp + line.timestamp_start == subdot) { *button = &b; return true; }
            }
        }
    }
    return false;
}

bool linereffromsubdot(e_suggestvariant_t &variant, int owner, u32 subdot, e_suggestline_t **line) {
    for(e_suggestline_t &l : variant.lines) {
        if(l.owner & u32(owner)) {
            if(l.containssubdot(subdot)) { *line = &l; return true; }
        }
    }
    return false;
}

struct playtoken_t {
    double when;
    int soundid;
};

double bpmtospsd(double bpm) { return (15.0 / bpm) / 24.0; }
bool tokensorter(playtoken_t a, playtoken_t b) { return (a.when < b.when); }

void playvariant(const e_suggestvariant_t &variant, soundenv_t &env, double bpm) {
    std::vector<playtoken_t> tokens;
    playtoken_t token;
    double spsd = bpmtospsd(bpm);
    for(const e_suggestline_t &line : variant.lines) {
        for(const suggestbutton_t &button : line.buttons) {
            for(const soundentry_t &se : button.sounds) {
                if(se.soundid == 0xFFFF) continue;
                token.when =
                    (spsd * double(button.timestamp + line.timestamp_start)) +
                    (double(se.relativetime) / double(GAME_FRAMERATE));
                token.soundid = se.soundid;
                tokens.push_back(token);
            }
        }
    }
    std::sort(tokens.begin(), tokens.end(), tokensorter);

    conscr::writes(0,0,L"Press any key to stop playback");
    conscr::refresh();

    double nexttick = 0.0;
    double secondsperbeat = 60.0/bpm;
    double timebase = gettime();
    for(int i = 0; i < tokens.size(); i += 1) {
        double rtime;
        do {
            rtime = gettime() - timebase;
            if(rtime > nexttick) {
                playticker();
                nexttick += secondsperbeat;
            }
            if(conscr::hasinput()) {
                INPUT_RECORD ir;
                conscr::read(ir);
                if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) return;
            }
        } while(rtime <= tokens[i].when);
        //snwprintf(gbuf, sizeof(gbuf), L"%02d", tokens[i].soundid);
        //conscr::writes(0,0,gbuf);
        //conscr::refresh();
        env.play(tokens[i].soundid);
    }
}

stageinfo_t getcurrentstageinfo() { return stages[current_stage]; } // Return from hardcoded Stage address/data list

void cleartopline(int nchars) {
    for(int i = 0; i < nchars; i += 1) conscr::putch(i,0,L' ');
}

void showerror(const wchar_t *msg) {
    cleartopline(80);
    conscr::writes(0,0,msg);
}

bool testdownload() {
    current_stage = 0;
    if(pcsx2reader::openpcsx2() == false) return false;
    showerror(L" Status: Reading current stage..."); conscr::refresh();
	
    pcsx2reader::read(CURRENT_STAGE, &current_stage, 1); current_stage -= 1;
    if(pal) current_stage += 1;
    showerror(L" Status: Getting stage info & database..."); conscr::refresh();
    stageinfo_t si = getcurrentstageinfo();
    u32 hdlistbase = findhdbase(RESOURCE_LIST_BASE);
    u32 bdlistbase = findbdbase(RESOURCE_LIST_BASE);
    int numhd = getnumhd(hdlistbase);

    showerror(L" Status: Reading game data..."); conscr::refresh();
    records.clear();
    pcsx2GetRecFromModelist((pal ? si.stagemodelistbaseP : si.stagemodelistbase), records, pal);
    pcsx2GetComBuffers((pal ? si.stagemodelistbaseP : si.stagemodelistbase), commands);
    pcsx2GetSoundboards(hdlistbase, bdlistbase, numhd, soundboards);
    pcsx2DwnlKeytables((pal ? si.keytablebaseP : si.keytablebase), numhd, 0, soundboards);

    return true;
}

#define FOREGROUND_GRAY (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
#define FOREGROUND_WHITE (FOREGROUND_GRAY | FOREGROUND_INTENSITY)
#define FG_AQUA (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_GREEN (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_PINK (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define FG_RED (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define FG_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FG_BLUE (FOREGROUND_BLUE | FOREGROUND_INTENSITY)

void drawhorizontal(int x1, int x2, int y, wchar_t c, WORD attr) {
    for(int i = x1; i <= x2; i+=1) conscr::putchcol(i, y, c, attr);
}

wchar_t buttonidtowchar[] = {
    '?',
    'T',
    'O',
    'X',
    'S',
    'L',
    'R',
};

WORD buttonidtocol[] = {
    FOREGROUND_GRAY,
    FG_GREEN,
    FG_RED,
    FG_AQUA,
    FG_PINK,
    FG_YELLOW,
    FG_BLUE
};

void drawbutton(int x, int y, int buttonid) { conscr::putchcol(x, y, buttonidtowchar[buttonid], buttonidtocol[buttonid]); }
void drawcursor(int x, int y) { conscr::putchcol(x, y, L'^', FOREGROUND_GRAY); }

bool dotisowned(int dot, int owner, e_suggestvariant_t &variant) {
    int subdot = dot * 24;
    for(e_suggestline_t &line : variant.lines) {
        if(line.owner & u32(owner)) if(line.containssubdot(subdot)) return true;
    }
    return false;
}

bool linestartsat(int dot, int owner, e_suggestvariant_t &variant) {
    int mindot = dot * 24;
    int maxdot = mindot + 23;
    for(e_suggestline_t &line : variant.lines) {
        if(line.owner & u32(owner)) {
            if((line.timestamp_start >= mindot) && (line.timestamp_start <= maxdot)) return true;
        }
    }
    return false;
}

int getcurrentvariantid() {
    if(currentrecord.variants[current_variant].islinked) return currentrecord.variants[current_variant].linknum;
    else return current_variant;
}

e_suggestvariant_t &getcurrentvariant() {
    if(currentrecord.variants[current_variant].islinked) return currentrecord.variants[currentrecord.variants[current_variant].linknum];
    else return currentrecord.variants[current_variant];
}

u32 getcurrentsubdot() { return (cursorpos * 24) + precisioncursorpos; }
int getcurrentowner() { return owners[cursorowner]; }

void advancecursor(int n, int maxindex) {
    cursorpos += n;
    if(cursorpos > maxindex) cursorpos = maxindex;
}

bool linesorter(e_suggestline_t &line1, e_suggestline_t &line2) {
    if(line1.timestamp_start < line2.timestamp_start) return true;
    return false;
}

bool buttonsorter(suggestbutton_t &button1, suggestbutton_t &button2) {
    if(button1.timestamp < button2.timestamp) return true;
    return false;
}

void doleftexpand(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getcurrentvariant();
    int chosen = -1;
    for(int i = 0; i < variant.lines.size(); i += 1) {
        e_suggestline_t &line = variant.lines[i];
        if(line.owner & u32(owner)) {
            if(line.timestamp_start < subdot) {
                if(chosen == -1) chosen = i;
                else if(line.timestamp_start > variant.lines[chosen].timestamp_start) chosen = i;
            }
        }
    }
    if(chosen != -1) {
        e_suggestline_t &line = variant.lines[chosen];
        int i = line.buttons.size() - 1;
        line.timestamp_end = subdot;
        if(i < 0) return;

        while(i >= 0) {
            if(!line.containssubdot(line.timestamp_start + line.buttons[i].timestamp)) {
                line.buttons.erase(line.buttons.begin() + i);
            }
            i-=1;
        }

    }
}

void domoveleftline(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getcurrentvariant();
    int chosen = -1;
    for(int i = 0; i < variant.lines.size(); i += 1) {
        e_suggestline_t &line = variant.lines[i];
        if(line.owner & u32(owner)) {
            if(line.timestamp_start < subdot) {
                if(chosen == -1) chosen = i;
				else if(line.timestamp_start > variant.lines[chosen].timestamp_start) chosen = i;
            }
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
    e_suggestvariant_t &variant = getcurrentvariant();
    e_suggestline_t *pline = nullptr;
    bool s = linereffromsubdot(variant, owner, subdot, &pline);
    if(!s) {
        pline = nullptr;
        for(int i = 0; i < variant.lines.size(); i += 1) {
            e_suggestline_t &line = variant.lines[i];
            if(line.owner & u32(owner)) {
                if(line.timestamp_start > subdot) {
                    if(pline == nullptr) pline = &line;
                    else if(line.timestamp_start < pline->timestamp_start) pline = &line;
                }
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

void dorightexpand(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getcurrentvariant();
    e_suggestline_t *pline = nullptr;
    bool s = linereffromsubdot(variant, owner, subdot, &pline);
    if(!s) {
        pline = nullptr;
        for(int i = 0; i < variant.lines.size(); i += 1) {
            e_suggestline_t &line = variant.lines[i];
            if(line.owner & u32(owner)) {
                if(line.timestamp_start > subdot) {
                    if(pline == nullptr) pline = &line;
					else if(line.timestamp_start < pline->timestamp_start) pline = &line;
                }
            }
        }
    }
    if(pline != nullptr) {
        e_suggestline_t &line = *pline;
        std::vector<suggestbutton_t> newbuttons;
        suggestbutton_t tmp;
        int delta = int(subdot) - int(line.timestamp_start);
        line.timestamp_start = subdot;
        for(int i = 0; i < line.buttons.size(); i += 1) {
            tmp = line.buttons[i];
            tmp.timestamp -= delta;
            if(line.containssubdot(tmp.timestamp + line.timestamp_start)) newbuttons.push_back(tmp);
        }
        line.buttons = newbuttons;
    }
}

void docreateline(u32 subdot_start, u32 subdot_end) {
    e_suggestvariant_t &variant = getcurrentvariant();
    e_suggestline_t *line;

    if(linereffromsubdot(variant, getcurrentowner(), subdot_start, &line)) return;
    e_suggestline_t newline;
    newline.always_zero = 0;
    newline.coolmodethreshold = 150;
    newline.localisations[0] = NULL;
    newline.localisations[1] = NULL;
    newline.localisations[2] = NULL;
    newline.localisations[3] = NULL;
    newline.owner = getcurrentowner();
    newline.timestamp_start = subdot_start;
    newline.timestamp_end = subdot_end;

    variant.lines.push_back(newline);
    std::sort(variant.lines.begin(), variant.lines.end(), linesorter);
}

void docreatebutton(u32 subdot, int owner, int buttonid) {
    e_suggestvariant_t &variant = getcurrentvariant();
    suggestbutton_t newbutton;

    /* First check if the button exists, if yes, just change button id and leave */
    suggestbutton_t *bref;
    bool s = buttonreffromsubdot(variant, owner, subdot, &bref);
    if(s) { bref->buttonid = buttonid; return; }

    newbutton.buttonid = buttonid;
    for(soundentry_t &e : newbutton.sounds) {
        e.always_zero = 0;
        e.soundid = ~0;
        e.animationid = ~0;
        e.relativetime = ~0;
    }

    for(e_suggestline_t &line : variant.lines) {
        if(line.owner & u32(owner)) {
            if(line.containssubdot(subdot)) {
                newbutton.timestamp = subdot - line.timestamp_start;
                line.buttons.push_back(newbutton);
                std::sort(line.buttons.begin(), line.buttons.end(), buttonsorter);
            }
        }
    }
}

void dodeletebutton(u32 subdot, int owner) {
    e_suggestvariant_t &variant = getcurrentvariant();

    for(e_suggestline_t &line : variant.lines) {
        if(line.owner & u32(owner)) {
            if(line.containssubdot(subdot)) {
                u32 rsubdot = subdot - line.timestamp_start;
                int i;
                for(i = 0; i < line.buttons.size(); i += 1) {
                    if(line.buttons[i].timestamp == rsubdot) {
                        line.buttons.erase(line.buttons.begin() + i);
                        return;
                    }
                }
            }
        }
    }
}

void doincreaseparameter(int amount) {
    suggestbutton_t *button;
    bool s = buttonreffromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(), &button);

    if(s) {
        soundentry_t &se = button->sounds[paramcursory];
        switch(paramcursorx) {
        case 0:
            se.soundid += amount;
            playsound(se.soundid);
            break;
        case 1:
            se.animationid += amount;
            break;
        case 2:
            se.relativetime += amount;
            break;
        default:
            se.soundid += amount;
            break;
        }
    }
}

void dodecreaseparameter(int amount) {
    suggestbutton_t *button;
    bool s = buttonreffromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(), &button);

    if(s) {
        soundentry_t &se = button->sounds[paramcursory];
        switch(paramcursorx) {
        case 0:
            se.soundid -= amount;
            playsound(se.soundid);
            break;
        case 1:
            se.animationid -= amount;
            break;
        case 2:
            se.relativetime -= amount;
            break;
        default:
            se.soundid -= amount;
            break;
        }
    }
}

void doadjustparameter() {
    suggestbutton_t *button;
    bool s = buttonreffromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(), &button);
    if(s) {
        soundentry_t &se = button->sounds[paramcursory];
        switch(paramcursorx) {
        case 0:
            se.soundid = conscr::query_hex(L" Input: Type new SND (hex): ");
            playsound(se.soundid);
            break;
        case 1:
            se.animationid = conscr::query_hex(L" Input: Type new ANIM (hex): ");
            break;
        case 2:
            se.relativetime = conscr::query_decimal(L" Input: Type new TIME (dec): ");
            break;
        default:
            se.soundid = conscr::query_hex(L" Input: Type new SND (hex): ");
            break;
        }
    }
}

void docopybutton(u32 subdot) {
    suggestbutton_t button;
    if(buttonfromsubdot(getcurrentvariant(), getcurrentowner(), subdot, button)) button_clipboard = button;
}

void dopastebutton(u32 subdot) {
    suggestbutton_t *pbutton;
    dodeletebutton(subdot, getcurrentowner());
    docreatebutton(subdot, getcurrentowner(), 1);

    bool s = buttonreffromsubdot(getcurrentvariant(), getcurrentowner(), subdot, &pbutton);
    if(s) {
        suggestbutton_t &tocopy = button_clipboard;
        pbutton->buttonid = tocopy.buttonid;
        for(int i = 0; i < 4; i += 1) pbutton->sounds[i] = tocopy.sounds[i];
    }
}

void dodeleteline(int subdot, int owner) {
    e_suggestvariant_t &variant = getcurrentvariant();
    for(int i = 0; i < variant.lines.size(); i += 1) {
        e_suggestline_t &line = variant.lines[i];
        if(line.owner & u32(owner)) {
            if(line.containssubdot(subdot)) {
                variant.lines.erase(variant.lines.begin() + i);
                return;
            }
        }
    }
}

void doadjustcoolthreshold(int amount) {
    e_suggestline_t *line;
    if(!linereffromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(),&line)) return;

    line->coolmodethreshold = conscr::query_decimal(L" Input: Type new threshold: ");
}

void waitkey() {
    conscr::flushinputs();
    for(;;) {
        INPUT_RECORD ir;
        conscr::read(ir);
        if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) return;
    }
}

void doadjustlink() {
    int linkid = conscr::query_decimal(L" Input: Type variant to link, or -1 to delete: ");
    if(linkid >= 17) {
        snwprintf(gbuf, 80, L" Error: Link identifier too high (%d)", linkid);
        showerror(gbuf);
        conscr::refresh();
        waitkey();
        return;
    } else if(linkid < 0) {
        e_suggestvariant_t &variant = currentrecord.variants[current_variant];
        variant.islinked = false;
        variant.linknum = -1;
        return;
    }
    if(linkid == current_variant) {
        e_suggestvariant_t &variant = currentrecord.variants[current_variant];
        variant.islinked = false;
        variant.linknum = -1;
        return;
    }
    if(currentrecord.variants[linkid].islinked) {
        showerror(L" Error: You can't link to a linked variant");
        conscr::refresh();
        waitkey();
        return;
    }
    e_suggestvariant_t &variant = currentrecord.variants[current_variant];
    variant.islinked = true;
    variant.linknum = linkid;
}

void doadjustinfo() {
    switch(infocursor) {
    case 0:
        doadjustlink();
        break;
    case 1:
        doadjustcoolthreshold(0);
        break;
    default:
        break;
    }
}

void onkeypress(int k, wchar_t uc, bool shiftmod) {
    if(k == 'D') {
        cursorpos += 1;
        if(shiftmod) cursorpos = (cursorpos + 3) & (~3);
        int maxindex = (currentrecord.lengthinsubdots / 24) - 1;
        if(cursorpos > maxindex) cursorpos = maxindex;
    }
    else if(k == 'A') {
        cursorpos -= 1;
        if(shiftmod) cursorpos &= (~3);
        if(cursorpos < 0) cursorpos = 0;
    }
    else if(k == 'S') {
        if(shiftmod) {
            if(owners[cursorowner] == 0) owners[cursorowner] = 1;
            else owners[cursorowner] = int(u32(owners[cursorowner]) << 1);
        } else {
            cursorowner += 1;
            if(cursorowner >= numowners) cursorowner = numowners - 1;
        }
    }
    else if(k == 'W') {
        if(shiftmod) {
            if(owners[cursorowner] == 0) owners[cursorowner] = int(u32(0x80000000));
            else owners[cursorowner] = int(u32(owners[cursorowner]) >> 1);
        } else {
            cursorowner -= 1;
            if(cursorowner < 0) cursorowner = 0;
        }
    }
    else if(k == 'Q') {
        precisioncursorpos -= 1;
        if(shiftmod) precisioncursorpos &= (~3);
        if(precisioncursorpos < 0) precisioncursorpos = 0;
    }
    else if(k == 'E') {
        precisioncursorpos += 1;
        if(shiftmod) precisioncursorpos = (precisioncursorpos + 3) & (~3);
        if(precisioncursorpos > 23) precisioncursorpos = 23;
    }
	// Param cursor
    else if(k == 'I') {
        paramcursory -= 1;
        if(paramcursory < 0) paramcursory = 0;
    }
    else if(k == 'K') {
        paramcursory += 1;
        if(paramcursory > 3) paramcursory = 3;
    }
    else if(k == 'J') {
        paramcursorx -= 1;
        if(paramcursorx < 0) paramcursorx = 0;
    }
    else if(k == 'L') {
        paramcursorx += 1;
        if(paramcursorx > 2) paramcursorx = 2;
    }
    else if(k == 'O') { doincreaseparameter(shiftmod ? 0x100 : 1); }
    else if(k == 'U') { dodecreaseparameter(shiftmod ? 0x100 : 1); }
    else if(k == 'M') { doleftexpand(cursorpos * 24 + 24, getcurrentowner()); }
    else if(uc == L'[' || uc == L'{') {
        if(shiftmod) domoveleftline(cursorpos * 24, getcurrentowner());
		else doleftexpand(cursorpos * 24 + 24, getcurrentowner());
    }
    else if(uc == L']' || uc == L'}') {
        if(shiftmod) domoverightline(cursorpos * 24, getcurrentowner());
        else dorightexpand(cursorpos * 24, getcurrentowner());
    }
    else if(k == VK_DOWN) {
        current_variant += 1;
        if(current_variant > 16) current_variant = 16;
    }
    else if(k == VK_UP) {
        current_variant -= 1;
        if(current_variant < 0) current_variant = 0;
    }
    else if(k == VK_LEFT) {
        current_record -= 1;
        if(current_record < 0) current_record = 0;
    }
    else if(k == VK_RIGHT) {
        current_record += 1;
        if(current_record >= records.size()) current_record = records.size() - 1;
    }
	// Line Creation/Deleting
    else if(k == VK_SPACE) { docreateline(cursorpos * 24, cursorpos * 24 + 24); }
    else if(k == VK_BACK) { dodeleteline(getcurrentsubdot(), getcurrentowner()); }
	// Button management
    #define dcb(x) docreatebutton(getcurrentsubdot(), getcurrentowner(), x)
    else if(k == '1') { dcb(1); }
    else if(k == '2') { dcb(2); }
    else if(k == '3') { dcb(3); }
    else if(k == '4') { dcb(4); }
    else if(k == '5') { dcb(5); }
    else if(k == '6') { dcb(6); }
    else if(k == '0') { dodeletebutton(getcurrentsubdot(), getcurrentowner()); }
    else if(k == '7') { dcb(0); }
    else if(k == 'C') { docopybutton(getcurrentsubdot()); }
    else if(k == 'V') { dopastebutton(getcurrentsubdot()); }
    else if(k == 'P') {
        e_suggestvariant_t &variant = getcurrentvariant();
        loadsoundboard(currentrecord.soundboardid-1);
        playvariant(
            variant,
            soundenv,
            stages[current_stage].bpm
        );
    }
    else if(k == VK_TAB) {
        infocursor += 1;
        if(infocursor > 1) infocursor = 0;
    }
    else if(uc == '`') { doadjustinfo(); }
    else if(k == VK_RETURN) { doadjustparameter(); }
	// Menu Options
    else if(k == VK_F5) {
        stageinfo_t si = getcurrentstageinfo();
        u32 totalsize = pcsx2calcsize(records, commands, oopslen, pal);
        u32 origsize = (pal ? si.buttondataendP : si.buttondataend) - (pal ? si.buttondatabaseP : si.buttondatabase) + 1;
        if(totalsize > origsize) {
            showerror(L" Error: Data too large!");
            conscr::refresh();
            waitkey();
            return;
        }
        showerror(L" Status: Uploading...");
        conscr::refresh();
        pcsx2upload(records, commands, oopsdat, oopslen, (pal ? si.buttondatabaseP : si.buttondatabase), (pal ? si.buttondataendP : si.buttondataend), (pal ? si.stagemodelistbaseP : si.stagemodelistbase), pal);
    }
	else if(k == VK_ESCAPE) { menu_options = true; }
}

bool dosaveproject(wchar_t *name) {
    HANDLE hfile = CreateFileW(
        LPCWSTR(name),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if(hfile == INVALID_HANDLE_VALUE) {
        //snwprintf(gbuf, 80, L"%d", GetLastError());
        //MessageBoxW(0,gbuf,0,0);
        return false;
    }

    DWORD written;
    #define WRITE(x) WriteFile(hfile, LPCVOID(&(x)), sizeof(x), &written, NULL)

    u32 tmpu32;
    i32 tmpi32;
    tmpu32 = u32(current_stage); WRITE(tmpu32);
    for(int i = 0; i < 9; i += 1) {
        std::vector<commandbuffer_t> &buffer = commands[i];
        tmpu32 = u32(buffer.size());
        WRITE(tmpu32);
        for(int k = 0; k < buffer.size(); k += 1) {
            WRITE(buffer[k].data);
        }
    }
    tmpu32 = records.size(); WRITE(tmpu32);
    for(int i = 0; i < records.size(); i += 1) {
        e_suggestrecord_t &record = records[i];
		
        WRITE(record.type);
        WRITE(record.lengthinsubdots);
        WRITE(record.soundboardid);
        for(int k = 0; k < 17; k += 1) {
            e_suggestvariant_t &variant = record.variants[k];
            tmpi32 = variant.islinked ? i32(variant.linknum) : -1; WRITE(tmpi32);

            tmpu32 = variant.lines.size(); WRITE(tmpu32);
            for(int m = 0; m < variant.lines.size(); m += 1) {
                e_suggestline_t &line = variant.lines[m];
                WRITE(line.always_zero);
                WRITE(line.coolmodethreshold);
				if(neosubtitles) {for(int susb = 0; susb < subcount; susb += 1) {
					WRITE(line.localisations[susb]);
				}}
                WRITE(line.owner);
                WRITE(line.timestamp_start);
                WRITE(line.timestamp_end);
                WRITE(line.unk1);
                WRITE(line.unk2);

                tmpu32 = line.buttons.size();
                WRITE(tmpu32);
                for(int n = 0; n < line.buttons.size(); n += 1) {
                    suggestbutton_t &button = line.buttons[n];
                    WRITE(button);
                }
            }
        }
    }

    tmpu32 = soundboards.size(); WRITE(tmpu32);
    for(int i = 0; i < soundboards.size(); i += 1) {
        e_soundboard_t &sb = soundboards[i];
        WRITE(sb.bd.len);
        WriteFile(
            hfile,
            LPCVOID(sb.bd.bytes),
            sb.bd.len,
            &written,
            NULL
        );

        tmpu32 = sb.keys.size(); WRITE(tmpu32);
        for(int k = 0; k < sb.keys.size(); k += 1) {
            key_t &key = sb.keys[k];
            WRITE(key);
        }
        WRITE(sb.prog);
        tmpu32 = sb.sounds.size(); WRITE(tmpu32);
        for(int k = 0; k < sb.sounds.size(); k += 1) {
            WRITE(sb.sounds[k]);
        }
    }
    #undef WRITE
    CloseHandle(hfile);
    return true;
}

bool doloadproject(wchar_t *name) {
    HANDLE hfile = CreateFileW(
        LPCWSTR(name),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if(hfile == INVALID_HANDLE_VALUE) {
        snwprintf(gbuf, 80, L"%d", GetLastError());
        MessageBoxW(0,gbuf,0,0);
        return false;
    }

    DWORD readin;
    records.clear();
    soundboards.clear();

    #define READ(x) ReadFile(hfile, LPVOID(&(x)), sizeof(x), &readin, NULL)
    u32 tmpu32;
    i32 tmpi32;
	
    READ(tmpu32); current_stage = int(tmpu32);
    currentstageinfo = getcurrentstageinfo();

    for(int i = 0; i < 9; i += 1) {
        std::vector<commandbuffer_t> &buffer = commands[i];
        READ(tmpu32); buffer.resize(tmpu32);
        for(int k = 0; k < buffer.size(); k += 1) READ(buffer[k].data);
    }

    READ(tmpu32); records.resize(tmpu32);
    for(int i = 0; i < records.size(); i += 1) {
        e_suggestrecord_t &record = records[i];
        u32 ib;
        READ(record.type);
        READ(record.lengthinsubdots);
        READ(record.soundboardid);
        for(int k = 0; k < 17; k += 1) {
            e_suggestvariant_t &variant = record.variants[k];
            READ(tmpi32); variant.islinked = bool(tmpi32 >= 0);
            variant.linknum = tmpi32;

            READ(tmpu32); variant.lines.resize(tmpu32);
            for(int m = 0; m < variant.lines.size(); m += 1) {
                e_suggestline_t &line = variant.lines[m];
                READ(line.always_zero);
                READ(line.coolmodethreshold);
				if(neosubtitles) {for(int susb = 0; susb < subcount; susb += 1) {
					READ(line.localisations[susb]);
				}}
                READ(line.owner);
                READ(line.timestamp_start);
                READ(line.timestamp_end);
                READ(line.unk1);
                READ(line.unk2);
                READ(tmpu32);
                line.buttons.resize(tmpu32);
                for(int n = 0; n < line.buttons.size(); n += 1) {
                    suggestbutton_t &button = line.buttons[n];
                    READ(button);
                }
            }

        }
    }

    READ(tmpu32); soundboards.resize(tmpu32);
    for(int i = 0; i < soundboards.size(); i += 1) {
        e_soundboard_t &sb = soundboards[i];
        READ(sb.bd.len); sb.bd.bytes = (byte*)(malloc(sb.bd.len));
        ReadFile(hfile, LPVOID(sb.bd.bytes), sb.bd.len, &readin, NULL);
		
        READ(tmpu32); sb.keys.resize(tmpu32);
        for(int k = 0; k < sb.keys.size(); k += 1) {
            key_t &key = sb.keys[k]; READ(key);
        }
        READ(sb.prog);
        READ(tmpu32); sb.sounds.resize(tmpu32);
        for(int k = 0; k < sb.sounds.size(); k += 1) {
            e_sound_t &snd = sb.sounds[k]; READ(snd);
        }
    }
    #undef READ
    CloseHandle(hfile);
    return true;
}

// menu

const wchar_t *optionlines[] = {
	L"NeoBesms 30/31/2023",
    (pal ? L"Current region: PAL" : L"Current region: NTSC"),
	L"",
    L"[F01] Save Project",
    L"[F03] Load Project",
	L"[F06] Load Project (ptr2besms)",
    L"",
    L"[F05] Inject OLM",
    L"[F09] Download From PCSX2",
	L"[F10] Toggle game region",
    L"",
    L""//L"[ESC] Go to editor"
};

void drawoptions() {
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_GRAY);
    optionlines[1] = (pal ? L"Current region: PAL" : L"Current region: NTSC");
    optionlines[11] = ((records.size() == 0) ? L"" : L"[ESC] Return to editor");
	int optlines = (sizeof(optionlines) / sizeof(optionlines[0]));
    for(int i = 0; i < optlines; i += 1) conscr::writes(1,1+i, optionlines[i]);
    conscr::refresh();
}

void onoptionskey(int k, wchar_t uc, bool shiftmod) {
    wchar_t filename[MAX_PATH];
    if(k == VK_F9) {
        current_record = 0;
        current_variant = 0;
        if(testdownload()) menu_options = false;
        else { showerror(L" Error: PCSX2 process not found!"); conscr::refresh(); waitkey(); }
    }
    else if(k == VK_F1) {
        int l = conscr::query_string(L" Input: Save as: ", filename, MAX_PATH);
        if(l == 0) return;
        if(dosaveproject(filename)) { snwprintf(gbuf, 80, L"Saved as %ls", filename); showerror(gbuf); }
		else { snwprintf(gbuf, 80, L" Error: Couldn't open %ls", filename); showerror(gbuf); }
        conscr::refresh(); waitkey();
    }
    else if(k == VK_F3 || k == VK_F6) {
        int l = conscr::query_string(L"Load Path: ", filename, MAX_PATH);
        if(l == 0) return;
        if(k == VK_F6) neosubtitles = false; else neosubtitles = true;
        if(doloadproject(filename)) { menu_options = false; }
		else { snwprintf(gbuf, 80, L" Error: Couldn't open %ls", filename); showerror(gbuf); conscr::refresh(); waitkey(); }
    }
    else if(k == VK_F5) {
        int l = conscr::query_string(L" Input: OLM File Path: ", filename, MAX_PATH);
        if(l == 0) return;
        stageinfo_t si = getcurrentstageinfo();
		
        bool s = olmupload(filename, records, commands, oopsdat, oopslen, si.buttondatabase, si.buttondataend, si.stagemodelistbase, pal);
        if(s) {snwprintf(gbuf, 80, L" Info: Injected %ls", filename); showerror(gbuf);}
        else snwprintf(gbuf, 80, L" Error: Failed to inject %ls", filename); showerror(gbuf);
        conscr::refresh(); waitkey();
    }
	else if(k == VK_F10) {
		wchar_t *tmpr;
        if(pal) {
			pal = false;
			GAME_FRAMERATE = NGAME_FRAMERATE;
			RESOURCE_LIST_BASE = NRESOURCE_LIST_BASE;
			CURRENT_STAGE = NCURRENT_STAGE;
            subcount = 4;
			tmpr = L"NTSC";
		} else {
			pal = true;
			GAME_FRAMERATE = PGAME_FRAMERATE;
			RESOURCE_LIST_BASE = PRESOURCE_LIST_BASE;
			CURRENT_STAGE = PCURRENT_STAGE;
            subcount = 7;
			tmpr = L"PAL";
		}
        if(!menu_options) {snwprintf(gbuf, 80, L" Info: Switched to %ls", tmpr); showerror(gbuf); conscr::refresh(); waitkey();}
    }
    else if(k == VK_ESCAPE) {
	    if(records.size() == 0) {showerror(L" Error: No data loaded."); conscr::refresh(); waitkey();}
		else { menu_options = false; }
    }
}

void drawvariant_base(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    int ndots = length / 24;
    for(int i = 0; i < ndots; i += 1) {
        wchar_t chartoput = ((i % 4) == 0) ? L'*' : L'-';
        WORD attrtoput = dotisowned(i, owner, variant) ? FOREGROUND_GRAY : FOREGROUND_INTENSITY;
        conscr::putchcol(x + i, y, chartoput, attrtoput);
    }
}

void drawlinemarkers(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    int ndots = length / 24;
    for(int i = 0; i < ndots; i += 1) {
        if(linestartsat(i, owner, variant)) conscr::putchcol(x + i, y, L'$', FOREGROUND_GRAY);
    }
}

void drawprecision_base(int x, int y, int owner, e_suggestvariant_t &variant) {
    bool owned = dotisowned(cursorpos, owner, variant);
    WORD attrtoput = owned ? FOREGROUND_GRAY : FOREGROUND_INTENSITY;

    for(int i = 0; i < 24; i += 1) {
        wchar_t chartoput = (((i % 4) == 0) ? L'*' : L'-');
        conscr::putchcol(x + i, y, chartoput, attrtoput);
    }
}

void drawprecision_buttons(int x, int y, int owner, e_suggestvariant_t &variant) {
    u32 subdotbase = cursorpos * 24;
    suggestbutton_t button;
    for(int i = 0; i < 24; i += 1) {
        bool s = buttonfromsubdot(variant, owner, subdotbase + i, button);
        if(s) drawbutton(x + i, y, button.buttonid);
    }
}

void drawvariant(int x, int y, int owner, e_suggestvariant_t &variant, int length) {
    suggestbutton_t button;
    for(int i = 0; i < length; i += 1) {
        bool s = buttonfromsubdot(variant, owner, i, button);
        if(s) drawbutton((i / 24) + x, y, button.buttonid);
    }
}

const wchar_t *ownernames[] = {
    L"None",
    L"???",
    L"Teacher",
    L"PaRappa",
    L"Boxxy"
};

void drawownername(int x, int y, int ownerid) {
    int cid = 0;
    u32 tmp = u32(ownerid);
    if(ownerid != 0) {
        cid += 1;
        while(tmp != 1) { tmp >>= 1; cid += 1; }
    }
    if(cid >= 5) { snwprintf(gbuf, 80, L"%x", ownerid); conscr::writes(x,y,gbuf); }
    else conscr::writes(x,y,ownernames[cid]);
}

void drawrecord(e_suggestrecord_t &record, int variantid) {
    e_suggestvariant_t &variant = record.variants[variantid];
    drawownername(1,2,owners[0]);
    drawlinemarkers(10,1,owners[0],variant, record.lengthinsubdots);
    drawvariant_base(10, 2, owners[0], variant, record.lengthinsubdots);
    drawvariant(10,2,owners[0], variant, record.lengthinsubdots);

    drawownername(1,5,owners[1]);
    drawlinemarkers(10,4,owners[1],variant,record.lengthinsubdots);
    drawvariant_base(10, 5, owners[1], variant, record.lengthinsubdots);
    drawvariant(10,5,owners[1], variant, record.lengthinsubdots);

    drawprecision_base(14, 8, getcurrentowner(), variant);
    drawprecision_buttons(14,8, getcurrentowner(), variant);

    drawcursor(10 + cursorpos, 3 + (cursorowner * 3));
    drawcursor(14 + precisioncursorpos, 9);
}

void drawbuttonparameters(int x, int y, suggestbutton_t &button) {
    conscr::putchcol(x + (5 * (paramcursorx + 1)), y, L'v', FOREGROUND_GRAY);
    conscr::writescol(x + 5, y + 1, L"SND  ANIM TIME", FOREGROUND_GRAY);
    for(int i = 0; i < 4; i += 1) {
        soundentry_t &se = button.sounds[i];
        snwprintf(gbuf, 80, L"[%d] %04x %04x %04d", i, u32(se.soundid), se.animationid, se.relativetime);
        if(paramcursory == i) conscr::putchcol(x, y + (i * 2) + 2, L'>', FOREGROUND_GRAY);
        conscr::writescol(x + 1, y + (i * 2) + 2, gbuf, (se.soundid != 0xFFFF) ? FOREGROUND_GRAY : FOREGROUND_INTENSITY);
    }
}

void drawlineparameters(int x, int y) {
    e_suggestline_t *line;
    bool s = linereffromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(), &line);
    if(!s) return;

    snwprintf(gbuf, 80, L"COOL: %d", line->coolmodethreshold);
    conscr::writescol(x+1,y,gbuf,FOREGROUND_GRAY);
    if(infocursor == 1) conscr::putchcol(x, y, L'>', FOREGROUND_GRAY);
}

void drawinfo(int x, int y) {
    snwprintf(gbuf, 80, L"Stage: %d  %ls", current_stage, stages[current_stage].name);
    conscr::writescol(x, y-1, gbuf, FOREGROUND_GRAY);

    snwprintf(gbuf, 80, L"Record: %d", current_record);
    conscr::writescol(x, y, gbuf, FOREGROUND_GRAY);

    snwprintf(gbuf, 80, L"Variant: %d  %ls", current_variant, difficulties[current_variant]);
    conscr::writescol(x,y+1,gbuf,FOREGROUND_GRAY);

    if(infocursor == 0) conscr::putchcol(x-1,y+2,L'>',FOREGROUND_GRAY);
    if(currentrecord.variants[current_variant].islinked) {
        snwprintf(gbuf, 80, L"Linked: Variant %d", currentrecord.variants[current_variant].linknum);
        conscr::writescol(x+1,y+2,gbuf,FOREGROUND_WHITE);
    } else conscr::writescol(x+1,y+2,L"Not linked",FOREGROUND_INTENSITY);

    u32 totalsize = pcsx2calcsize(records, commands, oopslen, pal);
    u32 origsize = (pal ? stages[current_stage].buttondataendP : stages[current_stage].buttondataend) - (pal ? stages[current_stage].buttondatabaseP : stages[current_stage].buttondatabase) + 1;

    WORD attr = (totalsize > origsize) ? (FG_RED) : (FG_GREEN);

    snwprintf(gbuf, 80, L"%d / %d bytes", totalsize, origsize);
    conscr::writescol(x, y+4, gbuf, attr);
}

void draw() {
    conscr::clearchars(L' ');
    conscr::clearcol(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);

    drawrecord(currentrecord, getcurrentvariantid());

    suggestbutton_t button;
    bool s = buttonfromsubdot(getcurrentvariant(), getcurrentowner(), getcurrentsubdot(), button);
    if(s) drawbuttonparameters(1, 12, button);

    drawlineparameters(60, 20);
    drawinfo(50, 13);

    conscr::refresh();
}

void sandbox() {
    menu_options = true;
    current_record = 0;

    for(;;) {
        INPUT_RECORD ir;
        SwitchToThread();
		while(conscr::hasinput()) {
			conscr::read(ir);
			if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
				if(menu_options) {onoptionskey(ir.Event.KeyEvent.wVirtualKeyCode,
					ir.Event.KeyEvent.uChar.UnicodeChar,
					ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED);}
                else {onkeypress(ir.Event.KeyEvent.wVirtualKeyCode,
					ir.Event.KeyEvent.uChar.UnicodeChar,
					ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED);}
			}
			if(menu_options) {drawoptions();} else {draw();}
		}
    }
}

bool loadoops() {
    HANDLE hfile = CreateFileW(
        L"oops.dat",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );

    if(hfile == INVALID_HANDLE_VALUE) {
        oopslen = 0;
        oopsdat = nullptr;
        return false;
    }

    LARGE_INTEGER filesize;
    GetFileSizeEx(hfile, &filesize);

    oopslen = int(filesize.LowPart);
    oopsdat = new byte[oopslen];

    DWORD readn;
    ReadFile(hfile, LPVOID(oopsdat), DWORD(oopslen), &readn, NULL);

    CloseHandle(hfile);
    return true;
}

int main() {
    initconsole();
    initsound(GetConsoleWindow());
    loadoops();
    loadticker();
    sandbox();
}
