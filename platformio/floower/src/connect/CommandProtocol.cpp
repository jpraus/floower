#include "CommandProtocol.h"

CommandProtocol::CommandProtocol(Config *config, Floower *floower) 
        : config(config), floower(floower) {}

void CommandProtocol::onControlCommand(ControlCommandCallback callback) {
    controlCommandCallback = callback;
}

uint16_t CommandProtocol::run(const uint16_t type, const char *payload, const uint16_t payloadLength, char *responsePayload, uint16_t *responseLength) {
    // commands that require request payload
    if (payloadLength > 0) {
        payloadUnpacker.feed((const uint8_t *) payload, payloadLength);
        if (!payloadUnpacker.deserialize(jsonPayload)) {
            ESP_LOGE(LOG_TAG, "Invalid Payload");
            return STATUS_ERROR;
        }
        switch (type) {
            case CommandType::CMD_WRITE_PETALS: {
                // { l: <level>, t: <time> }
                if (jsonPayload.containsKey("l")) {
                    uint8_t level = jsonPayload["l"];
                    uint16_t time = config->speedMillis;
                    if (jsonPayload.containsKey("t")) {
                        time = jsonPayload["t"];
                    }
                    floower->setPetalsOpenLevel(level, time);
                    fireControlCommandCallback();
                }
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_RGB_COLOR: {
                // { r: <red>, g: <green>, b: <blue>, t: <time> }
                HsbColor color = HsbColor(RgbColor(
                    jsonPayload["r"], 
                    jsonPayload["g"], 
                    jsonPayload["b"]
                ));
                uint16_t time = config->speedMillis;
                if (jsonPayload.containsKey("t")) {
                    time = jsonPayload["t"];
                }
                floower->transitionColor(color.H, color.S, color.B, time);
                fireControlCommandCallback();
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_STATE: {
                // { r: <red>, g: <green>, b: <blue>, l: <petalsLevel>, t: <time> }
                uint16_t time = config->speedMillis;
                if (jsonPayload.containsKey("t")) {
                    time = jsonPayload["t"];
                }
                if (jsonPayload.containsKey("l")) {
                    uint8_t level = jsonPayload["l"];
                    floower->setPetalsOpenLevel(level, time);
                }
                if (jsonPayload.containsKey("r") || jsonPayload.containsKey("g") || jsonPayload.containsKey("b")) {
                    HsbColor color = HsbColor(RgbColor(
                        jsonPayload["r"], 
                        jsonPayload["g"], 
                        jsonPayload["b"]
                    ));
                    floower->transitionColor(color.H, color.S, color.B, time);
                }
                fireControlCommandCallback();
                return STATUS_OK;
            }
            case CommandType::CMD_PLAY_ANIMATION: {
                // { a: <animationCode> }
                uint8_t animation = jsonPayload["a"];
                if (animation > 0) {
                    floower->startAnimation(animation);
                    fireControlCommandCallback();
                }
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_WIFI: {
                // { ssid: <wifiSsid>, pwd: <wifiPwd>, dvc: <floudDeviceId>, tkn: <floudToken> }
                if (jsonPayload.containsKey("ssid")) {
                    config->setWifi(jsonPayload["ssid"], jsonPayload["pwd"]);
                }
                if (jsonPayload.containsKey("dvc") && jsonPayload.containsKey("tkn")) {
                    config->setFloud(jsonPayload["dvc"], jsonPayload["tkn"]);
                }
                config->commit();
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_NAME: {
                // { n: <string> }
                String name = jsonPayload["n"];
                if (!name.isEmpty()) {
                    config->setName(name);
                }
                config->commit();
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_CUSTOMIZATION: {
                // { spd: <transitionSpeedInTenthsOfSeconds>, brg: <colorBrightness>, mol: <maxOpenLevel> }
                if (jsonPayload.containsKey("spd")) {
                    config->setSpeed(jsonPayload["spd"]);
                }
                if (jsonPayload.containsKey("brg")) {
                    config->setColorBrightness(jsonPayload["brg"]);
                }
                if (jsonPayload.containsKey("mol")) {
                    config->setMaxOpenLevel(jsonPayload["mol"]);
                }
                config->commit();
                return STATUS_OK;
            }
            case CommandType::CMD_WRITE_COLOR_SCHEME: {
                // [ <encoded HS color values as single 2 byte number>, ... ]
                config->commit();
                return STATUS_UNSUPPORTED;
            }
            case CommandType::CMD_RUN_OTA: {
                // TODO: notify wifi to run OTA
                return STATUS_OK;
            }
        }
    }
    
    // command to retrive data
    if (responsePayload != nullptr && responseLength != nullptr) {
        switch (type) {
            case CommandType::CMD_READ_STATE: {
                // response: { r: <red>, g: <green>, b: <blue>, l: <level >}
                jsonPayload.clear();
                RgbColor color = RgbColor(floower->getColor());
                jsonPayload["r"] = color.R;
                jsonPayload["g"] = color.G;
                jsonPayload["b"] = color.B;
                jsonPayload["l"] = floower->getPetalsOpenLevel();
                *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
                return STATUS_OK;
            }
            case CommandType::CMD_READ_WIFI: {
                // response: { ssid: <wifiSsid>, tkn: <floudToken>, s: <state>}
                jsonPayload.clear();
                jsonPayload["ssid"] = config->wifiSsid;
                jsonPayload["tkn"] = config->floudToken;
                //jsonPayload["s"] = color.B; TODO: how to get state of WiFi
                *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
                return STATUS_OK;
            }
            case CommandType::CMD_READ_CUSTOMIZATION: {
                // response: { spd: <transitionSpeedInTenthsOfSeconds>, brg: <colorBrightness>, mol: <maxOpenLevel>}
                jsonPayload.clear();
                jsonPayload["spd"] = config->speed;
                jsonPayload["brg"] = config->colorBrightness;
                jsonPayload["mol"] = config->maxOpenLevel;
                *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
                return STATUS_OK;
            }
            case CommandType::CMD_READ_COLOR_SCHEME: {
                // response: [ <encoded HS color values as single 2 byte number>, ... ]
                jsonPayload.clear();
                JsonArray array = jsonPayload.to<JsonArray>();
                for (uint8_t i = 0; i < config->colorSchemeSize; i++) {
                    uint16_t valueHS = Config::encodeHSColor(config->colorScheme[i].H, config->colorScheme[i].S);
                    array.add(valueHS);
                }
                *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
                return STATUS_OK;
            }
            case CommandType::CMD_READ_DEVICE_INFO: {
                // response: { n: <name>, m: <modelName>, fw: <firmwareVersion>, hw: <hardwareRevision>, sn: <serialNumber> }
                jsonPayload.clear();
                jsonPayload["n"] = config->name;
                jsonPayload["m"] = config->modelName;
                jsonPayload["fw"] = config->firmwareVersion;
                jsonPayload["hw"] = config->hardwareRevision;
                jsonPayload["sn"] = config->serialNumber;
                *responseLength = serializeMsgPack(jsonPayload, responsePayload, MAX_MESSAGE_PAYLOAD_BYTES);
                return STATUS_OK;
            }
        }
    }

    return STATUS_UNSUPPORTED;
}

uint16_t CommandProtocol::sendStatus(const uint8_t batteryLevel, const bool charging, char *payload, uint16_t *payloadLength) {
    // payload: { b: <batteryLevel>, bc: <batteryCharging> }
    jsonPayload.clear();
    jsonPayload["b"] = batteryLevel;
    jsonPayload["c"] = charging;
    *payloadLength = serializeMsgPack(jsonPayload, payload, MAX_MESSAGE_PAYLOAD_BYTES);
    return PROTOCOL_STATUS;
}

inline void CommandProtocol::fireControlCommandCallback() {
    if (controlCommandCallback != nullptr) {
        controlCommandCallback();
    }
}