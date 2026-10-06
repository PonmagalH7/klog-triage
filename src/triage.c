#include "triage.h"

int should_triage(unsigned level)
{
    return level <= 4;
}
