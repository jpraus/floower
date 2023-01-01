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

//#define FLOUD_HOST "192.168.0.101"
#define FLOUD_HOST "connect.floud.cz"
#define FLOUD_PORT 3000

// floud connector state
#define STATE_FLOUD_DISCONNECTED 0
#define STATE_FLOUD_CONNECTING 1
#define STATE_FLOUD_ESTABLISHED 2
#define STATE_FLOUD_AUTHORIZING 3
#define STATE_FLOUD_AUTHORIZED 4

// ota state
#define STATE_OTA_UPDATE_PREPARING 10
#define STATE_OTA_UPDATE_READY 11
#define STATE_OTA_UPDATE_CONNECTING 12
#define STATE_OTA_UPDATE_CONNECTED 13
#define STATE_OTA_UPDATE_HEADER 14
#define STATE_OTA_UPDATE_DATA 15

// running mode
#define MODE_FLOUD 0
#define MODE_OTA_UPDATE 1

#define OTA_UPDATE_RESPONSE_TIMEOUT_MS 10000
#define SOCKET_RESPONSE_TIMEOUT_MS 2000

#define RECONNECT_INTERVAL_MS 3000
#define CONNECT_RETRY_INTERVAL_MS 30000

WifiConnect::WifiConnect(Config *config, CommandProtocol *cmdProtocol) 
        : config(config), cmdProtocol(cmdProtocol) {
    mode = MODE_FLOUD;
    state = STATE_FLOUD_DISCONNECTED;
    client = NULL;
}

void WifiConnect::setup() {
}

void WifiConnect::enable() {
    if (!config->wifiSsid.isEmpty()) {
        WiFi.mode(WIFI_STA);

        wifiConnectedEventId = WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiConnected(event, info); }, ARDUINO_EVENT_WIFI_STA_CONNECTED);
        wifiGotIpEventId = WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiGotIp(event, info); }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
        wifiDisconnectedEventId = WiFi.onEvent([=](WiFiEvent_t event, WiFiEventInfo_t info){ onWifiDisconnected(event, info); }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

        WiFi.begin(config->wifiSsid.c_str(), config->wifiPassword.c_str());
        ESP_LOGI(LOG_TAG, "WiFi on: %s", config->wifiSsid.c_str());
        wifiOn = true;
        wifiFailed = false;
    }
    enabled = true;
}

void WifiConnect::disable() {
    if (!enabled) {
        return;
    }

    // TODO: disconnect the socket
    WiFi.disconnect(true); // turn off wifi
    WiFi.removeEvent(wifiConnectedEventId);
    WiFi.removeEvent(wifiGotIpEventId);
    WiFi.removeEvent(wifiDisconnectedEventId);
    esp_wifi_stop();

    ESP_LOGI(LOG_TAG, "Wifi off");
    wifiOn = false;
    enabled = false;
}

bool WifiConnect::isEnabled() {
    return enabled;
}

bool WifiConnect::isConnected() {
    return state == STATE_FLOUD_AUTHORIZED;
}

void WifiConnect::reconnect() {
    if (enabled) {
        if (wifiOn) {
            if (!config->wifiSsid.isEmpty()) {
                wifiFailed = false;
                WiFi.begin(config->wifiSsid.c_str(), config->wifiPassword.c_str());
                ESP_LOGI(LOG_TAG, "WiFi reconnecting: %s", config->wifiSsid);
            }
            else {
                disable();
            }
        }
        else {
            enable();
        }
    }
}

void WifiConnect::updateStatusData(uint8_t batteryLevel, bool batteryCharging) {
    if (enabled && state == STATE_FLOUD_AUTHORIZED) {
        uint16_t payloadSize = 0;
        uint16_t type = cmdProtocol->sendStatus(batteryLevel, batteryCharging, sendBuffer, &payloadSize);
        sendRequest(type, receivedMessage.id, sendBuffer, payloadSize);
    }
}

