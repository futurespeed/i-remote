#include <Arduino.h>
#include <string.h>
#include "KeyScanManager.h"

void KeyScanManager::init(uint8_t write_pins[], uint8_t read_pins[],
                          const char *keys[], uint8_t keysNum,
                          void (*keyPress)(const char *key, KeyPressType KeyPressType))
{
    for (int i = 0; i < KEY_SCAN_ROWS; i++)
    {
        this->writePins[i] = write_pins[i];
        pinMode(writePins[i], OUTPUT);
        digitalWrite(writePins[i], HIGH);
    }
    for (int i = 0; i < KEY_SCAN_COLS; i++)
    {
        this->readPins[i] = read_pins[i];
        pinMode(readPins[i], INPUT);
    }
    for (int i = 0; i < keysNum; i++)
    {
        String s = String(keys[i]);
        memset(&this->keyObjs[i].key, 0, KEY_SCAN_KEY_CODE_LEN);
        memcpy(&this->keyObjs[i].key, s.c_str(), strlen(keys[i]));
    }
    this->keyPress = keyPress;
}

void KeyScanManager::scan()
{
    if (!checkSkip(&skipConfig))
        return;
    digitalWrite(writePins[currentRow], HIGH);
    currentRow = (currentRow + 1) % KEY_SCAN_ROWS;
    digitalWrite(writePins[currentRow], LOW);
    KeyObj *keyObj;
    for (int i = 0; i < KEY_SCAN_COLS; i++)
    {
        readValues[i] = digitalRead(readPins[i]);
        keyObj = &keyObjs[currentRow * KEY_SCAN_COLS + i];
        if (readValues[i])
        {
            if (keyObj->stayNum > KEY_SCAN_LONG_PRESS_MIN_NUM)
            {
                // long press
                keyPress(keyObj->key, KeyPressType::PRESS_LONG);
            }
            else if (keyObj->stayNum > KEY_SCAN_SHORT_PRESS_MIN_NUM)
            {
                // short press
                keyPress(keyObj->key, KeyPressType::PRESS_SHORT);
            }
            keyObj->stayNum = 0;
        }
        else
        {
            keyObj->stayNum++;
        }
    }
}
