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

extern bool preferences_display_fps;
extern bool preferences_frame_skip;
extern bool preferences_itcm;
extern int preferences_sound_mode;
extern int preferences_crank_mode;

void preferences_init(void);

void preferences_read_from_disk(void);
int preferences_save_to_disk(void);

#endif /* preferences_h */
