#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>
#include <ArduinoJson.h>

#include "KeyScanManager.h"
#include "ClockHelper.h"
// #include "font_custom24.h"
#include "img_learning.h"

#define PIN_TFT_LED 16
#define PIN_IR_RX 13
#define PIN_IR_TX 12
#define PIN_SLEEP_TOUCH 14
#define PWM_CHANNEL_TFT_LED 2

#define LEARN_MIN_TIMES 3
#define LEARN_MAX_TIMES 5
#define AUTO_SLEEP_DELAY 300 // uint: second

const char *configFile = "/config.json";
// const char *MSG_KEY_LEARN = "请选择需学习的按键";
// const char *MSG_IR_RECV = "接收红外信号";
// const char *MSG_IR_RECV_REPEAT = "再次接收红外信号";
// const char *MSG_LEARN_SUCCESS = "学习成功";
// const char *MSG_LEARN_FAIL = "学习失败";

typedef enum
{
  LOADING = 0,
  STANDBY,
  LEARNING,
  REMOTE,
  TIP,
  SETTING
} RunningMode;

typedef enum
{
  CHOICE_BUTTON = 0,
  WAIT_RECV
} LearningStep;

typedef struct
{
  uint64_t delayTime;
  uint64_t delayBeginTime;
  boolean runningModeChange;
  RunningMode runningMode;
  boolean learningStepChange;
  LearningStep learningStep;
  void (*callback)();
} DelayParam;

void lvglInit();
void lvglDisplayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void standbyView();
void learningView();
void remoteView();
void tipView();
void settingView();
void settingViewRefresh();

void runningModeChange(RunningMode mode);
void refreshDisplay();
void btnPress(const char *key, KeyPressType type);
void configInit();
void loadConfig();
void storageConfig();
void loadConfigRemote();
void storageConfigRemote();
void irSend(const char *key, KeyPressType type);
void irScan();
void setDelay(uint64_t delayTime);
void delayScan();
void sleepScan();
void notifyActive();
void sleepCallback();
// void printTftString(const char *msg, uint8_t x, uint8_t y);

void mqttCallback(char *topic, byte *payload, unsigned int length);
void wifiConnect();
void mqttInit();
void toggleMqtt();
void mqttConnect();
void syncRemoteTime();
String getCurrentTime();

WiFiClient httpClient;
const char *mqttSubTopic = "i-remote-server";
const char *mqttPubTopic = "i-remote-client";
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

TFT_eSPI tft = TFT_eSPI();
static lv_disp_buf_t lvDispBuf;
static lv_color_t lvColorBuf[LV_HOR_RES_MAX * 10];

KeyScanManager keyManager = KeyScanManager();
uint8_t btnReadPins[] = {32, 33, 34, 35};
uint8_t btnWritePins[] = {22, 25, 26, 27};
const char *btnKeys[] = {"power", "mode", "sence", "quick",
                         "A", "B", "C", "D",
                         "menu", "up", "cancel", "right",
                         "left", "ok", "down", "fn"};

ClockHelper clockHelper = ClockHelper();

IRrecv irr(PIN_IR_RX);
IRsend irs(PIN_IR_TX);
decode_results irResult;

StaticJsonDocument<4096> json;

RunningMode runningMode = RunningMode::LOADING;
bool isDisplayChange = false;
uint8_t currentScene = 0;
uint8_t sceneSize = 0;
uint8_t currentRemoteClient = 0;
uint8_t remoteClientSize = 0;

LearningStep learningStep = LearningStep::CHOICE_BUTTON;
String learningKey = "quick";
uint8_t learningRecvCnt = 0;
uint64_t learningVals[LEARN_MAX_TIMES];
uint8_t learningCnts[LEARN_MAX_TIMES];

uint64_t lastActiveTime = 0;
String currentDeviceId = "";

DelayParam delayParam;

lv_obj_t *viewBgStandby;
lv_obj_t *viewBgLearning;
lv_obj_t *viewBgRemote;
lv_obj_t *viewBgSetting;
lv_obj_t *viewBgTip;
lv_obj_t *labelSence;
lv_obj_t *labelStateMqtt;
lv_obj_t *labelRemoteClient;
lv_obj_t *labelTip;
lv_obj_t *labelTipLearning;
lv_obj_t *labelSettingInfo;
lv_obj_t *labelSettingSync;

