#include "WifiConnect.h"
#include <Update.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "WifiConnect";
#endif

#define OTA_RESPONSE_TIMEOUT_MS 5000
#define SOCKET_RESPONSE_TIMEOUT_MS 2000

#define STATE_FLOUD_DISCONNECTED 0
#define STATE_FLOUD_CONNECTING 1
#define STATE_FLOUD_ESTABLISHED 2
#define STATE_FLOUD_AUTHORIZING 3
#define STATE_FLOUD_AUTHORIZED 4
#define STATE_FLOUD_UNAUTHORIZED 5

#define RECONNECT_INTERVAL_MS 5000

#define FLOUD_HOST "192.168.0.103"
#define FLOUD_PORT 3000

WifiConnect::WifiConnect(Config *config, Floower *floower) 
        : config(config), floower(floower) {
    state = STATE_FLOUD_DISCONNECTED;
    client = NULL;
}

void WifiConnect::setup() {
}

void WifiConnect::start() {
    if (config->wifiSsid.isEmpty()) {
        return;
    }

    WiFi.mode(WIFI_STA);

    WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiConnected(event, info); }, SYSTEM_EVENT_STA_CONNECTED);
    WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiGotIp(event, info); }, SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiDisconnected(event, info); }, SYSTEM_EVENT_STA_DISCONNECTED);

    WiFi.begin(config->wifiSsid.c_str(), config->wifiPassword.c_str());
    ESP_LOGI(LOG_TAG, "Starting WiFi: %s", config->wifiSsid);
    enabled = true;
}

void WifiConnect::stop() {
    // TODO: disconnect the socket
    WiFi.disconnect(true); // turn off wifi
    WiFi.removeEvent(SYSTEM_EVENT_STA_CONNECTED);
    WiFi.removeEvent(SYSTEM_EVENT_STA_GOT_IP);
    WiFi.removeEvent(SYSTEM_EVENT_STA_DISCONNECTED);
    esp_wifi_stop();

    ESP_LOGI(LOG_TAG, "Wifi stopped");
    enabled = false;
}

void WifiConnect::loop() {
    if (!enabled) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (config->floudToken) {
            switch (state) {
                case STATE_FLOUD_DISCONNECTED:
                    if (reconnectTime <= millis()) {
                        ESP_LOGI(LOG_TAG, "Connecting to Floud");
                        if (client == NULL) {
                            client = new AsyncClient(NULL);
                            client->onData([=](void* arg, AsyncClient* client, void *data, size_t len){ onSocketData((char *)data, len); });
                            client->onConnect([=](void* arg, AsyncClient* client){ onSocketConnected(); });
                            client->onDisconnect([=](void* arg, AsyncClient* client){ onSocketDisconnected(); });
                        }
                        client->connect(FLOUD_HOST, FLOUD_PORT);
                        reconnectTime = millis() + RECONNECT_INTERVAL_MS;
                        state = STATE_FLOUD_CONNECTING;
                    }
                    break;
                case STATE_FLOUD_ESTABLISHED:
                    sendAuthorization();
                    state = STATE_FLOUD_AUTHORIZING;
                    break;
            }
        }
    }

    if (received) {
        // got message to process   
        handleReceivedMessage();
        received = false;
    }
    else if (receiveTime > 0 && receiveTime <= millis()) {
        // did not received resposne
        ESP_LOGW(LOG_TAG, "Floud response timeout");
        socketReconnect();
    }
}

void WifiConnect::handleReceivedMessage() {
    ESP_LOGI(LOG_TAG, "Got message: %d/%d/%d", receivedMessage.type, receivedMessage.id, receivedMessage.length);
    // check for response codes
    if (receivedMessage.type == MessageType::STATUS_ERROR) {
        socketReconnect(); // reset on error
    }
    else if (receivedMessage.type == MessageType::STATUS_OK) {
        if (state == STATE_FLOUD_AUTHORIZING && receivedMessage.id == authorizationMessageId) {
            ESP_LOGI(LOG_TAG, "Authorized");
            state = STATE_FLOUD_AUTHORIZED;
        }
    }
    else if (receivedMessage.type == MessageType::STATUS_UNAUTHORIZED) {
        state = STATE_FLOUD_UNAUTHORIZED; // TODO: force authorization again
    }
    // handle commands
    else if (state == STATE_FLOUD_AUTHORIZED) {
        // TODO interpret commands
        if (receivedMessage.length > 0) {
            payloadUnpacker.feed((const uint8_t *) receiveBuffer, receivedMessage.length);
            if (!payloadUnpacker.deserialize(jsonPayload)) {
                ESP_LOGE(LOG_TAG, "Invalid Payload");
                return;
            }
            if (receivedMessage.type == MessageType::CMD_WRITE_COLOR) {
                // { r: <red>, g: <green>, b: <blue>, t: <time >}
                HsbColor color = HsbColor(RgbColor(jsonPayload["r"], jsonPayload["g"], jsonPayload["b"]));
                //uint16_t time = jsonPayload["t"];
                floower->transitionColor(color.H, color.S, color.B, 5000); // TODO: time
            }
            else if (receivedMessage.type == MessageType::CMD_WRITE_PETALS) {
                // { l: <level>, t: <time >}
                uint8_t level = jsonPayload["l"];
                //uint16_t time = jsonPayload["t"];
                floower->setPetalsOpenLevel(level, 5000);
            }
            else if (receivedMessage.type == MessageType::PROTOCOL_WRITE_WIFI) {
                Serial.println("Setup WiFi");
                String ssid = jsonPayload["ssid"];
                String ssid = jsonPayload["pwd"];
            }
        }
    }
}

