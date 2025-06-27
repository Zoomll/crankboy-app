PREF(per_game, 0)
PREF(save_state_slot, 0)

// audio
PREF(sound_mode, 2)
PREF(sample_rate, (pd_rev == PD_REV_A) ? 1 : 0)

// display
PREF(frame_skip, true)
PREF(dither_pattern, 0)
PREF(dynamic_rate, DYNAMIC_RATE_OFF)
PREF(dynamic_level, 5)
PREF(transparency, 0)

// behaviour
PREF(crank_mode, CRANK_MODE_START_SELECT)
PREF(joypad_interrupts, 0)
PREF(overclock, 0)
PREF(lua_support, false)

// misc
PREF(itcm, (pd_rev == PD_REV_A))
PREF(uncap_fps, false)
PREF(display_fps, 0)
PREF(ui_sounds, 1)

#undef PREF