const char *menuSettingNames[] = {"Info", "Sync"};
const uint8_t menuSettingLen = sizeof(menuSettingNames) / sizeof(*menuSettingNames);
lv_obj_t *menuSettings[menuSettingLen];
uint8_t currentSettingMenu = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000); // test
  Serial.println("i-Remote init...");

  Serial.println("IO init...");
  pinMode(PIN_TFT_LED, OUTPUT);
  ledcSetup(PWM_CHANNEL_TFT_LED, 1000, 10);
  ledcAttachPin(PIN_TFT_LED, PWM_CHANNEL_TFT_LED);

  Serial.println("TFT init...");
  tft.begin();
  tft.setRotation(2);
  lvglInit();
  standbyView();
  learningView();
  remoteView();
  tipView();
  settingView();

  tft.fillScreen(TFT_BLACK);
  delay(50);
  ledcWrite(PWM_CHANNEL_TFT_LED, 800);
  tft.setCursor(68, 100, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("i-Remote");
  // printTftString("i-Remote", 68, 108);
  delay(10);

  Serial.println("KeyManager init...");
  keyManager.init(btnWritePins, btnReadPins, btnKeys, sizeof(btnKeys) / sizeof(*btnKeys), &btnPress);

  loadConfig();

  irr.enableIRIn();
  irs.begin();

  delay(2000);
  runningModeChange(RunningMode::STANDBY);

  notifyActive();
}

void loop()
{
  delayScan();
  keyManager.scan();
  irScan();
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }
  refreshDisplay();
  lv_task_handler();
  sleepScan();
}

void runningModeChange(RunningMode mode)
{
  Serial.printf("Running mode change to [%d]\r\n", mode);
  runningMode = mode;
  isDisplayChange = true;
}

void refreshDisplay()
{
  if (isDisplayChange)
  {
    lv_obj_set_hidden(viewBgStandby, true);
    lv_obj_set_hidden(viewBgLearning, true);
    lv_obj_set_hidden(viewBgRemote, true);
    lv_obj_set_hidden(viewBgTip, true);
    lv_obj_set_hidden(viewBgSetting, true);

    if (RunningMode::STANDBY == runningMode)
    {
      // tft.fillScreen(TFT_BLACK);
      // tft.setCursor(50, 80, 4);
      // tft.setTextColor(TFT_WHITE, TFT_BLACK);
      // // tft.println("i-Remote");
      // printTftString("i-Remote", 68, 108);

      lv_obj_set_hidden(viewBgStandby, false);
    }
    if (RunningMode::LEARNING == runningMode)
    {
      lv_obj_set_hidden(viewBgLearning, false);
    }
    if (RunningMode::TIP == runningMode)
    {
      lv_obj_set_hidden(viewBgTip, false);
    }
    if (RunningMode::REMOTE == runningMode)
    {
      lv_obj_set_hidden(viewBgRemote, false);
    }
    if (RunningMode::SETTING == runningMode)
    {
      lv_obj_set_hidden(viewBgSetting, false);
    }
  }
  isDisplayChange = false;
}

void btnPress(const char *key, KeyPressType type)
{
  Serial.printf("key press: %s[%d]\r\n", key, type);

  notifyActive();

  if (RunningMode::STANDBY == runningMode)
  {
    if (!strcmp(key, "mode") && KeyPressType::PRESS_LONG == type)
    {
      runningModeChange(RunningMode::SETTING);
      return;
    }
    if (!strcmp(key, "mode") && KeyPressType::PRESS_SHORT == type && mqttClient.connected())
    {
      runningModeChange(RunningMode::REMOTE);
      return;
    }
    if (!strcmp(key, "sence") && KeyPressType::PRESS_SHORT == type)
    {
      currentScene = (currentScene + 1) % sceneSize;
      Serial.printf("change to scene[%d]\r\n", currentScene);
      String sceneName = json["scenes"][currentScene]["name"];
      lv_label_set_text(labelSence, sceneName.c_str());
      isDisplayChange = true;
      return;
    }
    if (!strcmp(key, "sence") && KeyPressType::PRESS_LONG == type)
    {
      toggleMqtt();
      return;
    }
    if (!strcmp(key, "quick") && KeyPressType::PRESS_LONG == type)
    {
      learningStep = LearningStep::CHOICE_BUTTON;
      runningModeChange(RunningMode::LEARNING);
      memset(&learningVals, 0, LEARN_MAX_TIMES);
      memset(&learningCnts, 0, LEARN_MAX_TIMES);
      // tft.fillScreen(TFT_BLACK);
      // tft.setCursor(20, 80, 4);
      // tft.setTextColor(TFT_WHITE, TFT_BLACK);
      // tft.println("Please press key to learn");
      // learningView();
      // printTftString(MSG_KEY_LEARN, 12, 128);
      lv_label_set_text(labelTipLearning, "Please press button to learn");
      return;
    }
    irSend(key, type);
    return;
  }

  if (RunningMode::LEARNING == runningMode && !strcmp(key, "quick") && KeyPressType::PRESS_LONG == type)
  {
    runningModeChange(RunningMode::STANDBY);
    return;
  }

  if (RunningMode::LEARNING == runningMode && LearningStep::CHOICE_BUTTON == learningStep)
  {
    learningKey = String(key);
    learningRecvCnt = 0;

    // learningStep = LearningStep::WAIT_RECV;
    // tft.fillScreen(TFT_BLACK);
    // tft.setCursor(50, 80, 4);
    // tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.println("Please receive IR");
    // learningView();
    // printTftString(MSG_IR_RECV, 48, 128);
    // String keyMsg = "[ " + learningKey + " ]";
    // printTftString(keyMsg.c_str(), 72, 168);
    String msg = "Please receive IR for [" + learningKey + "]";
    lv_label_set_text(labelTipLearning, msg.c_str());
    delayParam.learningStep = LearningStep::WAIT_RECV;
    delayParam.learningStepChange = true;
    setDelay(1000);
    // delay(1000);
    return;
  }
  if (RunningMode::REMOTE == runningMode)
  {
    if (!strcmp(key, "mode") && KeyPressType::PRESS_SHORT == type)
    {
      runningModeChange(RunningMode::STANDBY);
      return;
    }
    if (!strcmp(key, "sence") && KeyPressType::PRESS_SHORT == type)
    {
      currentRemoteClient = (currentRemoteClient + 1) % remoteClientSize;
      String remoteClientName = json["remote-clients"][currentRemoteClient]["name"];
      remoteClientName = String("Target: ") + remoteClientName;
      lv_label_set_text(labelRemoteClient, remoteClientName.c_str());
      return;
    }
    String targetDeviceId = json["remote-clients"][currentRemoteClient]["code"];
    String msg = "{\"type\":\"ir-send\",\"time\":" + getCurrentTime() + ",\"deviceId\":\"" + targetDeviceId + "\",\"key\":\"" + String(key) + "\"}";
    mqttClient.publish(mqttSubTopic, msg.c_str());
    return;
  }
  if (RunningMode::SETTING == runningMode)
  {
    if (!strcmp(key, "mode") && KeyPressType::PRESS_LONG == type)
    {
      runningModeChange(RunningMode::STANDBY);
      return;
    }
    if (!strcmp(key, "up") && KeyPressType::PRESS_SHORT == type)
    {
      if (0 == currentSettingMenu)
      {
        currentSettingMenu = menuSettingLen;
      }
      currentSettingMenu--;
      settingViewRefresh();
      return;
    }
    if (!strcmp(key, "down") && KeyPressType::PRESS_SHORT == type)
    {
      currentSettingMenu = (currentSettingMenu + 1) % menuSettingLen;
      settingViewRefresh();
      return;
    }
    if (!strcmp(key, "ok") && KeyPressType::PRESS_SHORT == type)
    {
      if (1 == currentSettingMenu && WiFi.isConnected())
      {
        lv_label_set_text(labelTip, "Saving...");
        runningModeChange(RunningMode::TIP);
        delayParam.callback = storageConfigRemote;
        setDelay(500);
      }
      return;
    }
  }
}

