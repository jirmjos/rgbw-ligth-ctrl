#ifndef CONFIG_H
#define CONFIG_H

const char *CONFIG_MQTT_TOPIC_SET = "/home-assistant/ESP_LED_KITCHEN/set";
const char *CONFIG_MQTT_TOPIC_STATE = "/home-assistant/ESP_LED_KITCHEN";
const char *CONFIG_MQTT_HOST = "hassio.local";
const uint16_t CONFIG_MQTT_PORT = 1883;
#endif // CONFIG_H