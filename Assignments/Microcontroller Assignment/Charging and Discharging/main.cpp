/*
====================================================
Title: ESP8266 LiPo Charging Monitor
====================================================
Program Detail:

Purpose:
Measure and record the LiPo battery voltage once
every 60 seconds during the charging experiment.

Inputs:
Battery voltage through voltage divider connected to A0

Outputs:
Sample number, elapsed time, raw ADC reading,
and calculated battery voltage through Serial Monitor

Date:
October 1, 2026

Author:
Carlos Gonzalez

Versions:
V1 - 60-second charging data logger
====================================================
File Dependencies:
Arduino framework
====================================================
Main Program
====================================================
*/

#include <Arduino.h>

// Actual measured voltage-divider resistors
const float R1 = 9.55;   // kOhms
const float R2 = 9.56;   // kOhms

// ADC calibration value
const float ADC_MAX_VOLTAGE = 3.17;

// ESP8266 10-bit ADC
const float ADC_MAX = 1023.0;

unsigned long sampleNumber = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("Sample,Time_min,ADC,Battery_V");
}

void loop()
{
    // Read the analog input
    int adcValue = analogRead(A0);

    // Convert raw ADC reading to voltage at A0
    float a0Voltage =
        adcValue * (ADC_MAX_VOLTAGE / ADC_MAX);

    // Convert divider voltage back to battery voltage
    float batteryVoltage =
        a0Voltage * ((R1 + R2) / R2);

    // Print CSV-style data
    Serial.print(sampleNumber);
    Serial.print(",");

    Serial.print(sampleNumber);
    Serial.print(",");

    Serial.print(adcValue);
    Serial.print(",");

    Serial.println(batteryVoltage, 3);

    // Move to next minute/sample
    sampleNumber++;

    // Wait exactly 60 seconds
    delay(60000);
}