void loadConfig()
{
  Serial.println("load config...");
  SPIFFS.begin();
  File file = SPIFFS.open(configFile, FILE_READ);
  DeserializationError error = deserializeJson(json, file);
  if (error)
    Serial.println("Failed to read file, using default configuration");
  file.close();
  SPIFFS.end();
  configInit();
}

void configInit()
{
  String code = json["code"];
  currentDeviceId = code;
  JsonArray scenes = json["scenes"];
  sceneSize = scenes.size();
  Serial.printf("load config: %s\r\n", currentDeviceId.c_str());
  Serial.printf("scenes: %d\r\n", sceneSize);
  String sceneName = json["scenes"][currentScene]["name"];
  lv_label_set_text(labelSence, sceneName.c_str());
  JsonArray remoteClients = json["remote-clients"];
  remoteClientSize = remoteClients.size();
  String remoteClientName = json["remote-clients"][currentRemoteClient]["name"];
  remoteClientName = String("Target: ") + remoteClientName;
  lv_label_set_text(labelRemoteClient, remoteClientName.c_str());
}

void storageConfig()
{
  Serial.println("storage config...");
  SPIFFS.begin();
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(configFile, FILE_WRITE);
  if (serializeJson(json, file) == 0)
  {
    Serial.println("Failed to write config file");
  }
  file.close();
  SPIFFS.end();
}

void loadConfigRemote()
{
  String deviceId = json["code"];
  Serial.printf("load remote config [%s]...\r\n", deviceId.c_str());
  const char *host = "www.futurespeed.cn";
  uint16_t port = 80;
  String url = "/cloud-album/api/i-remote/config/" + deviceId;
  if (!httpClient.connect(host, port))
  {
    Serial.println("connection failed");
    return;
  }
  delay(10);

  String postRequest = (String)("POST ") + url + " HTTP/1.1\r\n" +
                       "Content-Type: text/html;charset=utf-8\r\n" +
                       "Host: " + host + "\r\n" +
                       "User-Agent: i-Remote\r\n" +
                       "Connection: Keep Alive\r\n\r\n";
  Serial.print("HTTP send: ");
  Serial.println(postRequest);
  httpClient.print(postRequest);

  Serial.print("HTTP receive: ");
  String jsonStr;
  String line = httpClient.readStringUntil('\n');
  while (line.length() != 0)
  {
    Serial.println(line);
    if (line == "\r")
    {
      jsonStr = httpClient.readStringUntil('\n');
      break;
    }
    line = httpClient.readStringUntil('\n');
  }
  httpClient.stop();
  Serial.print("HTTP receive json: ");
  Serial.println(jsonStr);

  json.clear();
  deserializeJson(json, jsonStr);
  configInit();
}

