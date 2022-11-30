#include <stdint.h>
#include "Skipper.h"

#define KEY_SCAN_ROWS 4
#define KEY_SCAN_COLS 4
#define KEY_SCAN_FPS 200
#define KEY_SCAN_KEY_CODE_LEN 30
#define KEY_SCAN_SHORT_PRESS_MIN_NUM 8
#define KEY_SCAN_LONG_PRESS_MIN_NUM 100

typedef enum
{
    PRESS_SHORT = 0,
    PRESS_LONG
} KeyPressType;

typedef struct
{
    char key[KEY_SCAN_KEY_CODE_LEN];
    uint8_t stayNum;
} KeyObj;

class KeyScanManager
{
public:
    void init(uint8_t write_pins[], uint8_t read_pins[], const char *keys[], uint8_t keysNum, void (*keyPress)(const char *key, KeyPressType type));
    void scan();

private:
    KeyObj keyObjs[KEY_SCAN_ROWS * KEY_SCAN_COLS];
    uint8_t writePins[KEY_SCAN_ROWS];
    uint8_t readPins[KEY_SCAN_COLS];
    void (*keyPress)(const char *key, KeyPressType type);
    uint8_t currentRow;
    uint8_t readValues[KEY_SCAN_COLS];
    SkipConfig skipConfig = {KEY_SCAN_FPS, 0};
};