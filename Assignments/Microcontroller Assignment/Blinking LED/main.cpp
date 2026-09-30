/*
====================================================
Title: ESP8266 LED Blink
====================================================
Program Detail:

Purpose:
Blink the built-in LED on the ESP8266 and display
the LED status in the Serial Monitor.

Inputs:
None

Outputs:
Built-in LED
Serial Monitor messages

Date:
September 30, 2026

Author:
Carlos Gonzalez

Versions:
V1 - Initial LED blink and serial output program
====================================================
File Dependencies:
Arduino framework
====================================================
Main Program
====================================================
*/

#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);

    delay(1000);

    Serial.println("ESP8266 LED Blink Program Started");
}

void loop()
{
    // Built-in LED on the NodeMCU is active LOW
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("LED is ON");
    delay(1000);

    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED is OFF");
    delay(1000);
}
