#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <DHT.h>
#include <time.h>

#include "secrets.h"

// HW-507 sensor configuration
#define DHTPIN D1
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// Your PHP database endpoint
const char* serverURL =
    "https://darkseagreen-stinkbug-987347.hostingersite.com/database-part2/insert.php";

// Sensor registration name
const char* nodeName = "node_1";

// Send data every 10 seconds
const unsigned long sendInterval = 10000;
unsigned long previousSend = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("EE 470 - IoT Live Sensor");
    Serial.println("------------------------");

    // Initialize HW-507
    dht.begin();

    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 20000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("Wi-Fi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("Wi-Fi not connected yet.");
    }

    // Synchronize time using NTP (UTC)
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    // Send the first reading as soon as possible
    previousSend = millis() - sendInterval;
}

void loop() {

    // Check Wi-Fi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected. Reconnecting...");
        WiFi.reconnect();
        delay(2000);
        return;
    }

    // Wait until 10 seconds have passed
    if (millis() - previousSend < sendInterval) {
        return;
    }

    previousSend = millis();

    // Read sensor values
    float temperatureC = dht.readTemperature();
    float temperatureF = dht.readTemperature(true);
    float humidity = dht.readHumidity();

    // Validate sensor readings
    if (isnan(temperatureC) ||
        isnan(temperatureF) ||
        isnan(humidity)) {

        Serial.println("Sensor reading failed!");
        return;
    }

    // Display Fahrenheit in Serial Monitor
    Serial.println("------------------------");

    Serial.print("Temperature: ");
    Serial.print(temperatureF, 1);
    Serial.println(" F");

    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");

    // Get current UTC time
    time_t now = time(nullptr);

    if (now < 1700000000) {
        Serial.println("Waiting for NTP time...");
        return;
    }

    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);

    char timestamp[20];

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    // Encode the timestamp for the URL
    String encodedTime = String(timestamp);
    encodedTime.replace(" ", "%20");
    encodedTime.replace(":", "%3A");

    // Build HTTP GET URL
    String url = String(serverURL);
    url += "?node_name=";
    url += nodeName;
    url += "&time_received=";
    url += encodedTime;
    url += "&temperature=";
    url += String(temperatureC, 1);
    url += "&humidity=";
    url += String(humidity, 1);

    // Create HTTPS connection
    BearSSL::WiFiClientSecure client;

    // Classroom connectivity test only:
    // This disables TLS certificate verification.
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    if (!http.begin(client, url)) {
        Serial.println("HTTP connection setup failed.");
        return;
    }

    // Send the API key in the HTTP header
    http.addHeader("X-API-Key", API_KEY);

    // Send GET request
    int httpCode = http.GET();

    Serial.print("HTTP Status: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
        String response = http.getString();
        Serial.println(response);

        if (httpCode == 201) {
            Serial.println("Database updated successfully!");
        }
    } else {
        Serial.print("Connection error: ");
        Serial.println(http.errorToString(httpCode));
    }

    http.end();
}
