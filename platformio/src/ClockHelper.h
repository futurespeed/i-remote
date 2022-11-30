#include <stdint.h>

class ClockHelper
{
public:
    uint64_t getTime();
    void setTime(uint64_t time);
    uint16_t getYear();
    uint8_t getMonth();
    uint8_t getDay();
    uint8_t getHour();
    uint8_t getMinute();
    uint8_t getSecond();
    void refresh();

private:
    uint64_t bootDelay;
    uint64_t time;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};