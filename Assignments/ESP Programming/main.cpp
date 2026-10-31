/*
 * --------------------------------------------------
 * Project: EE 470 - Programming ESP8266
 * File: main.cpp
 * Author: Carlos Gonzalez
 * Date: 10/30/2026
 * Version: 3.1
 *
 * Purpose:
 * Read temperature, humidity, and light level
 * when the pushbutton or tilt switch is activated.
 * Send measurements to a MariaDB database through
 * an HTTPS GET request.
 *
 * Failed messages are stored and retransmitted
 * every 30 seconds until delivery is confirmed.
 *
 * The user can select one of seven time zones
 * through the USB Serial Monitor. The ESP8266
 * uses NTP for its clock and retrieves the current
 * selected-zone time from TimeAPI.io.
 *
 * Inputs:
 * D1 - HW-507 DHT11 temperature/humidity sensor
 * D2 - HW-501 tilt switch
 * D5 - Pushbutton switch
 * A0 - HW-486 photoresistor
 * USB - Time zone selection
 *
 * Outputs:
 * Serial Monitor messages
 * HTTPS requests to Hostinger
 * Sensor readings stored in MariaDB
 * --------------------------------------------------
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <DHT.h>
#include <time.h>
#include <stdlib.h>

#include "secrets.h"


// --------------------------------------------------
// Hardware pin assignments
// --------------------------------------------------

#define DHT_PIN D1
#define DHT_TYPE DHT11

#define TILT_PIN D2
#define BUTTON_PIN D5
#define LIGHT_PIN A0

DHT dht(DHT_PIN, DHT_TYPE);


// --------------------------------------------------
// Hostinger server information
// --------------------------------------------------

const char* serverURL =
    "https://darkseagreen-stinkbug-987347.hostingersite.com/database-part2/insert.php";


// --------------------------------------------------
// Timing settings
// --------------------------------------------------

const unsigned long debounceDelay = 50;
const unsigned long retryInterval = 30000;
const unsigned long wifiCheckInterval = 10000;

// Maximum number of measurements waiting
// for delivery
const int MAX_PENDING = 10;


// --------------------------------------------------
// Switch information
// --------------------------------------------------

struct SwitchState {

    uint8_t pin;
    int lastRaw;
    int stableState;
    unsigned long lastChange;

};

SwitchState button = {BUTTON_PIN, HIGH, HIGH, 0};
SwitchState tilt = {TILT_PIN, HIGH, HIGH, 0};


// --------------------------------------------------
// Store measurements for automatic retransmission
// --------------------------------------------------

struct PendingReading {

    String nodeName;
    String timestamp;

    float temperatureC;
    float humidity;
    int lightLevel;

    unsigned long lastAttempt;
    bool attempted;

};

PendingReading pending[MAX_PENDING];

int pendingCount = 0;

unsigned long lastWiFiCheck = 0;


// --------------------------------------------------
// Wi-Fi Functions
// --------------------------------------------------

// Display network information
void printNetworkInformation() {

    Serial.println("Wi-Fi Connected!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}


// Connect ESP8266 to phone hotspot
void connectWiFi() {

    Serial.println("Connecting to Wi-Fi...");

    WiFi.mode(WIFI_STA);

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 20000) {

        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {

        printNetworkInformation();

    } else {

        Serial.println("Wi-Fi unavailable.");
    }
}


// Try reconnecting if Wi-Fi is disconnected
void maintainWiFi() {

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if (millis() - lastWiFiCheck >= wifiCheckInterval) {

        lastWiFiCheck = millis();

        Serial.println(
            "Attempting Wi-Fi reconnection..."
        );

        WiFi.reconnect();
    }
}


// --------------------------------------------------
// Time Zone Information
// --------------------------------------------------

// Names displayed in the Serial Monitor
const char* timeZoneNames[] = {

    "Eastern - New York",
    "Central - Chicago",
    "Mountain - Denver",
    "Pacific - Los Angeles",
    "Alaska - Anchorage",
    "Hawaii - Honolulu",
    "Atlantic - Puerto Rico"

};


// Time zone rules used by the ESP8266 clock
// Includes daylight saving time where applicable
const char* timeZoneRules[] = {

    "EST5EDT,M3.2.0,M11.1.0",
    "CST6CDT,M3.2.0,M11.1.0",
    "MST7MDT,M3.2.0,M11.1.0",
    "PST8PDT,M3.2.0,M11.1.0",
    "AKST9AKDT,M3.2.0,M11.1.0",
    "HST10",
    "AST4"

};


// Time zone names used by TimeAPI.io
const char* timeZoneApiIds[] = {

    "America/New_York",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Anchorage",
    "Pacific/Honolulu",
    "America/Puerto_Rico"

};


// Default to California Pacific Time
int selectedTimeZone = 4;


// --------------------------------------------------
// Clock Functions
// --------------------------------------------------

// Synchronize ESP8266 clock using NTP
void initializeClock() {

    // Start with Pacific Time
    configTime(
        "PST8PDT,M3.2.0,M11.1.0",
        "pool.ntp.org",
        "time.google.com"
    );

    Serial.println("Synchronizing clock...");

    unsigned long start = millis();

    while (time(nullptr) < 1700000000 &&
           millis() - start < 15000) {

        delay(250);
    }

    if (time(nullptr) >= 1700000000) {

        Serial.println("Clock synchronized.");

    } else {

        Serial.println("Clock not synchronized yet.");
    }
}


// Generate a timestamp using the selected time zone
bool getTimestamp(char* buffer, size_t size) {

    time_t now = time(nullptr);

    // Do not use an invalid clock value
    if (now < 1700000000) {
        return false;
    }

    struct tm timeInfo;

    // Convert to the selected local time zone
    localtime_r(&now, &timeInfo);

    return strftime(
        buffer,
        size,
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    ) > 0;
}


// Show current local time from NTP clock
void showCurrentTime() {

    char timestamp[20];

    if (getTimestamp(timestamp, sizeof(timestamp))) {

        Serial.print("Current Time: ");
        Serial.println(timestamp);

    } else {

        Serial.println("Time not synchronized.");
    }
}


// --------------------------------------------------
// TimeAPI.io Function
// --------------------------------------------------

// Retrieve current time for the selected time zone
// from the external TimeAPI.io service.
//
// The API time is displayed in the Serial Monitor.
// The NTP clock is still used when recording
// individual sensor measurements.
void getTimeFromAPI(int selection) {

    if (selection < 1 || selection > 7) {
        return;
    }

    Serial.println();
    Serial.println("Contacting TimeAPI.io...");

    // Make sure Wi-Fi is available
    if (WiFi.status() != WL_CONNECTED) {

        Serial.println(
            "Time API unavailable: no Wi-Fi."
        );

        Serial.println("Using existing NTP clock.");

        return;
    }

    // Get the selected time zone identifier
    String zone = timeZoneApiIds[selection - 1];

    // Encode the slash for the URL
    zone.replace("/", "%2F");

    // Build the API request URL
    String url =
        "https://timeapi.io/api/Time/current/zone?timeZone=";

    url += zone;

    // Create secure Wi-Fi client
    BearSSL::WiFiClientSecure client;

    // Temporary lab configuration
    // Server certificate is not verified
    client.setInsecure();

    HTTPClient http;

    http.setTimeout(5000);

    // Begin HTTPS connection
    if (!http.begin(client, url)) {

        Serial.println(
            "Time API connection setup failed."
        );

        return;
    }

    // Send HTTP GET request
    int httpCode = http.GET();

    Serial.print("Time API HTTP Status: ");
    Serial.println(httpCode);

    if (httpCode == 200) {

        // Read the JSON response
        String response = http.getString();

        // Locate the dateTime field
        int keyPosition =
            response.indexOf("\"dateTime\"");

        String apiTime = "";

        if (keyPosition >= 0) {

            int colonPosition =
                response.indexOf(':', keyPosition);

            if (colonPosition >= 0) {

                int firstQuote =
                    response.indexOf(
                        '"',
                        colonPosition + 1
                    );

                if (firstQuote >= 0) {

                    int secondQuote =
                        response.indexOf(
                            '"',
                            firstQuote + 1
                        );

                    if (secondQuote > firstQuote) {

                        apiTime = response.substring(
                            firstQuote + 1,
                            secondQuote
                        );
                    }
                }
            }
        }

        // Make sure the timestamp was found
        if (apiTime.length() >= 19) {

            // Use YYYY-MM-DD HH:MM:SS format
            apiTime = apiTime.substring(0, 19);

            apiTime.replace('T', ' ');

            Serial.print("Time API Zone: ");
            Serial.println(
                timeZoneApiIds[selection - 1]
            );

            Serial.print("Time API Current Time: ");
            Serial.println(apiTime);

        } else {

            Serial.println(
                "Unable to read dateTime from API response."
            );
        }

    } else {

        Serial.println("Time API request failed.");

        Serial.println(
            "Using existing NTP clock."
        );
    }

    http.end();
}


// --------------------------------------------------
// Time Zone Selection Functions
// --------------------------------------------------

// Change time zone based on user selection
void changeTimeZone(int selection) {

    if (selection < 1 || selection > 7) {

        Serial.println("Invalid time zone.");

        return;
    }

    selectedTimeZone = selection;

    // Update the ESP8266 local time zone
    setenv(
        "TZ",
        timeZoneRules[selection - 1],
        1
    );

    tzset();

    Serial.println();

    Serial.print("Selected Time Zone: ");
    Serial.println(timeZoneNames[selection - 1]);

    // Display the current time from NTP
    Serial.println("ESP8266 NTP Time:");

    showCurrentTime();

    // Retrieve current time from TimeAPI.io
    getTimeFromAPI(selection);
}


// Display the time zone menu
void showTimeZoneMenu() {

    Serial.println();

    Serial.println(
        "===== TIME ZONE SELECTION ====="
    );

    for (int i = 0; i < 7; i++) {

        Serial.print(i + 1);
        Serial.print(". ");

        Serial.println(timeZoneNames[i]);
    }

    Serial.println();

    Serial.println("Default: 4 - Pacific Time");

    Serial.println(
        "Enter a number from 1 to 7."
    );

    Serial.println(
        "=============================="
    );
}


// Check for a selection through USB Serial
void checkTimeZoneSelection() {

    if (Serial.available() == 0) {
        return;
    }

    String input = Serial.readStringUntil('\n');

    input.trim();

    if (input.length() == 0) {
        return;
    }

    // Only accept numbers 1 through 7
    if (input.length() == 1 &&
        input[0] >= '1' &&
        input[0] <= '7') {

        changeTimeZone(input[0] - '0');

    } else {

        Serial.println(
            "Enter a number from 1 to 7."
        );
    }
}


// --------------------------------------------------
// Sensor Measurement Functions
// --------------------------------------------------

// Capture and save a new measurement
// when a switch is activated
void queueSensorReading(const char* nodeName) {

    // Check if the message queue is full
    if (pendingCount >= MAX_PENDING) {

        Serial.println("Message queue full!");

        Serial.println(
            "New reading not recorded."
        );

        return;
    }

    // Read the sensors one time
    float temperatureC = dht.readTemperature();

    float humidity = dht.readHumidity();

    int lightLevel = analogRead(LIGHT_PIN);

    // Check for DHT11 read errors
    if (isnan(temperatureC) || isnan(humidity)) {

        Serial.println("DHT11 reading failed.");

        return;
    }

    // Generate the measurement timestamp
    char timestamp[20];

    if (!getTimestamp(
            timestamp,
            sizeof(timestamp)
        )) {

        Serial.println(
            "Valid timestamp unavailable."
        );

        return;
    }

    // Store the original measurement
    PendingReading reading;

    reading.nodeName = nodeName;
    reading.timestamp = timestamp;

    reading.temperatureC = temperatureC;
    reading.humidity = humidity;
    reading.lightLevel = lightLevel;

    reading.lastAttempt = 0;
    reading.attempted = false;

    // Add the measurement to the queue
    pending[pendingCount] = reading;

    pendingCount++;

    // Display the captured measurement
    Serial.println();

    Serial.println(
        "===== NEW SENSOR ACTIVATION ====="
    );

    Serial.print("Node: ");
    Serial.println(reading.nodeName);

    Serial.print("Temperature: ");
    Serial.print(reading.temperatureC, 2);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(reading.humidity, 2);
    Serial.println(" %");

    Serial.print("Light Level (ADC): ");
    Serial.println(reading.lightLevel);

    Serial.print("Timestamp: ");
    Serial.println(reading.timestamp);

    Serial.print("Messages waiting: ");
    Serial.println(pendingCount);
}


// --------------------------------------------------
// HTTP Transmission Function
// --------------------------------------------------

// Send a stored measurement to Hostinger
//
// Returns true only if the server confirms
// successful delivery or an existing record.
bool transmitReading(const PendingReading& reading) {

    // Do not attempt HTTP if Wi-Fi is unavailable
    if (WiFi.status() != WL_CONNECTED) {

        Serial.println("Wi-Fi disconnected.");

        return false;
    }

    // Encode timestamp for the URL
    String encodedTime = reading.timestamp;

    encodedTime.replace(" ", "%20");
    encodedTime.replace(":", "%3A");

    // Build HTTP GET request using stored values
    String url = String(serverURL);

    url += "?node_name=";
    url += reading.nodeName;

    url += "&time_received=";
    url += encodedTime;

    url += "&temperature=";
    url += String(reading.temperatureC, 2);

    url += "&humidity=";
    url += String(reading.humidity, 2);

    url += "&light_level=";
    url += String(reading.lightLevel);

    // Create HTTPS client
    BearSSL::WiFiClientSecure client;

    // Temporary lab configuration
    // Certificate verification is disabled
    client.setInsecure();

    HTTPClient http;

    http.setTimeout(5000);

    // Begin HTTPS connection
    if (!http.begin(client, url)) {

        Serial.println("HTTP setup failed.");

        return false;
    }

    // Include the database API key
    http.addHeader("X-API-Key", API_KEY);

    Serial.println();

    Serial.println(
        "Sending stored measurement..."
    );

    // Send the GET request
    int httpCode = http.GET();

    Serial.print("HTTP Status: ");
    Serial.println(httpCode);

    bool delivered = false;

    if (httpCode > 0) {

        String response = http.getString();

        Serial.println(response);

        // New measurement accepted
        if (httpCode == 201) {

            Serial.println(
                "Delivery confirmed!"
            );

            delivered = true;

        }

        // Measurement already exists
        else if (
            httpCode == 409 &&
            response.indexOf(
                "\"already_stored\":true"
            ) >= 0
        ) {

            Serial.println(
                "Reading already stored."
            );

            delivered = true;

        }

        // Server did not confirm delivery
        else {

            Serial.println(
                "Delivery not confirmed."
            );
        }

    } else {

        // HTTP connection or network error
        Serial.print("HTTP error: ");

        Serial.println(
            http.errorToString(httpCode)
        );
    }

    http.end();

    return delivered;
}


// --------------------------------------------------
// Automatic Retransmission Function
// --------------------------------------------------

// Send queued messages and retry failures
// every 30 seconds until delivery is confirmed
void processPendingMessages() {

    if (pendingCount == 0) {
        return;
    }

    // Always process oldest pending message first
    PendingReading& reading = pending[0];

    // Wait 30 seconds before retrying
    if (reading.attempted &&
        millis() - reading.lastAttempt <
            retryInterval) {

        return;
    }

    // Record when this attempt started
    reading.attempted = true;

    reading.lastAttempt = millis();

    // Attempt transmission
    bool delivered = transmitReading(reading);

    if (delivered) {

        Serial.println(
            "Removing delivered message."
        );

        // Shift remaining messages forward
        for (int i = 1; i < pendingCount; i++) {

            pending[i - 1] = pending[i];
        }

        pendingCount--;

        Serial.print(
            "Messages still waiting: "
        );

        Serial.println(pendingCount);

    } else {

        // Preserve original timestamp and values
        Serial.println(
            "Transmission failed."
        );

        Serial.println(
            "Original message preserved."
        );

        Serial.println(
            "Retry scheduled in 30 seconds."
        );
    }
}


// --------------------------------------------------
// Switch Functions
// --------------------------------------------------

// Read and store the initial switch state
void initializeSwitch(SwitchState& sw) {

    sw.lastRaw = digitalRead(sw.pin);

    sw.stableState = sw.lastRaw;

    sw.lastChange = millis();
}


// Detect a new switch activation
void checkSwitch(
    SwitchState& sw,
    const char* nodeName
) {

    int rawState = digitalRead(sw.pin);

    // Check if the raw input changed
    if (rawState != sw.lastRaw) {

        sw.lastChange = millis();

        sw.lastRaw = rawState;
    }

    // Wait for the switch signal to stabilize
    if (millis() - sw.lastChange >=
        debounceDelay) {

        if (rawState != sw.stableState) {

            sw.stableState = rawState;

            // Both switches activate on LOW
            if (sw.stableState == LOW) {

                // Capture one measurement
                queueSensorReading(nodeName);

            } else {

                Serial.print(nodeName);

                Serial.println(
                    " switch released/reset."
                );
            }
        }
    }
}


// --------------------------------------------------
// Setup Function
// --------------------------------------------------

void setup() {

    Serial.begin(115200);

    Serial.setTimeout(100);

    delay(1000);

    Serial.println();

    Serial.println(
        "EE 470 - ESP8266 Data Transmission"
    );

    Serial.println(
        "Version 3.1 - Time API and Retries"
    );

    Serial.println(
        "--------------------------------"
    );

    // Initialize DHT11
    dht.begin();

    // Initialize pushbutton and tilt switch
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(TILT_PIN, INPUT);

    initializeSwitch(button);

    initializeSwitch(tilt);

    Serial.println("Hardware initialized.");

    // Connect to phone hotspot
    connectWiFi();

    // Synchronize ESP8266 clock
    initializeClock();

    // Select Pacific Time by default
    // Also retrieves California time from API
    changeTimeZone(4);

    // Display the seven time zone options
    showTimeZoneMenu();

    Serial.println();

    Serial.println("System ready.");

    Serial.println(
        "Pushbutton -> node_1"
    );

    Serial.println(
        "Tilt switch -> node_2"
    );

    Serial.println(
        "Retry interval: 30 seconds"
    );
}


// --------------------------------------------------
// Main Loop
// --------------------------------------------------

void loop() {

    // Check USB Serial time zone selection
    checkTimeZoneSelection();

    // Reconnect Wi-Fi when needed
    maintainWiFi();

    // Check pushbutton
    checkSwitch(button, "node_1");

    // Check tilt switch
    checkSwitch(tilt, "node_2");

    // Process new and failed transmissions
    processPendingMessages();

    delay(1);
}
