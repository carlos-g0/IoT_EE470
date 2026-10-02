/*
====================================================
Title: ESP8266 Battery Voltage Monitor
====================================================
Program Detail:

Purpose:
Measure the voltage of a LiPo battery using the
ESP8266 analog input and an external voltage divider.

Inputs:
Battery voltage through voltage divider connected to A0

Outputs:
Raw ADC reading
Calculated A0 voltage
Calculated battery voltage through Serial Monitor

Date:
September 30, 2026

Author:
Carlos Gonzalez

Versions:
V1 - Initial ADC battery measurement program
====================================================
File Dependencies:
Arduino framework
====================================================
Main Program
====================================================
*/

#include <Arduino.h>

// Measured voltage-divider resistor values
const float R1 = 9.55;     // kOhms
const float R2 = 9.56;     // kOhms

// NodeMCU exposed A0 full-scale voltage
const float ADC_MAX_VOLTAGE = 3.17;

// 10-bit ADC maximum value
const float ADC_MAX = 1023.0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("ESP8266 Battery Voltage Monitor");
    Serial.println("--------------------------------");
}

void loop()
{
    for (int i = 1; i <= 20; i++)
    {
        int adcValue = analogRead(A0);

        float a0Voltage =
            adcValue * (ADC_MAX_VOLTAGE / ADC_MAX);

        float batteryVoltage =
            a0Voltage * ((R1 + R2) / R2);

        Serial.print(i);
        Serial.print(", ");
        Serial.print(adcValue);
        Serial.print(", ");
        Serial.println(batteryVoltage, 3);

        delay(1000);
    }

    // Stop here after 20 measurements
    while (true)
    {
        delay(1000);
    }
}
