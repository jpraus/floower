#include "CommandProtocol.h"

CommandProtocol::CommandProtocol(Config *config, Floower *floower) 
        : config(config), floower(floower) {}

void CommandProtocol::onControlCommand(ControlCommandCallback callback) {
    controlCommandCallback = callback;
}

void CommandProtocol::onRunOTA(RunOTACallback callback) {
    runOTACallback = callback;
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
                uint8_t level = -1;
                HsbColor color;
                bool transitionColor = false;

                if (jsonPayload.containsKey("t")) {
                    time = jsonPayload["t"];
                }
                if (jsonPayload.containsKey("l")) {
                    level = jsonPayload["l"];
                }
                if (jsonPayload.containsKey("r") || jsonPayload.containsKey("g") || jsonPayload.containsKey("b")) {
                    color = HsbColor(RgbColor(
                        jsonPayload["r"], 
                        jsonPayload["g"], 
                        jsonPayload["b"]
                    ));
                    transitionColor = true;
                }
                if (level != -1) {
                    floower->setPetalsOpenLevel(level, time);
                }
                if (transitionColor) {
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
                JsonArray array = jsonPayload.as<JsonArray>();
                size_t size = floor(jsonPayload.size());
                if (size > 0 && size <= COLOR_SCHEME_MAX_LENGTH) {
                    HsbColor colors[array.size()];
                    ESP_LOGI(LOG_TAG, "New color scheme: %d", array.size());
                    for (uint8_t i = 0; i < array.size(); i++) {\
                        uint16_t hsValue = array.getElement(i).as<int>();
                        colors[i] = Config::decodeHSColor(hsValue);
                        ESP_LOGI(LOG_TAG, "Color %d: %.2f,%.2f", i, colors[i].H, colors[i].S);
                    }
                    config->setColorScheme(colors, size);
                    config->commit();
                    return STATUS_OK;
                }
                return STATUS_ERROR;
            }
            case CommandType::CMD_RUN_OTA: {
                // { u: <firmwareUrl> }
                if (jsonPayload.containsKey("u") && runOTACallback != nullptr) {
                    runOTACallback(jsonPayload["u"]);
                    return STATUS_OK;
                }
                return STATUS_ERROR;
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
                    array.add(Config::encodeHSColor(config->colorScheme[i].H, config->colorScheme[i].S));
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

uint16_t CommandProtocol::sendState(const int8_t petalsOpenLevel, const HsbColor hsbColor, char *payload, uint16_t *payloadLength) {
    // payload: { r: <red>, g: <green>, b: <blue>, l: <level >}
    jsonPayload.clear();
    RgbColor color = RgbColor(hsbColor);
    jsonPayload["r"] = color.R;
    jsonPayload["g"] = color.G;
    jsonPayload["b"] = color.B;
    jsonPayload["l"] = petalsOpenLevel;
    *payloadLength = serializeMsgPack(jsonPayload, payload, MAX_MESSAGE_PAYLOAD_BYTES);
    return CMD_WRITE_STATE;
}

inline void CommandProtocol::fireControlCommandCallback() {
    if (controlCommandCallback != nullptr) {
        controlCommandCallback();
    }
}