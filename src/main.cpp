#include <Arduino.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "secret.h"
#include "config.h"

/*------------------------------------------------------------------------------------*/
/* Constant Definitions                                                               */
/*------------------------------------------------------------------------------------*/
// Access point to configure Wi-Fi
const char* ACCESS_POINT_NAME = "ESP8266";
const char* ACCESS_POINT_PASS = "esp8266";

/*------------------------------------------------------------------------------------*/
/* GPIO Definitions                                                                   */
/*------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------*/
/* GPIO Definitions                                                                   */
/*------------------------------------------------------------------------------------*/
const uint8_t GPIO_ANALOG_00 = 0;          // ESP8266 NodeMCU A0
const uint8_t GPIO_UNUSED_00 = 0;          // ESP8266 NodeMCU D3
const uint8_t GPIO_UNUSED_01 = 1;          // ESP8266 NodeMUC TX (UART)
const uint8_t GPIO_WHITE = 2;              // ESP8266 NodeMUC D4 (Boot mode. Do not user for INPUT)
const uint8_t GPIO_UNUSED_03 = 3;          // ESP8266 NodeMCU RX (UART)
const uint8_t GPIO_DISPLAY_SDA = 4;        // ESP8266 NodeMCU D2 (SDA) 
const uint8_t GPIO_DISPLAY_SCL = 5;        // ESP8266 NodeMCU D1 (SCL)
const uint8_t GPIO_UNUSED_06 = 6;          // ESP8266 NodeMCU -+ F M
const uint8_t GPIO_UNUSED_07 = 7;          // ESP8266 NodeMCU  + L E
const uint8_t GPIO_UNUSED_08 = 8;          // ESP8266 NodeMCU  + A M
const uint8_t GPIO_UNUSED_09 = 9;          // ESP8266 NodeMCU  + S O
const uint8_t GPIO_UNUSED_10 = 10;         // ESP8266 NodeMCU  + H R
const uint8_t GPIO_UNUSED_11 = 11;         // ESP8266 NodeMCU -+   Y
const uint8_t GPIO_RED = 12;               // ESP8266 NodeMCU D6
const uint8_t GPIO_BLUE = 13;              // ESP8266 NodeMUC D7
const uint8_t GPIO_GREEN = 14;             // ESP8266 NodeMCU D5
const uint8_t GPIO_UNUSED_15 = 15;         // ESP8266 NodeMCU D8 (Boot from SD Card)
const uint8_t GPIO_UNUSED_16 = 16;         // ESP8266 NodeMCU D0

/*------------------------------------------------------------------------------------*/
/* Effects Definitions                                                                */
/*------------------------------------------------------------------------------------*/
const uint8_t COLORFUL = 0;         // Flash: No, Persistent: Yes, Fade: No
const uint8_t CROSS_FADE_SLOW = 1;  // Flash: No, Persistent: Yes, Fade: yes   TODO
const uint8_t CROSS_FADE_FAST = 2;  // Flash: No, Persistent: Yes, Fade: Yes   TODO
const uint8_t FLASH = 3;            // Flash: Yes, Persistent: No, Fade: No
const uint8_t CHRISTMAS = 4;        // Flash: Yes, Persistent: Yes, Fade: No
const uint8_t PURE_WHITE = 5;       // Flash: No, Persistent: Yes, Fade: No
const uint8_t FOUR_OF_JULY = 6;     // Flash: Yes, Persistent: Yes, Fade: No
const uint8_t MAX_EFFECTS = 7;

typedef struct {
  bool isFlashingEffect;
  bool isPersistentEffect;
  bool isFadingEffect;
  int8_t falsh_count;
} EffectParams;

const EffectParams effectParams[] = {
  {false, true, false, -1}, // COLORFUL
  {false, true, true, -1},  // CROSS_FADE_SLOW
  {false, true, true, -1},  // CROSS_FADE_FAST
  {true, false, false, 10}, // FLASH
  {true, true, false, -1},  // CHRISTMAS
  {false, true, false, -1}, // PURE_WHITE
  {true, true, false, -1}   // FOUR_OF_JULY
};