void WifiConnect::updateFloowerState(int8_t petalsOpenLevel, HsbColor hsbColor) {
    if (enabled && state == STATE_FLOUD_AUTHORIZED) {
        uint16_t payloadSize = 0;
        uint16_t type = cmdProtocol->sendState(petalsOpenLevel, hsbColor, sendBuffer, &payloadSize);
        sendRequest(type, receivedMessage.id, sendBuffer, payloadSize);
    }
}

uint8_t WifiConnect::getStatus() {
    if (config->wifiSsid.isEmpty()) {
        return WIFI_STATUS_NOT_CONFIGURED;
    }
    else if (!enabled) {
        return WIFI_STATUS_DISABLED;
    }
    else if (state == STATE_FLOUD_AUTHORIZED) {
        return WIFI_STATUS_FLOUD_CONNECTED;
    }
    else if (authorizationFailed) {
        return WIFI_STATUS_FLOUD_UNAUTHORIZED;
    }
    else if (wifiFailed) {
        return WIFI_STATUS_FAILED;
    }
    else {
        return WIFI_STATUS_CONNECTING;
    }   
}

void WifiConnect::ensureClient() {
    if (client == NULL) {
        client = new AsyncClient(NULL);
        client->onData([=](void* arg, AsyncClient* client, void *data, size_t len){ onSocketData((char *)data, len); });
        client->onConnect([=](void* arg, AsyncClient* client){ onSocketConnected(); });
        client->onDisconnect([=](void* arg, AsyncClient* client){ onSocketDisconnected(); });
    }
}

