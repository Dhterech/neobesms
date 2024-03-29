#ifndef BES_SUGGEST_H
#define BES_SUGGEST_H

#include "types.h"

struct soundentry_t {
    u32 relativetime;
    u16 animationid;
    u16 unk;
    u16 soundid;
    u16 always_zero;
};

struct suggestbutton_t {
    u32 timestamp;
    u32 buttonid;
    soundentry_t sounds[4];
};

struct suggestline_t {
    u32 owner;
    u32 buttoncount;
    u32 ptr_buttons;

    u32 oopscount;
    u32 ptr_oops;

    u32 timestamp_start;
    u32 timestamp_end;

    u32 coolmodethreshold;

    u32 localisations[4];
    u32 vs_count;
};

struct suggestline_t_pal {
    u32 owner;
    u32 buttoncount;
    u32 ptr_buttons;

    u32 oopscount;
    u32 ptr_oops;

    u32 timestamp_start;
    u32 timestamp_end;

    u32 coolmodethreshold;

    u32 localisations[7];
    u32 vs_count;
};

struct e_suggestline_t {
    u32 owner;
    std::vector<suggestbutton_t> buttons;

    u32 oopscount;
    u32 ptr_oops;

    u32 timestamp_start;
    u32 timestamp_end;

    u32 coolmodethreshold;

    u32 localisations[7];
    u32 vs_count;

    bool containssubdot(u32 subdot);
};

struct e_suggestvariant_t {
    bool islinked = false;
    int linknum;
    std::vector<e_suggestline_t> lines;
};

struct suggestvariant_t {
    u32 numlines;
    u32 ptr_lines;
};

struct e_suggestrecord_t {
    u32 type;
    u32 address;
    u32 soundboardid;
    u32 lengthinsubdots;
    e_suggestvariant_t variants[17];
    byte vs_data[0x48];
};

struct suggestrecord_t {
    u32 soundboardid;
    u32 lengthinsubdots;
    suggestvariant_t variants[17];
    byte vs_data[0x48];
};

#define SCENECMD_SETCURSOR 0
#define SCENECMD_SETRECORD 1
#define SCENECMD_SETGRADED 2
#define SCENECMD_ACTIVATE 9

struct commandbuffer_t {
    u16 cmd_id;
    u16 arg1;
    u32 arg2;
    u32 arg3;
    u32 arg4;
};

struct scenemode_t {
    u8 music;
    u8 left_channel;
    u8 right_channel;
    u8 always_zero; /* Maybe? */
    u32 count_scenecommands;
    u32 ptr_scenecommands;
    u32 reserved_timer1; /* Used by game engine */
    u32 reserved_timer2; /* Used by game engine */
    u32 subtitle_scene_id;
    u32 scene_id;
    u32 unk[2]; /* Used by versus mode */
    u32 offset_in_ms;
    float bpm;
};

#endif // BES_SUGGEST_H