const char *STR_COLORFUL = "colorful";
const char *STR_CROSS_FADE_SLOW = "colorfade_slow";
const char *STR_CROSS_FADE_FAST = "colorfade_fast";
const char *STR_FLASH = "flash";
const char *STR_CHRISTMAS = "christmas";
const char *STR_PURE_WHITE = "pure_white";
const char *STR_FOUR_OF_JULY = "four_of_july";

const uint16_t DEFAULT_FLASHING_ON_TIME_MS = 1000;
const uint16_t DEFAULT_FLASHING_OFF_TIME_MS = 1000;
const uint8_t DEFAULT_FLASHING_COUNT = 10;

/*------------------------------------------------------------------------------------*/
/* State Definitions                                                                  */
/*------------------------------------------------------------------------------------*/
const uint8_t STATE_OFF = 0;
const uint8_t STATE_ON = 1;
const char *STR_STATE_ON = "ON";
const char *STR_STATE_OFF = "OFF";

/*------------------------------------------------------------------------------------*/
/* Flashing cycles                                                                    */
/*------------------------------------------------------------------------------------*/
// Flashing with current set colors
// Cycle: r, g, b, w, t.
// -1 means current color
// t time in miliseconds
const int16_t USE_CURRENT_SET_COLORS = -1;
const int8_t FLASH_FOREVER = -1;
const uint8_t CYCLE_COLUMNS = 5;
const int16_t CYCLE_FLASH[][CYCLE_COLUMNS] = {
  {USE_CURRENT_SET_COLORS, USE_CURRENT_SET_COLORS, USE_CURRENT_SET_COLORS, USE_CURRENT_SET_COLORS, 1000},
  {0, 0, 0, 0, 1000}
};

const int16_t CYCLE_CHRISTMAS[][CYCLE_COLUMNS] = {
  {255, 0, 0, 0, 1000},
  {0, 255, 0, 0, 1000}
};

const int16_t CYCLE_FOUR_OF_JULY[][CYCLE_COLUMNS] = {
  {255, 0, 0, 0, 1000},
  {0, 0, 0, 255, 1000},
  {0, 0, 255, 0, 1000}
};

typedef struct {
  uint8_t current_step = 0;
  uint8_t total_steps = 0;
  int8_t count; // -1 indefinetly
  int16_t **cycle;
} flash_cycle;

/*------------------------------------------------------------------------------------*/
/* Current parameter values. Set to default until we read from EEPROM                 */
/*------------------------------------------------------------------------------------*/
uint8_t gstate = STATE_OFF;
uint8_t gred = 0;
uint8_t ggreen = 0;
uint8_t gblue = 0;
uint8_t geffect = COLORFUL;
uint8_t gwhite = 0;
uint8_t gbrightness = 0;
bool gflash = false;
unsigned long flashStartTime = 0;
bool somethingChanged = false;
flash_cycle gflash_cycle;

/*------------------------------------------------------------------------------------*/
/* Global Variables                                                                   */
/*------------------------------------------------------------------------------------*/
// WiFi Manager
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

/*------------------------------------------------------------------------------------*/
/* WiFi Manager Global Functions                                                      */
/*------------------------------------------------------------------------------------*/
// WiFiManager Configuration CallBack
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("[WIFI]: Entered config mode");
  Serial.print("[WIFI]:"); Serial.println(WiFi.softAPIP());
  Serial.printf("[WIFI]: %s", (myWiFiManager->getConfigPortalSSID()).c_str());
}