void WifiConnect::loop() {
    if (!enabled) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        if (mode == MODE_FLOUD) {
            if (!config->floudToken.isEmpty()) {
                switch (state) {
                    case STATE_FLOUD_DISCONNECTED:
                        if (reconnectTime <= millis()) {
                            ESP_LOGI(LOG_TAG, "Connecting to Floud");
                            ensureClient();
                            client->connect(FLOUD_HOST, FLOUD_PORT);
                            reconnectTime = millis() + CONNECT_RETRY_INTERVAL_MS;
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
        else if (mode == MODE_OTA_UPDATE) {
            switch (state) {
                case STATE_OTA_UPDATE_READY:
                    ensureClient();
                    client->connect(updateFirmwareHost.c_str(), 80);
                    state = STATE_OTA_UPDATE_CONNECTING;
                    break;
                case STATE_OTA_UPDATE_CONNECTED:
                    requestOTAUpdateData();
                    break;
            }
        }
    }
    else if (reconnectTime > 0 && reconnectTime <= millis()) {
        reconnectTime = millis() + CONNECT_RETRY_INTERVAL_MS;
        reconnect();
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
    if (receivedMessage.type == CommandType::STATUS_ERROR) {
        socketReconnect(); // reset on error
    }
    else if (receivedMessage.type == CommandType::STATUS_OK) {
        if (state == STATE_FLOUD_AUTHORIZING && receivedMessage.id == authorizationMessageId) {
            ESP_LOGI(LOG_TAG, "Authorized");
            authorizationFailed = false;
            state = STATE_FLOUD_AUTHORIZED;
        }
    }
    else if (receivedMessage.type == CommandType::STATUS_UNAUTHORIZED) {
        ESP_LOGW(LOG_TAG, "Unauthorized");
        authorizationFailed = true;
        socketReconnect();
    }
    else if (state == STATE_FLOUD_AUTHORIZED) {
        // handle commands
        uint16_t responseSize = 0;
        uint16_t responseType = cmdProtocol->run(receivedMessage.type, receiveBuffer, receivedMessage.length, sendBuffer, &responseSize);
        sendMessage(responseType, receivedMessage.id, sendBuffer, responseSize);
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
    authorizationMessageId = sendRequest(CommandType::PROTOCOL_AUTH, sendBuffer, token.length()); // dont send the 0 terminate char
}

uint16_t WifiConnect::sendRequest(const uint16_t type, const char* payload, const size_t payloadSize) {
    uint16_t messageId = messageIdCounter++;
    sendRequest(type, messageId, payload, payloadSize);
    return messageId;
}

void WifiConnect::sendRequest(const uint16_t type, const uint16_t id, const char* payload, const size_t payloadSize) {
    sendMessage(type, id, payload, payloadSize);
    receiveTime = millis() + SOCKET_RESPONSE_TIMEOUT_MS;
}

void WifiConnect::sendMessage(const uint16_t type, const uint16_t id, const char* payload, const size_t payloadSize) {
    CommandMessageHeader header = {
        htons(type), htons(id), htons(payloadSize)
    };

    // TODO: check payload size?
    client->write((char*) &header, sizeof(header));
    client->write(payload, payloadSize);
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
    if (mode == MODE_FLOUD) {
        receiveMessage(data, len);
    }
    else if (mode == MODE_OTA_UPDATE) {
        receiveOTAUpdateData(data, len);
    }
}

void WifiConnect::onSocketConnected() {
    if (mode == MODE_FLOUD) {
        ESP_LOGI(LOG_TAG, "Connected to Floud");
        reconnectTime = 0;
        state = STATE_FLOUD_ESTABLISHED;
    }
    else if (mode == MODE_OTA_UPDATE) {
        ESP_LOGI(LOG_TAG, "Connected to OTA server");
        state = STATE_OTA_UPDATE_CONNECTED;
    }
}

void WifiConnect::onSocketDisconnected() {
    if (mode == MODE_FLOUD) {
        if (state == STATE_FLOUD_CONNECTING) {
            ESP_LOGI(LOG_TAG, "Failed to connect to Floud");
            reconnectTime = millis() + CONNECT_RETRY_INTERVAL_MS;
        }
        else {
            ESP_LOGI(LOG_TAG, "Disconnected from Floud");
            reconnectTime = millis() + RECONNECT_INTERVAL_MS;
        }
        state = STATE_FLOUD_DISCONNECTED;
    }
    else if (mode == MODE_OTA_UPDATE) {
        if (state == STATE_OTA_UPDATE_PREPARING) {
            state = STATE_OTA_UPDATE_READY;
        }
        else if (state == STATE_OTA_UPDATE_DATA) {
            ESP_LOGI(LOG_TAG, "OTA data received");
            finalizeOTAUpdate();
        }
        else {
            ESP_LOGI(LOG_TAG, "Disconnected from OTA server");
            mode = MODE_FLOUD;
            state = STATE_FLOUD_DISCONNECTED;
        }
    }
}

// WIFI

void WifiConnect::onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!wifiConnected) {
        ESP_LOGI(LOG_TAG, "Failed to connect WiFi");
        reconnectTime = millis() + CONNECT_RETRY_INTERVAL_MS;
        wifiFailed = true;
        WiFi.disconnect(true); // turn off wifi
    }
    else {
        ESP_LOGI(LOG_TAG, "Wifi lost: %d", info.disconnected.reason);
        reconnectTime = millis() + RECONNECT_INTERVAL_MS;
        wifiConnected = false;
        WiFi.disconnect(true); // turn off wifi
    }
}

void WifiConnect::onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_LOGI(LOG_TAG, "Wifi connected");
    // TODO
}

void WifiConnect::onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info) {
    ESP_LOGI(LOG_TAG, "Wifi got IP");
    reconnectTime = 0;
    wifiConnected = true;
    wifiFailed = false;
}

// OTA

bool WifiConnect::isOTAUpdateRunning() {
    return mode == MODE_OTA_UPDATE;
}

void WifiConnect::startOTAUpdate(String firmwareUrl) {
    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(LOG_TAG, "Staring OTA update: %s", firmwareUrl.c_str());
        String host, path;

        int hostIndex = firmwareUrl.indexOf('/');
        if (hostIndex != -1) {
            host = firmwareUrl.substring(0, hostIndex);
            path = firmwareUrl.substring(hostIndex);
        }

        if (host.isEmpty() || path.isEmpty()) {
            ESP_LOGI(LOG_TAG, "Invalid firmware url: %s", firmwareUrl.c_str());
            return;
        }

        mode = MODE_OTA_UPDATE;
        receiveTime = 0;
        received = false;
        updateFirmwareHost = host;
        updateFirmwarePath = path;

        if (client != NULL && (client->connecting() || client->connected())) {
            state = STATE_OTA_UPDATE_PREPARING;
            client->stop(); // disconnect from floud and wait for confirm
        }
        else {
            state = STATE_OTA_UPDATE_READY;
        }
    }
}

