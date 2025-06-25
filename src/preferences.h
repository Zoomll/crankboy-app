//
//  preferences.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 18/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef preferences_h
#define preferences_h

#include "utility.h"

#include <stdio.h>

#define DYNAMIC_RATE_OFF 0
#define DYNAMIC_RATE_ON 1
#define DYNAMIC_RATE_AUTO 2

#define CRANK_MODE_START_SELECT 0
#define CRANK_MODE_TURBO_CW 1
#define CRANK_MODE_TURBO_CCW 2
#define CRANK_MODE_OFF 3

// at least 1 bit for each setting
typedef uint32_t preferences_bitfield_t;

typedef enum preference_index_t {
    #define PREF(x, ...) PREFI_##x,
    #include "prefs.x"
    PREFI_COUNT,
} preference_index_t;

typedef enum preference_index_bit_t {
    #define PREF(x, ...) PREFBIT_##x = (1 << (int)PREFI_##x),
    #include "prefs.x"
} preference_index_bit_t;

#define PREF(x, ...) extern int preferences_##x;
#include "prefs.x"

void preferences_init(void);

void preferences_read_from_disk(const char* filename);
int preferences_save_to_disk(const char* filename);

#endif /* preferences_h */
