// SWITCH 1: Broken Cable
// SWITCH 2: Sine LFO
// SWITCH 3: Ramp LFO
// SWITCH 4: Volume control

#include <Arduino.h>
#include <SoftwareSerial.h>

// Create a software serial port for MIDI (RX, TX)
// Check your MIDI shield to make sure these pins connect to MIDI input
// For Olimex MIDI Shield, RX might be pin 0 or another pin
SoftwareSerial midiSerial(0, 1); // RX, TX

// LED pin
const int ledPin = LED_BUILTIN;

// SIREN CC codes

const byte OUT_CC_A_VOLUME = 29;
const byte OUT_CC_B_VOLUME = 40;
const byte OUT_CC_AB_VOLUME = 69;

const byte OUT_CC_AB_LFO_SPEED = 70; // 0 - off, 10-127: 100ms to 1.27s
const byte OUT_CC_AB_LFO_SINE = 61;  // 0-127: depth
const byte OUT_CC_AB_LFO_RAMP = 65;
const byte OUT_CC_B_LFO_DELAY = 51;  // 0-24 (1/24th of a duration)

byte OUT_CHANNEL = 0; // MIDI channel for LFO output (0-15)

// Hotone CC codes
const byte IN_CC_SWITCH_1 = 27;
const byte IN_CC_SWITCH_2 = 28;
const byte IN_CC_SWITCH_3 = 29;
const byte IN_CC_SWITCH_4 = 30;
const byte IN_CC_EXP = 31;
const byte IN_CC_SUS_DOWN = 32;
const byte IN_CC_SUS_UP = 33;
bool noteOn = false;

// Track when we last received MIDI data
unsigned long lastMidiReceiveTime = 0;

unsigned int counter = 0;


byte lastValue = 0;

byte activeMode = 0;

byte targetVolume = 127;
byte lastVolume = 127;

bool lfoActive = false;

unsigned long lastBrokenCableTime = 0;
unsigned long brokenCableHold = 100;

// Function to send MIDI Control Change message
void sendMidiCC(byte channel, byte controller, byte value)
{
  // MIDI status byte: 0xB0 + channel
  midiSerial.write(0xB0 | (channel & 0x0F));
  midiSerial.write(controller & 0x7F);
  midiSerial.write(value & 0x7F);

  // Debug message
  // Serial.print("Sent MIDI CC: ");
  // Serial.print(controller);
  // Serial.print(", value: ");
  // Serial.println(value);
}

void startSineLFO()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, targetVolume);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SINE, 127);
  sendMidiCC(OUT_CHANNEL, OUT_CC_B_LFO_DELAY, 12);
}

void startRampLFO() {
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, targetVolume);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_RAMP, 127);
}

void stopLFOs() {
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, 0);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_RAMP, 0);
}

void brokenCable(float dropChance = 0.7)
{
  
  unsigned long now = millis();
  if (now - lastBrokenCableTime >= brokenCableHold)
  {
    lastValue = random(1000) >= (dropChance * 1000) ? targetVolume : 0;
    brokenCableHold = random(1, lastValue == targetVolume ? 150 : 30);
    lastBrokenCableTime = now;
  }
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, lastValue);
}

// Function to map one range to another
long mapRange(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup()
{
  // Set up hardware serial for debug output
  Serial.begin(115200);
  Serial.println("MIDI Debug Started");

  // Set up software serial at MIDI baud rate
  midiSerial.begin(31250);

  // Initialize LED pin
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  stopLFOs();
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);

  Serial.println("Waiting for MIDI data...");
}

void loop()
{
  // Check if MIDI data is available
  if (midiSerial.available() > 0)
  {
    byte data = midiSerial.read();
    lastMidiReceiveTime = millis();
    
    // Log the raw data
    Serial.print("MIDI data: ");
    Serial.print(data);
    Serial.print(" (0x");
    Serial.print(data, HEX);
    Serial.println(")");


    bool modeSwitched = false;
    
    // Handle each control based on the direct byte value
    if (data == IN_CC_SWITCH_1)
    {
      Serial.println("Switch 1 activated");
      if (noteOn && activeMode == 0) {
        noteOn = false;
      } else {
        noteOn = true;
        modeSwitched = true;
      }
      activeMode = 0;
    }
    else if (data == IN_CC_SWITCH_2)
    {
      Serial.println("Switch 2 activated");
      if (noteOn && activeMode == 1) {
        noteOn = false;
      } else {
        noteOn = true;
        modeSwitched = true;
      }
      activeMode = 1;
    } 
    else if (data == IN_CC_SWITCH_4)
    {
      Serial.println("Switch 3 activated");
      if (noteOn && activeMode == 2) {
        noteOn = false;
      } else {
        noteOn = true;
        modeSwitched = true;
      }
      activeMode = 2;
    }
    else if (data == IN_CC_SWITCH_3)
    {
      Serial.println("Switch 4 activated");
      if (noteOn && activeMode == 3) {
        noteOn = false;
      } else {
        noteOn = true;
        modeSwitched = true;
      }
      activeMode = 3;
    }
    
   if (!noteOn || modeSwitched)
    {
      Serial.println("Sustain released");
      digitalWrite(ledPin, LOW);
      stopLFOs();
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      lfoActive = false;
    }
    // Check for expression pedal - seems to work differently, might need a delay
    if (data == IN_CC_EXP) {
      // Wait for the value byte to be available
      delay(5); // Short delay to ensure the next byte arrives
      if (midiSerial.available() > 0) {
        byte value = midiSerial.read();
        // Map from pedal range (31-127) to full MIDI range (0-127)
        targetVolume = 127- mapRange(value, 31, 127, 0, 127);
        Serial.print("Expression pedal value: ");
        Serial.println(targetVolume);
        Serial.print("Raw value: ");
        Serial.println(value);
      }
    }
  }
  
  // Handle active modes and other logic
  if (activeMode == 2) {
    sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, targetVolume);
  }
  if (noteOn)
  {
    lastMidiReceiveTime = millis();
    if (activeMode == 0)
    {
      brokenCable();
    }
    else if (activeMode == 1)
    {
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      if (!lfoActive)
      {
        startSineLFO();
        lfoActive = true;
      }  else {
        if (lastVolume != targetVolume) {
          sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, mapRange(targetVolume, 0, 127, 10, 127));
        }
      }
    }
    else if (activeMode == 3)
    {
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      
      if (!lfoActive)
      {
        startRampLFO();
        lfoActive = true;
      } else {
        if (lastVolume != targetVolume) {
          sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, mapRange(targetVolume, 0, 127, 10, 127));
        }
      }
    }
    lastVolume = targetVolume;
  }
  else
  {
    // sendMidiCC(lfoChannel, 70, 0);
    // Blink LED every second if no MIDI data received recently
    if (millis() - lastMidiReceiveTime > 5000)
    {
      static unsigned long lastBlinkTime = 0;
      if (millis() - lastBlinkTime > 1000)
      {
        lastBlinkTime = millis();
        digitalWrite(ledPin, !digitalRead(ledPin));
        // Serial.println("Blink - waiting for MIDI");
      }
    }
  }
}