/*------------------------------------------------------------------------------------*/
/* MQTT Global Functions                                                              */
/*------------------------------------------------------------------------------------*/
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(SECRET_MQTT_CLIENT_ID, SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
      Serial.println("connected");
      mqttClient.subscribe(CONFIG_MQTT_TOPIC_SET);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

uint8_t strEffectToInt(const char *effect) {
  Serial.printf("Effect (STR): %s\n", effect);
  uint8_t intEffect = COLORFUL;
  if (strcmp(effect, STR_COLORFUL) == 0) {
    intEffect = COLORFUL;
  } else if (strcmp(effect, STR_CROSS_FADE_SLOW) == 0) {
    intEffect = CROSS_FADE_SLOW;
  } else if (strcmp(effect, STR_CROSS_FADE_FAST) == 0) {
    intEffect = CROSS_FADE_FAST;
  } else if (strcmp(effect, STR_CHRISTMAS) == 0) {
    intEffect = CHRISTMAS;
  } else if (strcmp(effect, STR_FLASH) == 0) {
    intEffect = FLASH;
  } else if (strcmp(effect, STR_PURE_WHITE) == 0) {
    intEffect = PURE_WHITE;
  } else if (strcmp(effect, STR_FOUR_OF_JULY) == 0) {
    intEffect = FOUR_OF_JULY;
  }
  Serial.printf("Effect (INT) %d\n", intEffect);
  return intEffect;
}

bool isFlashingEffect(uint8_t effect) {
  if (effect >= MAX_EFFECTS) {
    return false;
  }
  return effectParams[effect].isFlashingEffect;
}

bool isPersistentEffect(uint8_t effect) {
  if (effect >= MAX_EFFECTS) {
    return false;
  }
  return effectParams[effect].isPersistentEffect;
}

bool isFadingEffect(uint8_t effect) {
  if (effect >= MAX_EFFECTS) {
    return false;
  }
  return effectParams[effect].isFadingEffect;
}

void saveToEEPROM() {
  Serial.println("Saving Data to EEPROM");
  uint8_t addr = 0;
  EEPROM.write(addr, 0x00); addr++;
  EEPROM.write(addr, gstate); addr++;
  EEPROM.write(addr, gred); addr++;
  EEPROM.write(addr, ggreen); addr++;
  EEPROM.write(addr, gblue); addr++;
  EEPROM.write(addr, gwhite); addr++;
  EEPROM.write(addr, gbrightness); addr++;
  EEPROM.write(addr, geffect); addr++;
  EEPROM.commit();
  Serial.println("Finished Saving Data to EEPROM");
}

void allocateCycle(const int16_t cycle[][CYCLE_COLUMNS], uint8_t rows) {
  Serial.printf("DEBUG: rows= %d\n", rows);
  gflash_cycle.cycle = new int16_t *[rows];
  for (uint8_t i = 0; i < rows; i++) {
    Serial.printf("DEBUG: row= %d", i);
    gflash_cycle.cycle[i] = new int16_t[CYCLE_COLUMNS];
    memcpy(gflash_cycle.cycle[i], cycle[i], sizeof(int16_t[CYCLE_COLUMNS]));
  }
}

void buildEffect(uint8_t effect) {
  uint16_t rows;
  switch(effect) {
    case CHRISTMAS:
      rows = sizeof(CYCLE_CHRISTMAS) / sizeof(CYCLE_CHRISTMAS[0]);
      allocateCycle(CYCLE_CHRISTMAS, rows);
      break;
    case FOUR_OF_JULY:
      rows = sizeof(CYCLE_FOUR_OF_JULY) / sizeof(CYCLE_FOUR_OF_JULY[0]);
      allocateCycle(CYCLE_FOUR_OF_JULY, rows);
      break;
    default:
      rows = sizeof(CYCLE_FLASH) / sizeof(CYCLE_FLASH[0]);
      allocateCycle(CYCLE_FLASH, rows);
      break;
  }
  gflash_cycle.total_steps = rows;
  gflash_cycle.count = effectParams[effect].falsh_count;
}

void deallocateCycle(uint8_t rows) {
  for (uint8_t i = 0; i < rows; i++) {
    delete gflash_cycle.cycle[i];
  }
  delete gflash_cycle.cycle;
}
// Supported MQTT Payload
// All fields are optional.
// {
//   "state": "ON" // or "OFF"
//   "color": {
//     "r": 255 // Red Color 0-255
//     "g": 255 // Green Color 0-255
//     "b": 255 // Blue Color 0-255
//   }
//   "brightness": 255 // 0-255
//   "white_value": 255 // White Color for RGBW LEDs 0-255
//   "effect": "colorful" // Light effect colorful|colorfade_slow|colorfade_fast|flash|christmas|pure_white
//   "flashes": 10 // Flashes count
// }
// (*) flash is not a persistent effect

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  StaticJsonDocument<500> doc;
  deserializeJson(doc, payload);

  const char *state = doc["state"];
  const uint8_t red = (uint8_t) (doc["color"]["r"]);
  const uint8_t green = (uint8_t) (doc["color"]["g"]);
  const uint8_t blue = (uint8_t) (doc["color"]["b"]);
  const uint8_t brightness = (uint8_t) doc["brightness"];
  const char *effect = doc["effect"];
  const uint8_t white = (uint8_t) doc["white_value"];
  bool persist_change = true;
  // Check State (ON-OFF)
  if (state != nullptr) {
    Serial.printf("State = %s\n", state);
    gstate = strcmp(state, STR_STATE_ON) == 0 ? STATE_ON : STATE_OFF;
  }

  // Check Effect
  if (effect != nullptr) {
    Serial.printf("Effect = %s\n", effect);
    uint8_t tEffect = strEffectToInt(effect);
    geffect = tEffect;

    if (!isPersistentEffect(tEffect)) {
      persist_change = false;
    }

    if (gflash == true) {
      deallocateCycle(gflash_cycle.total_steps);
      gflash = false;
    }

    if (isFlashingEffect(tEffect)) {
      Serial.println("Start Flashing...");
      gflash = true;
      buildEffect(tEffect);
      flashStartTime = millis();
    } else {
      if (tEffect == PURE_WHITE) {
        gred = 0;
        ggreen = 0;
        gblue = 0;
      }
    }
  }
  if (doc.containsKey("color") && geffect != PURE_WHITE) {
    Serial.printf("Red = %d, Green = %d, Blue = %d\n", red, green, blue);
    gred = red;
    ggreen = green;
    gblue = blue;
  }
  if (doc.containsKey("brightness")) {
    Serial.printf("Brightness = %d\n", brightness);
    gbrightness = brightness;
  }
  if (doc.containsKey("white_value")) {
    Serial.printf("White = %d\n", white);
    gwhite = white;
  }

  // TODO: Report the actual state, not just back what we received.
  char buffer[measureJson(doc) + 1];
  serializeJson(doc, buffer, measureJson(doc) + 1);

  mqttClient.publish(CONFIG_MQTT_TOPIC_STATE, buffer, true);

  somethingChanged = true;
  if (persist_change) {
    saveToEEPROM();
  }
}

