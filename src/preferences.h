//
//  preferences.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 18/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef preferences_h
#define preferences_h

#include <stdio.h>

#include "utility.h"

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

void preferences_init(void);

void preferences_read_from_disk(void);
int preferences_save_to_disk(void);

#endif /* preferences_h */
