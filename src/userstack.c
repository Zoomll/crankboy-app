#ifdef TARGET_PLAYDATE

#include "userstack.h"

#include "pd_api.h"
#include "utility.h"

#define USER_STACK_SIZE 0x4000
#define CANARY_VALUE 0x5AC3FA3B

static char user_stack[USER_STACK_SIZE] __attribute__((aligned(8)));

__section__(".rare") static uint32_t *get_stack_start_canary(void)
{
    return (uint32_t *)(user_stack);
}

__section__(".rare") static uint32_t *get_stack_end_canary(void)
{
    return (uint32_t *)(user_stack + USER_STACK_SIZE - sizeof(uint32_t));
}

__section__(".rare") void validate_user_stack(void)
{
    if (*get_stack_start_canary() != CANARY_VALUE ||
        *get_stack_end_canary() != CANARY_VALUE)
    {
        playdate->system->error("User stack canary corrupted");
    }
}

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

__attribute__((naked)) __section__(".rare") void *call_with_user_stack_impl(
    user_stack_fn fn, void *arg, void *arg2)
{
    __asm__ volatile (
        
        "push {lr}\n"
            // r3 <- user stasck base
            // lr <- (user stack base + size - 4)
            "ldr r3, =user_stack\n"
            "ldr lr, =" STRINGIFY(USER_STACK_SIZE) "\n"
            
            "add lr, r3, lr\n"
            "sub lr, lr, #4\n"
            
            // check that we're not already on user stack
            "cmp sp, r3\n"
            "blo not_on_user_stack\n"
            "cmp sp, lr\n"
            "bls already_on_user_stack\n"
            
        "not_on_user_stack:\n"
        
            // swap lr and sp
            // (sp <- user stack base + size - 4)
            "mov r3, lr\n"
            "mov lr, sp\n"
            "mov sp, r3\n"
            
            // save original sp while invoking fn
            "push {lr}\n"
                "bl shift_and_invoke\n"
            "pop {lr}\n"
            
            // restore original SP
            "mov sp, lr\n"
        
        // return
        "pop {pc}\n"
        
    "shift_and_invoke:\n"
        "push {lr}\n"
            // (fallthrough)
        "already_on_user_stack:\n"
            // r3 <- fn, and shift arguments down
            "mov r3, r0\n"
            "mov r0, r1\n"
            "mov r1, r2\n"
            "blx r3\n"   // call fn(args...)
            
            "push {r0}\n"
                "bl validate_user_stack\n"
                
        // return r0
        "pop {r0, pc}\n"
    );
}

__section__(".rare") void init_user_stack(void)
{
    *get_stack_start_canary() = CANARY_VALUE;
    *get_stack_end_canary() = CANARY_VALUE;
}

#else

void init_user_stack(void)
{
}

#endif