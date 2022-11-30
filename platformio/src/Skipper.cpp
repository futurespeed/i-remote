#include <Arduino.h>
#include "Skipper.h"

bool checkSkip(SkipConfig *skipConfig)
{
    return checkSkipWithTime(skipConfig, millis());
}

bool checkSkipWithTime(SkipConfig *skipConfig, unsigned long currTime)
{
    if (currTime - skipConfig->lastTriggerTime < 1000 / skipConfig->fps)
        return false;
    skipConfig->lastTriggerTime = currTime;
    return true;
}