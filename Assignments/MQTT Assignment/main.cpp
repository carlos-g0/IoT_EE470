/*
 * EE 470 - MQTT Assignment
 * Sections 4.2 A through G
 *
 * Features:
 * 1. Read potentiometer values
 * 2. Publish potentiometer every 15 seconds
 * 3. Control LED using MQTT or Serial Monitor
 * 4. Publish button 1 when pressed and released
 * 5. Publish button 0 five seconds later
 * 6. Receive messages from friend's ESP8266
 * 7. Publish current LED status to HiveMQ
 * 8. Automatically reconnect Wi-Fi and MQTT
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include "secrets.h"

// --------------------------------------------------
// Hardware pins
// --------------------------------------------------

#define POT_PIN A0
#define LED_PIN D2
#define BUTTON_PIN D5

// --------------------------------------------------
// MQTT settings
// --------------------------------------------------

const char* mqttServer = "broker.mqttdashboard.com";
const int mqttPort = 8883;

// My potentiometer readings
const char* potTopic =
    "testtopic/temp/outTopic/carlosg470";

// My pushbutton messages
const char* switchTopic =
    "testtopic/temp/outTopic/carlosg470/switch";

// LED commands received from my phone
const char* ledCommandTopic =
    "testtopic/temp/inTopic1";

// New topic for reporting the LED state
const char* ledStatusTopic =
    "testtopic/temp/outTopic/carlosg470/ledStatus";

// Friend's MQTT topic
const char* friendTopic =
    "testtopic/temp/outTopic/EE470pacgeo8266/#";

const char* friendTopicRoot =
    "testtopic/temp/outTopic/EE470pacgeo8266";

// MQTT clients
BearSSL::WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// Wi-Fi diagnostic handler
WiFiEventHandler wifiDisconnectHandler;

// --------------------------------------------------
// Timing settings
// --------------------------------------------------

const unsigned long wifiCheckInterval = 10000;
const unsigned long publishInterval = 15000;
const unsigned long potReadInterval = 500;
const unsigned long debounceDelay = 50;
const unsigned long switchResetInterval = 5000;

unsigned long lastWiFiCheck = 0;
unsigned long lastPublish = 0;
unsigned long lastPotRead = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastDebounceTime = 0;
unsigned long switchPublishTime = 0;

// Button variables
int lastButtonReading = HIGH;
int buttonState = HIGH;
bool buttonWasPressed = false;

bool switchOnePending = false;
bool switchZeroPending = false;

// LED status variables
bool ledState = false;
bool ledStatusPending = true;


// --------------------------------------------------
// Wi-Fi functions
// --------------------------------------------------

// Display connection information
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


// Scan for my iPhone hotspot
void scanForHotspot() {

    Serial.println();
    Serial.println("Scanning for Wi-Fi networks...");

    int networkCount = WiFi.scanNetworks();

    if (networkCount < 0) {
        Serial.println("Wi-Fi scan failed.");
        return;
    }

    Serial.print("Networks found: ");
    Serial.println(networkCount);

    bool hotspotFound = false;

    for (int i = 0; i < networkCount; i++) {

        if (WiFi.SSID(i) == String(WIFI_SSID)) {

            hotspotFound = true;

            Serial.println("Hotspot FOUND!");

            Serial.print("Signal strength: ");
            Serial.print(WiFi.RSSI(i));
            Serial.println(" dBm");

            Serial.print("Wi-Fi channel: ");
            Serial.println(WiFi.channel(i));
        }
    }

    if (!hotspotFound) {
        Serial.println("Hotspot NOT FOUND!");
    }

    WiFi.scanDelete();

    Serial.println("Wi-Fi scan complete.");
}


// Connect to Wi-Fi
void connectWiFi() {

    Serial.println("Connecting to Wi-Fi...");

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    scanForHotspot();

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
    }
    else {
        Serial.println("Wi-Fi unavailable.");

        Serial.print("Wi-Fi status: ");
        Serial.println(WiFi.status());
    }
}


// Reconnect Wi-Fi if necessary
void maintainWiFi() {

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if (millis() - lastWiFiCheck >= wifiCheckInterval) {

        lastWiFiCheck = millis();

        Serial.println("Attempting Wi-Fi reconnection...");

        WiFi.reconnect();
    }
}


// --------------------------------------------------
// LED control
// --------------------------------------------------

// Change LED state and request MQTT status update
void setLED(bool turnOn) {

    ledState = turnOn;

    digitalWrite(
        LED_PIN,
        ledState ? HIGH : LOW
    );

    if (ledState) {
        Serial.println("LED ON");
    }
    else {
        Serial.println("LED OFF");
    }

    // Mark the new state for publishing
    ledStatusPending = true;
}


// Publish current LED state to HiveMQ
void publishLEDStatus() {

    if (!mqttClient.connected()) {
        return;
    }

    if (!ledStatusPending) {
        return;
    }

    const char* message = ledState ? "1" : "0";

    // Retain the latest LED status so the
    // phone can receive it when subscribing.
    bool success = mqttClient.publish(
        ledStatusTopic,
        message,
        true
    );

    if (success) {

        Serial.print("MQTT LED status published: ");
        Serial.println(message);

        ledStatusPending = false;
    }
    else {

        Serial.println("LED status publish failed.");
    }
}


// --------------------------------------------------
// MQTT callback
// --------------------------------------------------

// Process messages received through HiveMQ
void mqttCallback(
    char* topic,
    byte* payload,
    unsigned int length
) {

    String message = "";

    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    message.trim();

    String receivedTopic = String(topic);

    Serial.print("MQTT received on ");
    Serial.print(receivedTopic);
    Serial.print(": ");
    Serial.println(message);

    // Handle LED commands from my phone
    if (receivedTopic == ledCommandTopic) {

        if (message == "1") {
            setLED(true);
        }
        else if (message == "0") {
            setLED(false);
        }

        return;
    }

    // Handle messages from my friend's ESP8266
    String root = String(friendTopicRoot);

    bool fromFriend =
        receivedTopic == root ||
        receivedTopic.startsWith(root + "/");

    if (fromFriend) {

        // Friend publishes potentiometer and
        // pushbutton values on the same topic.
        // Treat only exact 1 and 0 as commands.

        if (message == "1") {

            Serial.println("Friend sent 1");
            setLED(true);
        }

        else if (message == "0") {

            Serial.println("Friend sent 0");
            setLED(false);
        }

        else {

            Serial.print("Friend's potentiometer: ");
            Serial.println(message);
        }
    }
}


// --------------------------------------------------
// MQTT connection
// --------------------------------------------------

void connectMQTT() {

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (mqttClient.connected()) {
        return;
    }

    if (millis() - lastMQTTAttempt < 5000) {
        return;
    }

    lastMQTTAttempt = millis();

    Serial.println("Connecting to HiveMQ...");

    String clientID = "EE470-ESP-";
    clientID += String(ESP.getChipId(), HEX);

    if (mqttClient.connect(clientID.c_str())) {

        Serial.println("MQTT connected!");

        // Subscribe to commands from my phone
        if (mqttClient.subscribe(ledCommandTopic)) {

            Serial.println("Subscribed to phone LED commands");
        }

        // Subscribe to my friend's ESP8266
        if (mqttClient.subscribe(friendTopic)) {

            Serial.println("Subscribed to friend's ESP8266!");
        }
        else {

            Serial.println("Friend subscription failed.");
        }

        // Publish the current LED state again
        // whenever MQTT reconnects.
        ledStatusPending = true;

        Serial.print("LED status topic: ");
        Serial.println(ledStatusTopic);
    }

    else {

        Serial.print("MQTT connection failed. State: ");
        Serial.println(mqttClient.state());
    }
}


// --------------------------------------------------
// Publish potentiometer
// --------------------------------------------------

void publishPotentiometer() {

    if (!mqttClient.connected()) {
        return;
    }

    int potValue = analogRead(POT_PIN);

    char message[12];

    snprintf(
        message,
        sizeof(message),
        "%d",
        potValue
    );

    if (mqttClient.publish(potTopic, message)) {

        Serial.print("MQTT Potentiometer published: ");
        Serial.println(message);
    }
    else {

        Serial.println("Potentiometer publish failed.");
    }
}


// --------------------------------------------------
// Publish pushbutton messages
// --------------------------------------------------

void publishSwitch() {

    if (!mqttClient.connected()) {
        return;
    }

    // Publish 1 after the button is released
    if (switchOnePending) {

        if (mqttClient.publish(switchTopic, "1")) {

            Serial.println("MQTT Switch published: 1");

            switchOnePending = false;
            switchZeroPending = true;

            switchPublishTime = millis();
        }

        return;
    }

    // Publish 0 five seconds after 1
    if (switchZeroPending &&
        millis() - switchPublishTime >= switchResetInterval) {

        if (mqttClient.publish(switchTopic, "0")) {

            Serial.println("MQTT Switch published: 0");

            switchZeroPending = false;
        }
    }
}


// --------------------------------------------------
// Pushbutton hardware
// --------------------------------------------------

void checkButton() {

    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
    }

    // Debounce the pushbutton
    if (millis() - lastDebounceTime > debounceDelay) {

        if (reading != buttonState) {

            buttonState = reading;

            if (buttonState == LOW) {

                buttonWasPressed = true;

                Serial.println("Switch pressed");
            }

            else if (buttonWasPressed) {

                Serial.println("Switch released");

                switchOnePending = true;
                buttonWasPressed = false;
            }
        }
    }

    lastButtonReading = reading;
}


// --------------------------------------------------
// Serial Monitor LED commands
// --------------------------------------------------

void checkSerial() {

    while (Serial.available() > 0) {

        char command = Serial.read();

        if (command == '1') {
            setLED(true);
        }

        else if (command == '0') {
            setLED(false);
        }
    }
}


// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup() {

    Serial.begin(115200);
    Serial.setTimeout(100);

    delay(1000);

    Serial.println();
    Serial.println("EE 470 - MQTT Assignment");
    Serial.println("Section 4.2 G - LED Status");
    Serial.println("-------------------------------");

    // Configure hardware
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(LED_PIN, LOW);

    ledState = false;
    ledStatusPending = true;

    Serial.println("Hardware initialized.");

    // Wi-Fi diagnostics
    wifiDisconnectHandler =
        WiFi.onStationModeDisconnected(
            [](const WiFiEventStationModeDisconnected& event) {

                Serial.print("Wi-Fi disconnect reason: ");
                Serial.println(event.reason);
            }
        );

    connectWiFi();

    // TLS for classroom testing.
    // Certificate verification is disabled.
    wifiClient.setInsecure();

    // Configure MQTT
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);

    Serial.println("Setup complete.");
}


// --------------------------------------------------
// Main loop
// --------------------------------------------------

void loop() {

    // Maintain Wi-Fi connection
    maintainWiFi();

    // Maintain MQTT connection
    connectMQTT();

    if (mqttClient.connected()) {

        // Process received MQTT messages
        mqttClient.loop();

        // Send any pending LED status update
        publishLEDStatus();
    }

    // Display potentiometer readings
    if (millis() - lastPotRead >= potReadInterval) {

        lastPotRead = millis();

        int potValue = analogRead(POT_PIN);

        Serial.print("Potentiometer: ");
        Serial.println(potValue);
    }

    // Publish potentiometer every 15 seconds
    if (millis() - lastPublish >= publishInterval) {

        lastPublish = millis();

        publishPotentiometer();
    }

    // Monitor the physical pushbutton
    checkButton();

    // Publish pushbutton messages
    publishSwitch();

    // Keep Serial Monitor LED control working
    checkSerial();
}