void storageConfigRemote()
{
  String deviceId = json["code"];
  String sendJson;
  serializeJson(json, sendJson);
  Serial.printf("storage remote config [%s]...\r\n", deviceId.c_str());
  const char *host = "www.futurespeed.cn";
  uint16_t port = 80;
  String url = "/cloud-album/api/i-remote/config/" + deviceId;
  if (!httpClient.connect(host, port))
  {
    Serial.println("connection failed");
    return;
  }
  delay(10);

  String postRequest = (String)("PUT ") + url + " HTTP/1.1\r\n" +
                       "Content-Type: application/json;charset=utf-8\r\n" +
                       "Content-Length: " + sendJson.length() + "\r\n"
                                                                "Host: " +
                       host + "\r\n" +
                       "User-Agent: i-Remote\r\n" +
                       "Connection: Keep Alive\r\n\r\n";
  Serial.print("HTTP send: ");
  Serial.println(postRequest);
  httpClient.print(postRequest + sendJson);

  Serial.print("HTTP receive: ");
  String jsonStr;
  String line = httpClient.readStringUntil('\n');
  while (line.length() != 0)
  {
    Serial.println(line);
    if (line == "\r")
    {
      jsonStr = httpClient.readStringUntil('\n');
      break;
    }
    line = httpClient.readStringUntil('\n');
  }
  httpClient.stop();
  Serial.print("HTTP receive json: ");
  Serial.println(jsonStr);

  // lv_label_set_text(labelTip, "Save success");
  // runningModeChange(RunningMode::TIP);
  // delayParam.runningMode = RunningMode::SETTING;
  // delayParam.runningModeChange = true;
  // setDelay(2000);
  runningModeChange(RunningMode::SETTING);
}

void irSend(const char *key, KeyPressType type)
{
  String keyValue = json["scenes"][currentScene]["key-map"][String(key)];
  String irType = json["scenes"][currentScene]["type"];
  if (NULL == keyValue || 0 == keyValue.length() || String("null") == keyValue)
    return;
  Serial.printf("send IR value: %s\r\n", keyValue.c_str());
  char cmd[30];
  strcpy((char *)&cmd, keyValue.c_str());
  Serial.printf("IR send: [%s]\r\n", cmd);
  uint64_t num = 0;
  sscanf(cmd, "%llx", &num);

  if (irType == "sony")
  {
    irs.sendSony(0x4000 | num, num > 0xFFF ? 15 : 12);
  }
  else
  {
    irs.sendNEC(num);
  }
}

void irScan()
{
  if (RunningMode::LEARNING != runningMode || LearningStep::WAIT_RECV != learningStep)
    return;
  if (irr.decode(&irResult))
  {
    Serial.print("IR recv: ");
    String keyValue = uint64ToString(irResult.value, HEX);
    Serial.print(keyValue);
    Serial.println();
    irr.resume();
    notifyActive();

    bool found = false;
    if (learningRecvCnt > 0)
    {
      for (int i = 0; i < learningRecvCnt; i++)
      {
        if (irResult.value == learningVals[i])
        {
          learningCnts[i]++;
          found = true;
          break;
        }
      }
    }
    if (!found)
    {
      learningVals[learningRecvCnt] = irResult.value;
      learningCnts[learningRecvCnt] = 1;
    }
    bool learnSuccess = false;
    for (int i = 0; i < learningRecvCnt; i++)
    {
      if (learningCnts[i] >= LEARN_MIN_TIMES)
      {
        learnSuccess = true;
        break;
      }
    }

    if (learnSuccess)
    {
      json["scenes"][currentScene]["key-map"][learningKey] = keyValue;
      // tft.fillScreen(TFT_BLACK);
      // tft.setCursor(50, 80, 4);
      // tft.setTextColor(TFT_WHITE, TFT_BLACK);
      // tft.println("Learn success");
      // printTftString(MSG_LEARN_SUCCESS, 72, 108);

      storageConfig();
      // delay(2000);
      // runningModeChange(RunningMode::STANDBY);

      String msg = (String) "Learn success\r\ncode: [" + keyValue + "]";
      lv_label_set_text(labelTip, msg.c_str());
      runningModeChange(RunningMode::TIP);
      delayParam.runningMode = RunningMode::STANDBY;
      delayParam.runningModeChange = true;
      setDelay(3000);
      return;
    }
    learningRecvCnt++;

    if (learningRecvCnt >= LEARN_MAX_TIMES)
    {
      // tft.fillScreen(TFT_BLACK);
      // tft.setCursor(50, 80, 4);
      // tft.setTextColor(TFT_WHITE, TFT_BLACK);
      // tft.println("Learn fail");
      // printTftString(MSG_LEARN_FAIL, 72, 108);
      // delay(2000);
      // runningModeChange(RunningMode::STANDBY);
      lv_label_set_text(labelTip, "Learn fail");
      runningModeChange(RunningMode::TIP);
      delayParam.runningMode = RunningMode::STANDBY;
      delayParam.runningModeChange = true;
      setDelay(2000);
      return;
    }

    // tft.fillScreen(TFT_BLACK);
    // tft.setCursor(50, 80, 4);
    // tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.printf("Please receive IR again (%d)\r\n", learningRecvCnt);
    // learningView();
    // printTftString(MSG_IR_RECV_REPEAT, 24, 128);
    // String cntStr = "[ " + learningKey + ": " + String(learningRecvCnt) + " ]";
    // printTftString(cntStr.c_str(), 88, 168);
    String msg = "Please receive IR again (" + String(learningRecvCnt) + ")\r\n";
    lv_label_set_text(labelTipLearning, msg.c_str());
  }
}

