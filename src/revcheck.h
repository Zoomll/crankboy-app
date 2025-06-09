#pragma once

#define PD_REV_UNDEFINED 0 /* failed to detect (run revcheck() manually) */
#define PD_REV_A 1
#define PD_REV_B 2
#define PD_REV_UNKNOWN 1000
#define PD_REV_SIMULATOR -1

extern int pd_rev;
extern const char* pd_rev_description;

// calculates pd_rev (should run automatically on startup)
void pd_revcheck(void);