#include "CommandInterpreter.h"

CommandInterpreter::CommandInterpreter(Config *config, Floower *floower) 
        : config(config), floower(floower) {}

uint16_t CommandInterpreter::run(const uint16_t type, const char *payload, const uint16_t payloadLength, char *responsePayload, uint16_t *responseLength) {
    // commands that require request payload
    if (payloadLength > 0) {
        payloadUnpacker.feed((const uint8_t *) payload, payloadLength);
        if (!payloadUnpacker.deserialize(jsonPayload)) {
            ESP_LOGE(LOG_TAG, "Invalid Payload");
            return STATUS_ERROR;
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
                return STATUS_OK;
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
                return STATUS_OK;
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
                return STATUS_OK;
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
                return STATUS_OK;
            }
        }
    }
    
    // commands without request payload
    switch (type) {
         case MessageType::CMD_READ_STATE: {
            // response: { r: <red>, g: <green>, b: <blue>, l: <level >}
            jsonPayload.clear();
            RgbColor color = RgbColor(floower->getColor());
            jsonPayload["r"] = color.R;
            jsonPayload["g"] = color.G;
            jsonPayload["b"] = color.B;
            jsonPayload["l"] = floower->getPetalsOpenLevel();
            *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
            Serial.println(*responseLength);
            return STATUS_OK;
        }
    }

    return STATUS_UNSUPPORTED;
}