void setDelay(uint64_t delayTime)
{
  delayParam.delayTime = delayTime;
  delayParam.delayBeginTime = millis();
  Serial.printf("===%d,%d,%d\r\n", delayParam.delayTime,delayParam.runningModeChange,delayParam.runningMode);
}

void delayScan()
{
  if (delayParam.delayTime > 0)
  {
    if (millis() - delayParam.delayBeginTime >= delayParam.delayTime)
    {
      Serial.printf("===tr: %d,%d\r\n", delayParam.runningModeChange,delayParam.runningMode);
      if (delayParam.runningModeChange)
      {
        runningModeChange(delayParam.runningMode);
      }
      if (delayParam.learningStepChange)
      {
        learningStep = delayParam.learningStep;
      }
      if (delayParam.callback != NULL)
      {
        delayParam.callback();
      }

      delayParam.delayTime = 0;
      delayParam.runningModeChange = false;
      delayParam.learningStepChange = false;
      delayParam.callback = NULL;
    }
  }
}

void sleepScan()
{
  if ((millis() - lastActiveTime) / 1000 > AUTO_SLEEP_DELAY)
  {
    // EXT0 wakeup
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, LOW);

    // // EXT1 wakeup
    // // mask: pin39,38,37,36,35,34,33,32
    // // use pin35 00001000
    // uint64_t sleepPinMask = 0x08;
    // esp_sleep_enable_ext1_wakeup(sleepPinMask, ESP_EXT1_WAKEUP_ALL_LOW);

    // // touch pin wakeup
    // esp_sleep_enable_touchpad_wakeup();
    // touchAttachInterrupt(PIN_SLEEP_TOUCH, sleepCallback, 40);

    delay(10);
    esp_deep_sleep_start();
  }
}

void notifyActive()
{
  lastActiveTime = millis();
}

void sleepCallback()
{
  Serial.println("Sleep wakeup...");
}

// void printTftString(const char *msg, uint8_t x, uint8_t y)
// {
//   tft.loadFont(font_custom24);
//   tft.drawString(msg, x, y);
//   tft.unloadFont();
// }

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("MQTT receive: ");
  Serial.println(topic);
  String msgStr = "";
  Serial.print("msg: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    msgStr = msgStr + (char)payload[i];
  }
  Serial.println();
  DynamicJsonDocument msgJson(1024);
  deserializeJson(msgJson, msgStr);
  JsonObject msgObj = msgJson.as<JsonObject>();
  if (String("ir-send") == msgObj["type"])
  {
    // Example: {"type":"ir-send","time":1653905097751,"deviceId":"jx","key":"fn"}
    uint64_t optTime = msgObj["time"];
    clockHelper.refresh();
    uint64_t currTime = clockHelper.getTime();
    if (currTime - optTime > 3000)
      return;
    String deviceId = msgObj["deviceId"];
    if (deviceId == currentDeviceId)
    {
      String sendKey = msgJson["key"];
      btnPress(sendKey.c_str(), KeyPressType::PRESS_SHORT);
    }
  }
}

void wifiConnect()
{
  String ssid = json["network-settings"]["wifi"]["ssid"];
  String password = json["network-settings"]["wifi"]["passwd"];
  Serial.print("Wifi connecting");
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Wifi connected, IP: ");
  Serial.println(WiFi.localIP());
}

