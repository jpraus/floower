#include "WifiConnect.h"
#include "WiFi.h"
#include "HTTPClient.h"

WifiConnect::WifiConnect(Config *config) 
        : config(config) {
}

void WifiConnect::init() {
    connect();
}

void WifiConnect::connect() {
    delay(5000);

    WiFi.mode(WIFI_STA);
    WiFi.begin("", "");
    Serial.print("Connecting to ");
    Serial.println("floowerlab");
    
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

    request();
}

void WifiConnect::request() {
    //Check WiFi connection status
    if (WiFi.status()== WL_CONNECTED) {
        HTTPClient http;

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