void displayGlobalParams() {
  Serial.printf("State = %d\n", gstate);
  Serial.printf("Red = %d, Green = %d, Blue = %d\n", gred, ggreen, gblue);
  Serial.printf("White = %d\n", gwhite);
  Serial.printf("Brightness = %d\n", gbrightness);
  Serial.printf("Effect = %d\n", geffect);
}

void restoreFromEEPROM() {
  Serial.println("Restoring Data from EEPROM");
  uint8_t addr = 0;
  byte value = EEPROM.read(addr); addr++;
  if (value == 0x00) {
    Serial.println("Valid data on EEPROM. Restore global parameters");
    // Valid data on EEPROM
    gstate = EEPROM.read(addr); addr++;
    gred = EEPROM.read(addr); addr++;
    ggreen = EEPROM.read(addr); addr++;
    gblue = EEPROM.read(addr); addr++;
    gwhite = EEPROM.read(addr); addr++;
    gbrightness = EEPROM.read(addr); addr++;
    geffect = EEPROM.read(addr); addr++;
  }
  displayGlobalParams();
}

void turnLightOff() {
  analogWrite(GPIO_RED, 0);
  analogWrite(GPIO_GREEN, 0);
  analogWrite(GPIO_BLUE, 0);
  analogWrite(GPIO_WHITE, 0);
}

