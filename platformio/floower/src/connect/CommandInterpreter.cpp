#include "CommandInterpreter.h"

CommandInterpreter::CommandInterpreter(Config *config, Floower *floower) 
        : config(config), floower(floower) {}

void CommandInterpreter::run(const uint16_t type, const char *payload, const uint16_t payloadLength, const uint16_t *responseType, const char *responsePayload, const uint16_t *responseLength) {
    // commands that require content
    if (payloadLength > 0) {
        payloadUnpacker.feed((const uint8_t *) payload, payloadLength);
        if (!payloadUnpacker.deserialize(jsonPayload)) {
            ESP_LOGE(LOG_TAG, "Invalid Payload");
            return;
        }
        switch (type) {
            case MessageType::PROTOCOL_WRITE_WIFI: {
                Serial.println("Setup WiFi");
                if (jsonPayload.containsKey("ssid")) {
                    String ssid = jsonPayload["ssid"];
                    String pwd = jsonPayload["pwd"];
                    Serial.println(ssid);
                    Serial.println(pwd);

                    config->setWifi(jsonPayload["ssid"], jsonPayload["pwd"]);
                    if (jsonPayload.containsKey("token")) {
                        config->setFloud(jsonPayload["token"]);
                    }
                    config->commit();
                    // TODO: how to notify the Wifi Connect?
                }
                break;
            }
            case MessageType::PROTOCOL_WRITE_TOKEN: {
                Serial.println("Setup Floud");
                if (jsonPayload.containsKey("token")) {
                    String token = jsonPayload["token"];
                    Serial.println(token);

                    config->setFloud(jsonPayload["token"]);
                    config->commit();
                    // TODO: how to notify the Wifi Connect?
                }
                break;
            }
            case MessageType::CMD_WRITE_RGB_COLOR: {
                // { r: <red>, g: <green>, b: <blue>, t: <time >}
                HsbColor color = HsbColor(RgbColor(
                    jsonPayload["r"], 
                    jsonPayload["g"], 
                    jsonPayload["b"]
                ));
                uint16_t time = config->speedMillis;
                if (jsonPayload.containsKey("t")) {
                    time = jsonPayload["t"];
                }
                floower->transitionColor(color.H, color.S, color.B, time); // TODO: time
                if (remoteControlCallback != nullptr) {
                    remoteControlCallback();
                }
                break;
            }
            case MessageType::CMD_WRITE_PETALS: {
                // { l: <level>, t: <time >}
                if (jsonPayload.containsKey("l")) {
                    uint8_t level = jsonPayload["l"];
                    uint16_t time = config->speedMillis;
                    if (jsonPayload.containsKey("t")) {
                        time = jsonPayload["t"];
                    }
                    floower->setPetalsOpenLevel(level, time);
                    if (remoteControlCallback != nullptr) {
                        remoteControlCallback();
                    }
                }
                break;
            }
        }
    }
}