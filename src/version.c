#include "version.h"

#include "pd_api.h"
#include "utility.h"

void check_for_updates(void)
{
    playdate->network->setEnabled(false, NULL);
}