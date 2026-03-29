#pragma once

#include "Config.h"
#include <cstdint>

// straight

// 3,      // 1/64
// 6,      // 1/32
// 12,     // 1/16
// 24,     // 1/8
// 48,     // 1/4
// 96,     // 1/2
// 192,    // 1
// 384,    // 2
// 768,    // 4

// triplet

// 2,      // 1/64T
// 4,      // 1/32T
// 8,      // 1/16T
// 16,     // 1/8T
// 32,     // 1/4T
// 64,     // 1/2T
// 128,    // 1T
// 256,    // 2T
// 512,    // 4T

// dotted

// 9,      // 1/32.
// 18,     // 1/16.
// 36,     // 1/8.
// 72,     // 1/4.
// 144,    // 1/2.
// 288,    // 1.
// 576,    // 2.

struct KnownDivisor {
    uint16_t divisor;       // divisor value with respect to CONFIG_SEQUENCE_PPQN
    uint8_t numerator;      // note value numerator
    uint8_t denominator;    // note value denominator
    char type;              // note type (straight, triplet, dotted)
    int8_t index;           // index [0..15] for selecting via step keys
};

// divisors based on 48 PPQN

static KnownDivisor knownDivisors[] = {
    {   2,      1,  64, TXT_MODEL_TRIPLET_SHORTCODE,    8   },  // 1/64T
    {   3,      1,  64, TXT_MODEL_FULL_SHORTCODE,   0   },  // 1/64
    {   4,      1,  32, TXT_MODEL_TRIPLET_SHORTCODE,    9   },  // 1/32T
    {   6,      1,  32, TXT_MODEL_FULL_SHORTCODE,   1   },  // 1/32
    {   8,      1,  16, TXT_MODEL_TRIPLET_SHORTCODE,    10  },  // 1/16T
    {   9,      1,  32, TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/32.
    {   12,     1,  16, TXT_MODEL_FULL_SHORTCODE,   2   },  // 1/16
    {   16,     1,  8,  TXT_MODEL_TRIPLET_SHORTCODE,    11  },  // 1/8T
    {   18,     1,  16, TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/16.
    {   24,     1,  8,  TXT_MODEL_FULL_SHORTCODE,   3   },  // 1/8
    {   32,     1,  4,  TXT_MODEL_TRIPLET_SHORTCODE,    12  },  // 1/4T
    {   36,     1,  8,  TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/8.
    {   48,     1,  4,  TXT_MODEL_FULL_SHORTCODE,   4   },  // 1/4
    {   64,     1,  2,  TXT_MODEL_TRIPLET_SHORTCODE,    13  },  // 1/2T
    {   72,     1,  4,  TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/4.
    {   96,     1,  2,  TXT_MODEL_FULL_SHORTCODE,   5   },  // 1/2
    {   128,    1,  1,  TXT_MODEL_TRIPLET_SHORTCODE,    14  },  // 1T
    {   144,    1,  2,  TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/2.
    {   192,    1,  1,  TXT_MODEL_FULL_SHORTCODE,   6   },  // 1
    {   256,    2,  1,  TXT_MODEL_TRIPLET_SHORTCODE,    15  },  // 2/1T
    {   288,    1,  1,  TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 1/1.
    {   384,    2,  1,  TXT_MODEL_FULL_SHORTCODE,   7   },  // 2/1
    {   512,    4,  1,  TXT_MODEL_TRIPLET_SHORTCODE,    -1  },  // 4/1T
    {   576,    2,  1,  TXT_MODEL_DOTTED_SHORTCODE,    -1  },  // 2/1.
    {   768,    4,  1,  TXT_MODEL_FULL_SHORTCODE,   -1  },  // 4/1
};

static const int numKnownDivisors = sizeof(knownDivisors) / sizeof(KnownDivisor);
