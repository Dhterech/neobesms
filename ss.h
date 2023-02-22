#ifndef BES_SS_H
#define BES_SS_H

#include "types.h"
#include <Windows.h>
#include <vector>
#include <array>
#include <dsound.h>

extern LPDIRECTSOUND dsnd;
extern LPDIRECTSOUNDBUFFER ticksnd;

struct e_sound_t {
    u32 bdoffset;
    u16 frequency;
    u16 volume;
};

struct ptrkey_t {
    u16 program;
    u16 key;
    u16 vol;
};
/* TODO: Hack to work around pthread key_t name conflict apparently */
#define key_t ptrkey_t

struct e_soundboard_t {
    std::vector<e_sound_t> sounds;
    std::vector<key_t> keys;
    std::array<std::array<u8, 128>, 128> prog;
    struct {
        u32 len = 0;
        byte *bytes = nullptr;
    } bd;
    ~e_soundboard_t();
    void clear();
};

struct sound_t {
    IDirectSoundBuffer8 *sndbuf = NULL;
    ~sound_t();
    void clear();
    void load(const e_soundboard_t &sb, const e_sound_t &snd);
};

struct soundenv_t {
    u32 soundboardid;
    std::vector<sound_t> sounds;
    std::vector<key_t> keys;
    std::array<std::array<u8, 128>, 128> prog;
    std::array<u16, 128> lastkey;
    void clear();
    void load(const e_soundboard_t &sb);
    void play(int key);
};

void loadticker();
void playticker();
void initsound(HWND);

#endif // BES_SS_H
