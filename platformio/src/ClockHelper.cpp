#include <Arduino.h>
#include <time.h>
#include "ClockHelper.h"

void ClockHelper::setTime(uint64_t time)
{
    this->time = time;
    this->bootDelay = time - millis();
    refresh();
}

void ClockHelper::refresh()
{
    this->time = this->bootDelay + millis();
    time_t t = (this->time + 8 * 3600000) / 1000; // GTM+8
    struct tm *p = gmtime(&t);
    this->year = p->tm_year + 1900;
    this->month = p->tm_mon;
    this->day = p->tm_mday;
    this->hour = p->tm_hour;
    this->minute = p->tm_min;
    this->second = p->tm_sec;
}

uint64_t ClockHelper::getTime()
{
    return time;
}
uint16_t ClockHelper::getYear()
{
    return year;
}
uint8_t ClockHelper::getMonth()
{
    return month;
}
uint8_t ClockHelper::getDay()
{
    return day;
}
uint8_t ClockHelper::getHour()
{
    return hour;
}
uint8_t ClockHelper::getMinute()
{
    return minute;
}
uint8_t ClockHelper::getSecond()
{
    return second;
}