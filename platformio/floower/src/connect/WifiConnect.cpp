#include "WifiConnect.h"
#include <Update.h>
#include <esp_task_wdt.h>

#define OTA_RESPONSE_TIMEOUT_MS 5000

#define MESSAGE_TYPE_STATUS_OK 0
#define MESSAGE_TYPE_STATUS_ERROR 1
#define MESSAGE_TYPE_STATUS_UNAUTHORIZED 2
#define MESSAGE_TYPE_AUTH 10
#define MESSAGE_TYPE_PETALS 20
#define MESSAGE_TYPE_COLOR 21

WifiConnect::WifiConnect(Config *config) 
        : config(config) {
}

void WifiConnect::init() {
    connect();
}

void WifiConnect::connect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin("puchovi", "12EA34EA56");
    Serial.print("Connecting ");
    
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(500);
    
        if ((++i % 16) == 0) {
            Serial.println(F(" still trying to connect"));
        }
    }

    Serial.print(F("Connected. My IP address is: "));
    Serial.println(WiFi.localIP());

    client.connect("floowergarden-dev.eu-west-1.elasticbeanstalk.com", 80);

    //runOTAUpdate();
}
/*
void WifiConnect::request() {
    //Check WiFi connection status
    if (WiFi.status()== WL_CONNECTED) {
        String serverPath = "http://api.floower.io/";
        
        // Your Domain name with URL path or IP address with path
        http.begin(serverPath.c_str());
        
        // Send HTTP GET request
        int httpResponseCode = http.GET();
        
        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            String payload = http.getString();
            Serial.println(payload);
        }
        else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }
        // Free resources
        http.end();
    }
    else {
        Serial.println("WiFi Disconnected");
    }
}
*/
void WifiConnect::receive() {
    if (client.connected()) {
        while (client.available() > 0) {
            readMessage();
        }
    }

    // authorize
    if (client.connected()) {
        Serial.println("Sending auth packet");

        String payload = "nejbezpecnejsi";
        WifiMessageHeader header = {MESSAGE_TYPE_AUTH, 1, (uint16_t) payload.length()};
        payload.toCharArray((char *)payloadBuffer, payload.length() + 1); // add +1 to accomodate for 0 terminate string

        sendMessage(header, payloadBuffer);
        delay(1000);
    }
}

bool WifiConnect::readMessage() {
    WifiMessageHeader header;
    const int ret = readMessageHeader(header);

    if (ret == 0) {
        // no data ??
        return true;
    }

    if (header.length > WIFI_MAX_PAYLOAD_BYTES) {
        // too big
        return true;
    }

    if (header.length != client.read(payloadBuffer, header.length)) {
        // failed to read data
        return false;
    }
    payloadBuffer[header.length] = '\0';

    return true;
}

int8_t WifiConnect::readMessageHeader(WifiMessageHeader& header) {
    size_t rlen = client.read((uint8_t*)&header, sizeof(header));
    if (rlen == 0) {
        return 0;
    }

    if (sizeof(header) != rlen) {
        return -1;
    }

    //BLYNK_DBG_DUMP(">", &header, sizeof(WifiMessageHeader));

    header.type = ntohs(header.type);
    header.id = ntohs(header.id);
    header.length = ntohs(header.length);

    Serial.println("Got message");
    Serial.println(header.type);
    Serial.println(header.id);
    Serial.println(header.length);

    return rlen;
}

void WifiConnect::sendMessage(WifiMessageHeader& header, const uint8_t* payload) {
    header.type = htons(header.type);
    header.id = htons(header.id);
    header.length = htons(header.length);

    // TODO: check payload size?
    client.write((uint8_t*)&header, sizeof(header));
    client.write(payload, header.length);
}

// OTA
/*
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
*/