void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint8_t brightness) {
  if (gstate == STATE_ON) {
    Serial.printf("Setting colors to r: %d, g: %d, b: %d, w: %d\n", red, green, blue, white);
    Serial.printf("Setting brigthness to %d\n", brightness);
    analogWrite(GPIO_RED, map(red, 0, 255, 0, brightness));
    analogWrite(GPIO_GREEN, map(green, 0, 255, 0, brightness));
    analogWrite(GPIO_BLUE, map(blue, 0, 255, 0, brightness));
    analogWrite(GPIO_WHITE, map(white, 0, 255, 0, brightness));
  } else {
    turnLightOff();
  }
}

void setLightColor() {
  setLightColor(gred, ggreen, gblue, gwhite, gbrightness);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  pinMode(GPIO_RED, OUTPUT);
  pinMode(GPIO_GREEN, OUTPUT);
  pinMode(GPIO_BLUE, OUTPUT);
  pinMode(GPIO_WHITE, OUTPUT);

  turnLightOff();

  // Instantiate and setup WiFiManager
  // wifiManager.resetSettings(); Uncomment to reset wifi settings
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect(ACCESS_POINT_NAME, ACCESS_POINT_PASS)) {
    Serial.println("Failed to connect and hit timeout");
    ESP.reset();
    delay(1000);  
  }

  // Config time
  setenv("TZ", "EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00", 1);
  configTime(0, 0, "pool.ntp.org");

  // Initialize OTA (Over the air) update
  ArduinoOTA.setHostname(ACCESS_POINT_NAME);
  ArduinoOTA.setPassword(ACCESS_POINT_PASS);

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA]: Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA]: End");
  });
  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total) {
    Serial.printf("[OTA]: Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA]: Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("[OTA]: Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("[OTA]: Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA]: Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA]: Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("[OTA]: End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("[OTA]: Ready");

  mqttClient.setServer(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  restoreFromEEPROM();

  analogWriteRange(255);

  setLightColor();
}

void loop() {
  // OTA
  ArduinoOTA.handle();

  // MQTT
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  // Apply color changes or produce flashing effects
  if (gflash == true) {
    if (gflash_cycle.count == 0) {
      deallocateCycle(gflash_cycle.total_steps);
      gflash =false;
      restoreFromEEPROM();
      somethingChanged = true;
      Serial.println("Stop flashing...");
    } else {
      if ((int16_t)(millis() - flashStartTime) >= gflash_cycle.cycle[gflash_cycle.current_step][4]) {
        int16_t red = gflash_cycle.cycle[gflash_cycle.current_step][0] == USE_CURRENT_SET_COLORS ? gred : gflash_cycle.cycle[gflash_cycle.current_step][0];
        int16_t green = gflash_cycle.cycle[gflash_cycle.current_step][1] == USE_CURRENT_SET_COLORS ? ggreen : gflash_cycle.cycle[gflash_cycle.current_step][1];
        int16_t blue = gflash_cycle.cycle[gflash_cycle.current_step][2] == USE_CURRENT_SET_COLORS ? gblue : gflash_cycle.cycle[gflash_cycle.current_step][2];
        int16_t white = gflash_cycle.cycle[gflash_cycle.current_step][3] == USE_CURRENT_SET_COLORS ? gwhite : gflash_cycle.cycle[gflash_cycle.current_step][3];
        setLightColor(red, green, blue, white, gbrightness);
        Serial.printf("DEBUG: current step= %d, total_steps= %d\n", gflash_cycle.current_step, gflash_cycle.total_steps);
        if (++gflash_cycle.current_step == gflash_cycle.total_steps) {
          gflash_cycle.current_step = 0;
        }
        flashStartTime = millis();
        if (gflash_cycle.count != FLASH_FOREVER) {
          gflash_cycle.count--;
        }
      }
    }
  } else if (somethingChanged == true) {
    setLightColor();
    somethingChanged = false;
  }
}