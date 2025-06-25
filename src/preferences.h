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

extern int preferences_display_fps;
extern int preferences_frame_skip;
extern int preferences_itcm;
extern int preferences_lua_support;
extern int preferences_sound_mode;
extern int preferences_crank_mode;
extern int preferences_dynamic_rate;
extern int preferences_sample_rate;
extern int preferences_uncap_fps;
extern int preferences_dither_pattern;
extern int preferences_save_state_slot;
extern int preferences_overclock;
extern int preferences_dynamic_level;
extern int preferences_transparency;
extern int preferences_joypad_interrupts;
extern int preferences_per_game;

void preferences_init(void);

void preferences_read_from_disk(const char* filename);
int preferences_save_to_disk(const char* filename);

#endif /* preferences_h */
