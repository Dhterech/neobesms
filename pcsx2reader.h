#ifndef PCSX2READER_H
#define PCSX2READER_H

#include "types.h"

namespace pcsx2reader {

bool read(u32 addr, void *out, u32 size);
bool write(u32 addr, void *out, u32 size);
bool pcsx2opened();
bool openpcsx2();

};

#endif // PCSX2READER_H