void WifiConnect::requestOTAUpdateData() {
    state = STATE_OTA_UPDATE_HEADER;

    ESP_LOGI(LOG_TAG, "Requesting OTA update: host=%s path=%s", updateFirmwareHost.c_str(), updateFirmwarePath.c_str());
    client->write(String("GET " + updateFirmwarePath + " HTTP/1.1\r\n").c_str());
    client->write(String("Host: " + updateFirmwareHost + "\r\n").c_str());
    client->write("Cache-Control: no-cache\r\n");
    client->write("Connection: close\r\n\r\n");
}

inline String getHeaderValue(String header, String headerName) {
    return header.substring(strlen(headerName.c_str()));
}

void WifiConnect::receiveOTAUpdateData(char *data, size_t len) {
    if (state == STATE_OTA_UPDATE_HEADER) {
        int contentLength = 0;
        bool isValidContentType = false;
        bool isSuccess = false;
        String line;

        char *token = strtok(data, "\n");
        while (token) {
            line = String(token);
            line.trim();
            
            if (line.startsWith("HTTP/1.1")) {
                if (line.indexOf("200") != -1) {
                    isSuccess = true;
                }
                else {
                    ESP_LOGE(LOG_TAG, "Invalid reponse from server: %s", line.c_str());
                }
            }
            else if (line.startsWith("Content-Length: ")) {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                ESP_LOGI(LOG_TAG, "Firmware size: %d", contentLength);
            }
            else if (line.startsWith("Content-Type: ")) {
                String contentType = getHeaderValue(line, "Content-Type: ");
                ESP_LOGI(LOG_TAG, "Content type: %s", contentType.c_str());
                if (contentType == "application/octet-stream") {
                    isValidContentType = true;
                }
            }
            else if (line.isEmpty()) { // end of headers
                break;
            }

            token = strtok(NULL, "\n");
        }

        if (isSuccess && contentLength > 0 && isValidContentType) {
            if (Update.begin(contentLength)) {
                ESP_LOGI(LOG_TAG, "Running OTA update: size=%d", contentLength);
                state = STATE_OTA_UPDATE_DATA;
                updateDataLength = contentLength;
                updateReceivedBytes = 0;
            }
            else {
                ESP_LOGI(LOG_TAG, "Low space for OTA: size=%d", contentLength);
                client->close();
            }
        }
        else {
            ESP_LOGI(LOG_TAG, "Invalid firmware file");
            client->close();
        }
    }
    else if (state == STATE_OTA_UPDATE_DATA) {
        updateReceivedBytes += len;
        Update.write((uint8_t *)data, len);
    }
}

void WifiConnect::finalizeOTAUpdate() {
    if (Update.end()) {
        if (Update.isFinished()) {
            ESP_LOGI(LOG_TAG, "OTA successful, restarting");
            ESP.restart();
        }
        else {
            ESP_LOGI(LOG_TAG, "OTA failed");
        }
    }
    else {
        ESP_LOGI(LOG_TAG, "OTA failed: %d", Update.getError());
    }
    mode = MODE_FLOUD;
    state = STATE_FLOUD_DISCONNECTED;
}