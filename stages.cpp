#include "stageinfo.h"

#define STAGE1_RECORD_LIST_BASE 0x01D28114
#define STAGE1_KEYTABLE_LIST_BASE 0x01CD3968

stageinfo_t stages[8] = {
    {
        L"Toasty Buns",
        108.0,
        0x01D2A848,
        0x01CD3968, 
        0x01CD3A88,
        0x01D29B18 + 0xD30 - 1,
		
		0x01D31988,//0x01D35210,
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Romantic Love",
        93.4,
        0x01D304D0,
        0x01CE05D0,
        0x01CE06E0,
        0x01D2FA00 + 0xAD0 -1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"BIG",
        78.0,
        0x01D47A68,
        0x01CEB2A8,
        0x01CEB3B8,
        0x01D46F98 + 0xAD0 - 1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Sista Moosesha",
        102.0,
        0x01D3E978,
        0x01CDB220,
        0x01CDB380,
        0x01D3DB48 + 0xE30 - 1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Hair Scare",
        105.0,
        0x01D72490,
        0x01CFA3A0,
        0x01CFA4C0,
        0x01D71760 + 0xD30 - 1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Food Court",
        102.0,
        0x01D1F668,
        0x01CC3C78,
        0x01CC3DC8,
        0x01D1E7F8 + 0xE70 - 1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Noodles Can't Be Beat",
        95.0,
        0x01D14098,
        0x01CCEB40,
        0x01CCEC40,
        0x01D134D8 + 0xBC0 - 1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    },
    {
        L"Always LOVE",
        99.0,
        0x01D82460,
        0x01D1AB20,
        0x01D1ACD0,
        0x01D814D0 + 0xF90 -1,

        0x01D31988, // TODO: Placeholder
		0x01CD4908,
		0x01CD4A28,
		0x01D2FF28 + 0xD30 - 1
    }
};

//0x01CD3A88


