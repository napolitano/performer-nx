#include "Config.h"
#include "Scale.h"
#include "UserScale.h"

#define ARRAY_SIZE(_array_) (sizeof(_array_) / sizeof(_array_[0]))
#define NOTE_SCALE(_name_, _title_, _chromatic_, ...) \
static const uint16_t _name_##_notes[] = { __VA_ARGS__ }; \
static const NoteScale _name_(_title_, _chromatic_, ARRAY_SIZE(_name_##_notes), _name_##_notes);

NOTE_SCALE(semitoneScale, TXT_LIST_LABEL_SCALE_SEMITONES, true, 0, 128, 256, 384, 512, 640, 768, 896, 1024, 1152, 1280, 1408)

NOTE_SCALE(majorScale, TXT_LIST_LABEL_SCALE_MAJOR, true, 0, 256, 512, 640, 896, 1152, 1408)
NOTE_SCALE(minorScale, TXT_LIST_LABEL_SCALE_MINOR, true, 0, 256, 384, 640, 896, 1024, 1280)

NOTE_SCALE(majorBluesScale, TXT_LIST_LABEL_SCALE_MAJOR_BLUES, true, 0, 384, 512, 896, 1152, 1280)
NOTE_SCALE(minorBluesScale, TXT_LIST_LABEL_SCALE_MINOR_BLUES, true, 0, 384, 640, 768, 896, 1280)

NOTE_SCALE(majorPentatonicScale, TXT_LIST_LABEL_SCALE_MAJOR_PENT, true, 0, 256, 512, 896, 1152)
NOTE_SCALE(minorPentatonicScale, TXT_LIST_LABEL_SCALE_MINOR_PENT, true, 0, 384, 640, 896, 1280)

NOTE_SCALE(folkScale, TXT_LIST_LABEL_SCALE_FOLK, true, 0, 128, 384, 512, 640, 896, 1024, 1280)
NOTE_SCALE(japaneseScale, TXT_LIST_LABEL_SCALE_JAPANESE, true, 0, 128, 640, 896, 1024)
NOTE_SCALE(gamelanScale, TXT_LIST_LABEL_SCALE_GAMELAN, true, 0, 128, 384, 896, 1024)
NOTE_SCALE(gypsyScale, TXT_LIST_LABEL_SCALE_GYPSY, true, 0, 256, 384, 768, 896, 1024, 1408)
NOTE_SCALE(arabianScale, TXT_LIST_LABEL_SCALE_ARABIAN, true, 0, 128, 512, 640, 896, 1024, 1408)
NOTE_SCALE(flamencoScale, TXT_LIST_LABEL_SCALE_FLAMENCO, true, 0, 128, 512, 640, 896, 1024, 1280)
NOTE_SCALE(wholeToneScale, TXT_LIST_LABEL_SCALE_WHOLE_TONE, true, 0, 256, 512, 768, 1024, 1280)

// python: [int(round(x * (12 * 128) / float(N))) for x in range(N)]
NOTE_SCALE(tet5Scale, TXT_LIST_LABEL_SCALE_FIVE_TET, false, 0, 307, 614, 922, 1229);
NOTE_SCALE(tet7Scale, TXT_LIST_LABEL_SCALE_SEVEN_TET, false, 0, 219, 439, 658, 878, 1097, 1317);
NOTE_SCALE(tet19Scale, TXT_LIST_LABEL_SCALE_NINETEEN_TET, false, 0, 81, 162, 243, 323, 404, 485, 566, 647, 728, 808, 889, 970, 1051, 1132, 1213, 1293, 1374, 1455);
NOTE_SCALE(tet22Scale, TXT_LIST_LABEL_SCALE_TWENTYTWO_TET, false, 0, 70, 140, 209, 279, 349, 419, 489, 559, 628, 698, 768, 838, 908, 977, 1047, 1117, 1187, 1257, 1327, 1396, 1466);
NOTE_SCALE(tet24Scale, TXT_LIST_LABEL_SCALE_TWENTYFOUR_TET, false, 0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472);

#undef ARRAY_SIZE
#undef NOTE_SCALE

static const VoltScale voltageScale(TXT_LIST_LABEL_SCALE_VOLTAGE, 0.1f);

static const Scale *scales[] = {
    &semitoneScale,

    &majorScale,
    &minorScale,

    &majorBluesScale,
    &minorBluesScale,

    &majorPentatonicScale,
    &minorPentatonicScale,

    &folkScale,
    &japaneseScale,
    &gamelanScale,
    &gypsyScale,
    &arabianScale,
    &flamencoScale,
    &wholeToneScale,

    &tet5Scale,
    &tet7Scale,
    &tet19Scale,
    &tet22Scale,
    &tet24Scale,

    &voltageScale
};

static const int BuiltinCount = sizeof(scales) / sizeof(Scale *);
static const int UserCount = UserScale::userScales.size();

int Scale::Count = BuiltinCount + UserCount;

const Scale &Scale::get(int index) {
    if (index < BuiltinCount) {
        return *scales[index];
    } else {
        return UserScale::userScales[index - BuiltinCount];
    }
}

const char *Scale::name(int index) {
    if (index < BuiltinCount) {
        return get(index).displayName();
    } else {
        switch (index - BuiltinCount) {
        case 0: return TXT_LIST_LABEL_SCALE_USER1;
        case 1: return TXT_LIST_LABEL_SCALE_USER2;
        case 2: return TXT_LIST_LABEL_SCALE_USER3;
        case 3: return TXT_LIST_LABEL_SCALE_USER4;
        }
    }
    return nullptr;
}