void WifiConnect::socketReconnect() {
    receiveTime = 0;
    received = false;
    if (client != NULL) {
        client->stop();
    }
}

void WifiConnect::sendAuthorization() {
    ESP_LOGI(LOG_TAG, "Authorizing to Floud");
    String &token = config->floudToken;
    token.toCharArray(sendBuffer, token.length() + 1); // add +1 to accomodate for 0 terminate char
    authorizationMessageId = sendMessage(MessageType::PROTOCOL_AUTH, sendBuffer, token.length()); // dont send the 0 terminate char
}

uint16_t WifiConnect::sendMessage(const uint16_t type, const char* payload, const size_t payloadSize) {
    uint16_t messageId = messageIdCounter++;
    WifiMessageHeader header = {
        htons(type), htons(messageId), htons(payloadSize)
    };

    // TODO: check payload size?
    client->write((char*) &header, sizeof(header));
    client->write(payload, payloadSize);

    receiveTime = millis() + SOCKET_RESPONSE_TIMEOUT_MS;
    return messageId;
}

void WifiConnect::receiveMessage(char *data, size_t len) {
    if (received) {
        return; // cannot receive another message yet
    }

    size_t headerSize = sizeof(receivedMessage);
    if (len < headerSize) {
        return; // cannot receive
    }
    
    memcpy(&receivedMessage, data, headerSize);
    receivedMessage.type = ntohs(receivedMessage.type);
    receivedMessage.id = ntohs(receivedMessage.id);
    receivedMessage.length = ntohs(receivedMessage.length);

    if (receivedMessage.length > 0) {
        size_t available = len - headerSize;
        if (available > MAX_MESSAGE_PAYLOAD_BYTES) {
            // invalid package, reset
            socketReconnect();
        }
        else if (available >= receivedMessage.length) {
            // receive payload
            memcpy(receiveBuffer, data + headerSize, receivedMessage.length);
            received = true;
            receiveTime = 0;
        }
        // else invalid packet, payload incomplete or missing
    }
    else {
        received = true;
        receiveTime = 0;
    }
}

void WifiConnect::onSocketData(char *data, size_t len) {
    receiveMessage(data, len);
}

void WifiConnect::onSocketConnected() {
    ESP_LOGI(LOG_TAG, "Connected to Floud");
    state = STATE_FLOUD_ESTABLISHED;
}

void WifiConnect::onSocketDisconnected() {
    ESP_LOGI(LOG_TAG, "Disconnected from Floud");
    state = STATE_FLOUD_DISCONNECTED;
    reconnectTime = millis() + RECONNECT_INTERVAL_MS;
}

// WIFI

void WifiConnect::onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_LOGI(LOG_TAG, "Wifi lost: %d", info.disconnected.reason);
}

void WifiConnect::onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_LOGI(LOG_TAG, "Wifi connected");
}

void WifiConnect::onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_LOGI(LOG_TAG, "Wifi got IP");
}

// OTA

inline String getHeaderValue(String header, String headerName) {
    return header.substring(strlen(headerName.c_str()));
}

void WifiConnect::runOTAUpdate() {
    WiFiClient client;

    if (!client.connect("upgrade.floower.io", 80)) {
        Serial.println("Cannot connect to upgrade.floower.io");
        return;
    }

    client.print("GET /flooware.ino.bin HTTP/1.1\r\n");
    client.print("Host: upgrade.floower.io\r\n");
    client.print("Cache-Control: no-cache\r\n");
    client.print("Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > OTA_RESPONSE_TIMEOUT_MS) {
            Serial.println("OTA Timeout");
            client.stop();
            return;
        }
    }

    unsigned int contentLength = 0;
    bool isValidContentType = false;

    while (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim(); // Check if the line is end of headers by removing space symbol

        if (!line.length()) {
            break; // if the the line is empty, this is the end of the headers
        }

        // Check allowed HTTP responses
        if (line.startsWith("HTTP/1.1")) {
            if (line.indexOf("200") == -1) {
                Serial.println("Got " + line + " response from server");
                client.stop();
                return;
            }
        }

        // Checking headers
        if (line.startsWith("Content-Length: ")) {
            contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
            Serial.println("Got " + String(contentLength) + " bytes from server");
        }

        if (line.startsWith("Content-Type: ")) {
            String contentType = getHeaderValue(line, "Content-Type: ");
            Serial.println("Got " + contentType + " payload.");
            if (contentType == "application/octet-stream") {
                isValidContentType = true;
            }
        }
    }

    // check whether we have everything for OTA update
    if (contentLength && isValidContentType) {
        if (Update.begin(contentLength)) {
            Serial.println("Starting Over-The-Air update. This may take some time to complete ...");

            esp_task_wdt_delete(nullptr);
            esp_task_wdt_deinit();

            size_t written = Update.writeStream(client);

            if (written == contentLength) {
                Serial.println("Written : " + String(written) + " successfully");
            }
            else {
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
                // Retry??
            }

            if (Update.end()) {
                if (Update.isFinished()) {
                    Serial.println("OTA update has successfully completed. Rebooting ...");
                    ESP.restart();
                }
                else {
                    Serial.println("Something went wrong! OTA update hasn't been finished properly.");
                }
            }
            else {
                Serial.println("An error Occurred. Error #: " + String(Update.getError()));
            }
        }
        else {
            Serial.println("There isn't enough space to start OTA update");
            client.flush();
        }
    }
    else {
        Serial.println("There was no valid content in the response from the OTA server!");
        client.flush();
    }
}