void mqttInit()
{
  String mqttServer = json["network-settings"]["mqtt"]["ip"];
  String mqttPort = json["network-settings"]["mqtt"]["port"];
  String mqttUser = json["network-settings"]["mqtt"]["username"];
  String mqttPassword = json["network-settings"]["mqtt"]["passwd"];

  Serial.println("MQTT connecting...");
  mqttClient.setServer(mqttServer.c_str(), atoi(mqttPort.c_str()));
  mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected())
  {
    if (mqttClient.connect(mqttUser.c_str(), mqttUser.c_str(), mqttPassword.c_str()))
    {
      Serial.println("MQTT connected.");
    }
    else
    {
      Serial.print("MQTT connect fail.");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
  Serial.println("MQTT subscribe...");
  mqttClient.subscribe(mqttSubTopic);
  Serial.println("MQTT publish...");
  String deviceId = json["code"];
  String pubMsg = "{\"type\":\"event\",\"time\":" + getCurrentTime() + ",\"deviceId\":\"" + deviceId + "\",\"event\":\"connect\"}";
  mqttClient.publish(mqttPubTopic, pubMsg.c_str());
  lv_obj_set_hidden(labelStateMqtt, false);
}

void toggleMqtt()
{
  if (!mqttClient.connected())
  {
    // lv_obj_set_hidden(viewBgStandby, true);
    // tft.fillScreen(TFT_BLACK);
    // tft.setCursor(50, 80, 4);
    // tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // printTftString("Connecting...", 56, 108);

    lv_label_set_text(labelTip, "Connecting...");
    runningModeChange(RunningMode::TIP);
    delayParam.callback = mqttConnect;
    setDelay(500);
  }
  else
  {
    mqttClient.disconnect();
    WiFi.disconnect(true, true);
    lv_obj_set_hidden(labelStateMqtt, true);
  }
  // isDisplayChange = true;
}

void mqttConnect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    wifiConnect();
    syncRemoteTime();
  }
  mqttInit();
  runningModeChange(RunningMode::STANDBY);
}

void syncRemoteTime()
{
  const char *host = "www.futurespeed.cn";
  uint16_t port = 80;
  const char *url = "/cloud-album/api/album/info";
  if (!httpClient.connect(host, port))
  {
    Serial.println("connection failed");
    return;
  }
  delay(10);

  String postRequest = (String)("GET ") + url + " HTTP/1.1\r\n" +
                       "Content-Type: text/html;charset=utf-8\r\n" +
                       "Host: " + host + "\r\n" +
                       "User-Agent: i-Remote\r\n" +
                       "Connection: Keep Alive\r\n\r\n";
  Serial.print("HTTP send: ");
  Serial.println(postRequest);
  httpClient.print(postRequest);

  Serial.print("HTTP receive: ");
  String jsonStr;
  String line = httpClient.readStringUntil('\n');
  while (line.length() != 0)
  {
    Serial.println(line);
    if (line == "\r")
    {
      jsonStr = httpClient.readStringUntil('\n');
      break;
    }
    line = httpClient.readStringUntil('\n');
  }
  httpClient.stop();
  Serial.print("HTTP receive json: ");
  Serial.println(jsonStr);

  DynamicJsonDocument httpResp(1024);
  deserializeJson(httpResp, jsonStr);
  uint64_t currTime = httpResp["currTime"];
  clockHelper.setTime(currTime);
  Serial.printf("current time: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                clockHelper.getYear(),
                clockHelper.getMonth(),
                clockHelper.getDay(),
                clockHelper.getHour(),
                clockHelper.getMinute(),
                clockHelper.getSecond());
}

String getCurrentTime()
{
  clockHelper.refresh();
  char currTimeStr[20];
  sprintf(currTimeStr, "%lld", clockHelper.getTime());
  return String(currTimeStr);
}

/* Display flushing */
void lvglDisplayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void lvglInit()
{
  Serial.println("LVGL init...");
  lv_init();
  lv_disp_buf_init(&lvDispBuf, lvColorBuf, NULL, LV_HOR_RES_MAX * 10);
  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = lvglDisplayFlush;
  disp_drv.buffer = &lvDispBuf;
  lv_disp_drv_register(&disp_drv);
}

