//
//  app.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 14/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef app_h
#define app_h

#include "pd_api.h"
#include "scene.h"
#include "utility.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#if defined(TARGET_PLAYDATE) && !defined(TARGET_SIMULATOR) && !defined(TARGET_DEVICE)
#define TARGET_DEVICE 1
#endif

#if defined(TARGET_SIMULATOR)
#include <pthread.h>
extern pthread_mutex_t audio_mutex;
#endif

#if !defined(TARGET_DEVICE) && defined(DTCM_ALLOC)
#undef DTCM_ALLOC
#endif

// TODO: is this safe? should we lower it?
#define PLAYDATE_STACK_SIZE 0x2280

#define FPS_AVG_DECAY 0.8f

#define TENDENCY_BASED_ADAPTIVE_INTERLACING 1

typedef struct PGB_Application
{
    float dt;
    float avg_dt;       // for fps calculation
    float avg_dt_mult;  // reciprocal number of emulated frames last frame
    float crankChange;
    uint8_t* bootRomData;
    PGB_Scene* scene;
    PGB_Scene* pendingScene;
    LCDFont* bodyFont;
    LCDFont* titleFont;
    LCDFont* subheadFont;
    LCDFont* labelFont;
    LCDBitmapTable* selectorBitmapTable;
    LCDBitmap* startSelectBitmap;
    SoundSource* soundSource;
    PDButtons buttons_down;
    PDButtons buttons_pressed;
    PDButtons buttons_suppress;  // prevent these from registering until they
                                 // are released
} PGB_Application;

extern PGB_Application* PGB_App;

void PGB_init(void);
void PGB_event(PDSystemEvent event, uint32_t arg);
void PGB_update(float dt);
void PGB_present(PGB_Scene* scene);
void PGB_quit(void);
void PGB_goToLibrary(void);
void PGB_presentModal(PGB_Scene* scene);
void PGB_dismiss(PGB_Scene* scene);

// allocates in DTCM region (if enabled).
// note, there is no associated free.
void* dtcm_alloc(size_t size);

#define PLAYDATE_ROW_STRIDE 52

// relocatable and tightly-packed interpreter code
#ifdef TARGET_SIMULATOR
#define __core
#define __core_section(x)
#define __space
#else
#define __space __attribute__((optimize("Os")))
#ifdef ITCM_CORE
#define __core \
    __attribute__((optimize("Os"))) __attribute__((section(".itcm"))) __attribute__((short_call))
#define __core_section(x) \
    __attribute__((optimize("Os"))) __attribute__((section(".itcm." x))) __attribute__((short_call))
#else
#define __core __attribute__((optimize("Os"))) __attribute__((section(".text.itcm")))
#define __core_section(x) __core
#endif
#endif

// Any function which a __core fn can call MUST be marked as long_call (i.e.
// __shell) to ensure portability.
#ifdef TARGET_SIMULATOR
#define __shell
#else
#ifdef ITCM_CORE
#define __shell                                                     \
    __attribute__((long_call)) __attribute((noinline)) __section__( \
        ".text."                                                    \
        "pgb"                                                       \
    )
#else
#define __shell __attribute((noinline)) __section__(".text.pgb")
#endif
#endif

#ifdef ITCM_CORE
extern char __itcm_start[];
extern char __itcm_end[];
extern void* core_itcm_reloc;
#define itcm_core_size ((uintptr_t)&__itcm_end - (uintptr_t)&__itcm_start)
#define ITCM_CORE_FN(fn) \
    ((typeof(fn)*)((uintptr_t)(void*)&fn - (uintptr_t)&__itcm_start + core_itcm_reloc))
void itcm_core_init(void);
#else
#define ITCM_CORE_FN(fn) fn
#endif

#ifndef ENABLE_BGCACHE
#define ENABLE_BGCACHE 0
#endif

// don't exceed 60 fps
#define CAP_FRAME_RATE 1

#define SAVE_STATE_SLOT_COUNT 10
#define SAVE_STATE_THUMBNAIL_W 160
#define SAVE_STATE_THUMBNAIL_H 144

// for playdate extension crank menu IO register;
// how far one has to turn the crank before getting to the next menu item
#define CRANK_MENU_DELTA_BINANGLE 0x2800

#endif /* app_h */
