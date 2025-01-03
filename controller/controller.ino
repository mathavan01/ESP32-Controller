#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

BleGamepad bleGamepad;

#define numOfButtons 5
#define numOfHats 1 // Maximum 4 hat switches supported

byte previousButtonStates[numOfButtons];
byte currentButtonStates[numOfButtons];
byte buttonPins[numOfButtons] = {15, 26, 0, 33, 32};
byte physicalButtons[numOfButtons] = {1, 2, 3, 4, 5};

byte previousHatStates[numOfHats * 4];
byte currentHatStates[numOfHats * 4];
byte hatPins[numOfHats * 4] = {13, 18, 21, 23}; // in order UP, LEFT, DOWN, RIGHT. 4 pins per hat switch (Eg. List 12 pins if there are 3 hat switches)

const int xpin = 34;                  // x axis is connected to GPIO 34 (Analog ADC1_CH6)
const int ypin = 35;                  // y axis is connected to GPIO 35
const int numberOfPotSamples = 5;     // Number of pot samples to take (to smooth the values)
const int delayBetweenSamples = 4;    // Delay in milliseconds between pot samples
const int delayBetweenHIDReports = 5; // Additional delay in milliseconds between HID reports

void setup()
{
    // Setup Buttons
    for (byte currentPinIndex = 0; currentPinIndex < numOfButtons; currentPinIndex++)
    {
        pinMode(buttonPins[currentPinIndex], INPUT_PULLUP);
        previousButtonStates[currentPinIndex] = HIGH;
        currentButtonStates[currentPinIndex] = HIGH;
    }

    // Setup Hat Switches
    for (byte currentPinIndex = 0; currentPinIndex < numOfHats * 4; currentPinIndex++)
    {
        pinMode(hatPins[currentPinIndex], INPUT_PULLUP);
        previousHatStates[currentPinIndex] = HIGH;
        currentHatStates[currentPinIndex] = HIGH;
    }

    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setAutoReport(true);
    bleGamepadConfig.setButtonCount(numOfButtons);
    bleGamepadConfig.setHatSwitchCount(numOfHats);
    bleGamepad.begin(&bleGamepadConfig);

    // changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        // Deal with buttons
        for (byte currentIndex = 0; currentIndex < numOfButtons; currentIndex++)
        {
            currentButtonStates[currentIndex] = digitalRead(buttonPins[currentIndex]);

            if (currentButtonStates[currentIndex] != previousButtonStates[currentIndex])
            {
                if (currentButtonStates[currentIndex] == LOW)
                {
                    bleGamepad.press(physicalButtons[currentIndex]);
                }
                else
                {
                    bleGamepad.release(physicalButtons[currentIndex]);
                }
            }
        }

        // Update hat switch pin states
        for (byte currentHatPinsIndex = 0; currentHatPinsIndex < numOfHats * 4; currentHatPinsIndex++)
        {
            currentHatStates[currentHatPinsIndex] = digitalRead(hatPins[currentHatPinsIndex]);
        }

        // Update hats
        signed char hatValues[4] = {0, 0, 0, 0};

        for (byte currentHatIndex = 0; currentHatIndex < numOfHats; currentHatIndex++)
        {
            signed char hatValueToSend = 0;

            for (byte currentHatPin = 0; currentHatPin < 4; currentHatPin++)
            {
                // Check for direction
                if (currentHatStates[currentHatPin + currentHatIndex * 4] == LOW)
                {
                    hatValueToSend = currentHatPin * 2 + 1;

                    // Account for last diagonal
                    if (currentHatPin == 0)
                    {
                        if (currentHatStates[currentHatIndex * 4 + 3] == LOW)
                        {
                            hatValueToSend = 8;
                            break;
                        }
                    }

                    // Account for first 3 diagonals
                    if (currentHatPin < 3)
                    {
                        if (currentHatStates[currentHatPin + currentHatIndex * 4 + 1] == LOW)
                        {
                            hatValueToSend += 1;
                            break;
                        }
                    }
                }
            }

            hatValues[currentHatIndex] = hatValueToSend;
        }

        // Set hat values
        bleGamepad.setHats(hatValues[0], hatValues[1], hatValues[2], hatValues[3]);
        

        // Update previous states to current states and send report
        if (currentButtonStates != previousButtonStates || currentHatStates != previousHatStates)
        {
            for (byte currentIndex = 0; currentIndex < numOfButtons; currentIndex++)
            {
                previousButtonStates[currentIndex] = currentButtonStates[currentIndex];
            }

            for (byte currentIndex = 0; currentIndex < numOfHats * 4; currentIndex++)
            {
                previousHatStates[currentIndex] = currentHatStates[currentIndex];
            }
            bleGamepad.sendReport();
        }

        int potXValues[numberOfPotSamples]; // Array to store pot readings
        int potXValue = 0;                  // Variable to store calculated pot reading average
        int potYValues[numberOfPotSamples]; // Array to store pot readings
        int potYValue = 0;                  // Variable to store calculated pot reading average

        // Populate readings
        for (int i = 0; i < numberOfPotSamples; i++)
        {
            potXValues[i] = analogRead(xpin);
            potXValue += potXValues[i];
            potYValues[i] = analogRead(ypin);
            potYValue += potYValues[i];
            delay(delayBetweenSamples);
        }

        // Calculate the average
        potXValue = potXValue / numberOfPotSamples;
        potYValue = potYValue / numberOfPotSamples;

        // Map analog reading from 0 ~ 4095 to 32737 ~ 0 for use as an axis reading
        int adjustedXValue = map(potXValue, 0, 4095, 32737, 0);
        int adjustedYValue = map(potYValue, 0, 4095, 32737, 0);

        // Update axes and auto-send report
        bleGamepad.setX(adjustedXValue);
        bleGamepad.setY(adjustedYValue);
        delay(delayBetweenHIDReports);
        bleGamepad.sendReport();
    }
}