void standbyView()
{
  viewBgStandby = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_style_local_bg_color(viewBgStandby, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_border_opa(viewBgStandby, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
  lv_obj_set_style_local_radius(viewBgStandby, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(viewBgStandby, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_size(viewBgStandby, 240, 240);
  lv_obj_align(viewBgStandby, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_hidden(viewBgStandby, true);

  lv_obj_t *labelTitle;
  labelTitle = lv_label_create(viewBgStandby, NULL);
  lv_obj_set_style_local_text_color(labelTitle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTitle, "i-Remote");
  lv_obj_align(labelTitle, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

  lv_obj_t *cardA;
  cardA = lv_obj_create(viewBgStandby, NULL);
  lv_obj_set_size(cardA, 110, 110);
  lv_obj_align(cardA, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_border_opa(cardA, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);

  lv_obj_t *cardB;
  cardB = lv_obj_create(viewBgStandby, NULL);
  lv_obj_set_size(cardB, 110, 110);
  lv_obj_align(cardB, NULL, LV_ALIGN_CENTER, -140, 0);
  lv_obj_set_style_local_bg_opa(cardB, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_50);
  lv_obj_set_style_local_border_opa(cardB, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);

  lv_obj_t *cardC;
  cardC = lv_obj_create(viewBgStandby, NULL);
  lv_obj_set_size(cardC, 110, 110);
  lv_obj_align(cardC, NULL, LV_ALIGN_CENTER, 140, 0);
  lv_obj_set_style_local_bg_opa(cardC, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_50);
  lv_obj_set_style_local_border_opa(cardC, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);

  // lv_obj_t *btnA;
  // btnA = lv_obj_create(viewBgStandby, NULL);
  // lv_obj_set_size(btnA, 60, 50);
  // lv_obj_align(btnA, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 20);
  // lv_obj_t *btnB;
  // btnB = lv_obj_create(viewBgStandby, NULL);
  // lv_obj_set_size(btnB, 60, 50);
  // lv_obj_align(btnB, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 60, 20);
  // lv_obj_t *btnC;
  // btnC = lv_obj_create(viewBgStandby, NULL);
  // lv_obj_set_size(btnC, 60, 50);
  // lv_obj_align(btnC, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 120, 20);
  // lv_obj_t *btnD;
  // btnD = lv_obj_create(viewBgStandby, NULL);
  // lv_obj_set_size(btnD, 60, 50);
  // lv_obj_align(btnD, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 180, 20);

  // lv_obj_t *labelA;
  // labelA = lv_label_create(btnA, NULL);
  // lv_obj_set_style_local_text_color(labelA, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  // lv_label_set_text(labelA, "A");
  // lv_obj_align(labelA, NULL, LV_ALIGN_CENTER, 0, -8);
  // lv_obj_t *labelB;
  // labelB = lv_label_create(btnB, NULL);
  // lv_obj_set_style_local_text_color(labelB, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  // lv_label_set_text(labelB, "B");
  // lv_obj_align(labelB, NULL, LV_ALIGN_CENTER, 0, -8);
  // lv_obj_t *labelC;
  // labelC = lv_label_create(btnC, NULL);
  // lv_obj_set_style_local_text_color(labelC, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  // lv_label_set_text(labelC, "C");
  // lv_obj_align(labelC, NULL, LV_ALIGN_CENTER, 0, -8);
  // lv_obj_t *labelD;
  // labelD = lv_label_create(btnD, NULL);
  // lv_obj_set_style_local_text_color(labelD, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  // lv_label_set_text(labelD, "D");
  // lv_obj_align(labelD, NULL, LV_ALIGN_CENTER, 0, -8);

  String sceneName = json["scenes"][currentScene]["name"];

  labelSence = lv_label_create(viewBgStandby, NULL);
  lv_obj_set_style_local_text_color(labelSence, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_label_set_text(labelSence, sceneName.c_str());
  lv_obj_align(labelSence, NULL, LV_ALIGN_CENTER, 0, 0);

  labelStateMqtt = lv_label_create(viewBgStandby, NULL);
  lv_obj_set_style_local_text_color(labelStateMqtt, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelStateMqtt, "[MQ]");
  lv_obj_align(labelStateMqtt, NULL, LV_ALIGN_IN_TOP_RIGHT, -10, 10);
  lv_obj_set_hidden(labelStateMqtt, true);
}

void learningView()
{
  viewBgLearning = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_style_local_bg_color(viewBgLearning, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_border_opa(viewBgLearning, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
  lv_obj_set_style_local_radius(viewBgLearning, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(viewBgLearning, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_size(viewBgLearning, 240, 240);
  lv_obj_align(viewBgLearning, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_hidden(viewBgLearning, true);

  lv_obj_t *labelTitle;
  labelTitle = lv_label_create(viewBgLearning, NULL);
  lv_obj_set_style_local_text_color(labelTitle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTitle, "i-Remote");
  lv_obj_align(labelTitle, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

  LV_IMG_DECLARE(img_learning);
  lv_obj_t *imgLearning = lv_img_create(viewBgLearning, NULL);
  lv_img_set_src(imgLearning, &img_learning);
  lv_obj_align(imgLearning, NULL, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_size(imgLearning, 50, 44);

  labelTipLearning = lv_label_create(viewBgLearning, NULL);
  lv_obj_set_style_local_text_color(labelTipLearning, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTipLearning, "");
  lv_label_set_long_mode(labelTipLearning, LV_LABEL_LONG_BREAK);
  lv_label_set_align(labelTipLearning, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_width(labelTipLearning, 180);
  lv_obj_align(labelTipLearning, NULL, LV_ALIGN_CENTER, 0, 30);
}

void remoteView()
{
  viewBgRemote = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_style_local_bg_color(viewBgRemote, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_border_opa(viewBgRemote, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
  lv_obj_set_style_local_radius(viewBgRemote, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(viewBgRemote, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_size(viewBgRemote, 240, 240);
  lv_obj_align(viewBgRemote, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_hidden(viewBgRemote, true);

  lv_obj_t *labelTitle;
  labelTitle = lv_label_create(viewBgRemote, NULL);
  lv_obj_set_style_local_text_color(labelTitle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTitle, "i-Remote");
  lv_obj_align(labelTitle, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

  lv_obj_t *labelMsg;
  labelMsg = lv_label_create(viewBgRemote, NULL);
  lv_obj_set_style_local_text_color(labelMsg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelMsg, "[ Remote Mode ]");
  lv_obj_align(labelMsg, NULL, LV_ALIGN_CENTER, 0, -20);

  labelRemoteClient = lv_label_create(viewBgRemote, NULL);
  lv_obj_set_style_local_text_color(labelRemoteClient, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelRemoteClient, "");
  lv_label_set_long_mode(labelRemoteClient, LV_LABEL_LONG_BREAK);
  lv_label_set_align(labelRemoteClient, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_width(labelRemoteClient, 180);
  lv_obj_align(labelRemoteClient, NULL, LV_ALIGN_CENTER, 0, 20);
}

void tipView()
{
  viewBgTip = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_style_local_bg_color(viewBgTip, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_border_opa(viewBgTip, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
  lv_obj_set_style_local_radius(viewBgTip, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(viewBgTip, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_size(viewBgTip, 240, 240);
  lv_obj_align(viewBgTip, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_hidden(viewBgTip, true);

  lv_obj_t *labelTitle;
  labelTitle = lv_label_create(viewBgTip, NULL);
  lv_obj_set_style_local_text_color(labelTitle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTitle, "i-Remote");
  lv_obj_align(labelTitle, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

  labelTip = lv_label_create(viewBgTip, NULL);
  lv_obj_set_style_local_text_color(labelTip, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTip, "");
  lv_label_set_long_mode(labelTip, LV_LABEL_LONG_BREAK);
  lv_label_set_align(labelTip, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_width(labelTip, 180);
  lv_obj_align(labelTip, NULL, LV_ALIGN_CENTER, 0, 0);
}

void settingView()
{
  viewBgSetting = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_style_local_bg_color(viewBgSetting, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_border_opa(viewBgSetting, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
  lv_obj_set_style_local_radius(viewBgSetting, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_pad_inner(viewBgSetting, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_size(viewBgSetting, 240, 240);
  lv_obj_align(viewBgSetting, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_hidden(viewBgSetting, true);

  lv_obj_t *labelTitle;
  labelTitle = lv_label_create(viewBgSetting, NULL);
  lv_obj_set_style_local_text_color(labelTitle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelTitle, "Setting");
  lv_obj_align(labelTitle, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

  for (uint8_t i = 0; i < menuSettingLen; i++)
  {
    menuSettings[i] = lv_obj_create(viewBgSetting, NULL);
    lv_obj_set_style_local_bg_color(menuSettings[i], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_set_size(menuSettings[i], 75, 32);
    lv_obj_align(menuSettings[i], NULL, LV_ALIGN_IN_TOP_LEFT, -5, 40 + i * 30);
    lv_obj_t *labelSetting;
    labelSetting = lv_label_create(menuSettings[i], NULL);
    lv_obj_set_style_local_text_color(labelSetting, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_label_set_text(labelSetting, menuSettingNames[i]);
    lv_obj_align(labelSetting, NULL, LV_ALIGN_CENTER, 0, 0);
  }

  labelSettingInfo = lv_label_create(viewBgSetting, NULL);
  lv_obj_set_width(labelSettingInfo, 130);
  lv_obj_set_height(labelSettingInfo, 120);
  lv_obj_set_style_local_text_color(labelSettingInfo, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  String lvglVersionStr = (String)LVGL_VERSION_MAJOR + "." + LVGL_VERSION_MINOR + "." + LVGL_VERSION_PATCH;
  String sysInfoStr = "[i-Remote]\r\nFirmware: v0.1.0\r\nMCU: ESP32-S\r\nLVGL: " + lvglVersionStr + "\r\n";
  lv_label_set_text(labelSettingInfo, sysInfoStr.c_str());
  lv_obj_align(labelSettingInfo, NULL, LV_ALIGN_IN_TOP_LEFT, 90, 60);

  labelSettingSync = lv_label_create(viewBgSetting, NULL);
  lv_obj_set_width(labelSettingSync, 130);
  lv_obj_set_height(labelSettingSync, 120);
  lv_obj_set_style_local_text_color(labelSettingSync, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_label_set_text(labelSettingSync, "Storage to cloud");
  lv_obj_align(labelSettingSync, NULL, LV_ALIGN_IN_TOP_LEFT, 90, 60);

  settingViewRefresh();
  // TODO
}

void settingViewRefresh()
{
  lv_obj_set_hidden(labelSettingInfo, true);
  lv_obj_set_hidden(labelSettingSync, true);
  for (uint8_t i = 0; i < menuSettingLen; i++)
  {
    if (i == currentSettingMenu)
    {
      lv_obj_set_style_local_bg_color(menuSettings[i], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xFFFFFF));
    }
    else
    {
      lv_obj_set_style_local_bg_color(menuSettings[i], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xCCCCCC));
    }
  }
  if (0 == currentSettingMenu)
  {
    lv_obj_set_hidden(labelSettingInfo, false);
  }
  if (1 == currentSettingMenu)
  {
    lv_obj_set_hidden(labelSettingSync, false);
  }
}