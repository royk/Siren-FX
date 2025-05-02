// SWITCH 1: Broken Cable
// SWITCH 2: A/B control (expression pedal changes balance between A and B)
// SWITCH 3: Stereo Ramp LFO
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
const byte OUT_CC_B_LFO_DELAY = 51; // 0-24 (1/24th of a duration)

const byte LFO_MIN_SPEED = 10;
const byte LFO_MAX_SPEED = 127;

byte OUT_CHANNEL = 0; // MIDI channel for LFO output (0-15)

// Hotone CC codes
const byte IN_CC_SWITCH_1 = 27;
const byte IN_CC_SWITCH_2 = 28;
const byte IN_CC_SWITCH_3 = 29;
const byte IN_CC_SWITCH_4 = 30;
const byte IN_CC_EXP = 31;
bool noteOn = false;

// Modes
const byte MODE_BROKEN_CABLE = 0;
const byte MODE_AB_CONTROL = 1;
const byte MODE_RAMP_LFO = 2;
const byte MODE_VOLUME_CONTROL = 3;

unsigned int counter = 0;



byte activeMode = 0;

byte targetVolume = 127;
byte lastVolume = 127;

bool lfoActive = false;

unsigned long brokenCableHold_A = 100;
unsigned long brokenCableHold_B = 100;
boolean brokenCableState_A = false;
boolean brokenCableState_B = false;

// Function to map one range to another
long mapRange(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

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

long mapExpToLFOSpeed(byte expValue)
{
  return mapRange(expValue, 0, 127, LFO_MIN_SPEED, LFO_MAX_SPEED);
}

void startSineLFO()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, mapExpToLFOSpeed(targetVolume));
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SINE, 127);
  sendMidiCC(OUT_CHANNEL, OUT_CC_B_LFO_DELAY, 12);
  lfoActive = true;
}

void startRampLFO()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, mapExpToLFOSpeed(targetVolume));
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_RAMP, 127);
  sendMidiCC(OUT_CHANNEL, OUT_CC_B_LFO_DELAY, 12);
  lfoActive = true;
}

void stopLFOs()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, 0);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_RAMP, 0);
  lfoActive = false;
}

//  —————————————————————————————
//  Global state and config
//  —————————————————————————————
int currentVolume = 0;         // last sent CC value


// hold values in frames
const int onMinHold = 10;  //
const int onMaxHold = 600000; //
const int offMinHold = 10;  //
const int offMaxHold = 80000; //

//  —————————————————————————————
//  Smoothly ramp CC from start→end over durationMs
//  —————————————————————————————
void fadeTo(byte outCC, int start, int end, int durationMs)
{
  // int steps = abs(end - start);
  // if (steps == 0)
  //   return;
  // int delayPerStep = durationMs / steps;
  // int dir = (end > start) ? 1 : -1;
  // for (int v = start; v != end; v += dir)
  // {
  //   sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, v);
  //   delay(delayPerStep);
  // }
  // sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, end);
  sendMidiCC(OUT_CHANNEL, outCC, end);
}

//  —————————————————————————————
//  Call this *every* loop to drive the stutter
//  —————————————————————————————
void brokenCable(unsigned long &holdTime, boolean &cableState, byte outputCC)
{
  // countdown until next toggle
  if (--holdTime <= 0)
  {
    cableState = !cableState;

    if (cableState)
    {
      // → entering "ON" state
      // Generate a value from 0.0 to 1.0, square it to bias toward smaller values
      // This makes small values much more likely than large ones
      float randomBias = random(0, 10000) / 10000.0;
      randomBias = randomBias * randomBias; // Square it to make small values more likely
      
      // Apply the bias to the range
      int range = onMaxHold - onMinHold;
      holdTime = onMinHold + (int)(randomBias * range);
      Serial.print("Broken cable hold ON: ");
      Serial.println(holdTime);
      // choose a slightly random on-level (40–100% of target)
      int minVol = targetVolume * 0.4;
      int maxVol = targetVolume;
      int newVol = random(minVol, maxVol + 1);
      // fade in over ~50ms
      fadeTo(outputCC, currentVolume, newVol, 50);
      currentVolume = newVol;
    }
    else
    {
      // → entering "OFF" state
      // Also apply nonlinear distribution to OFF state
      float randomBias = random(0, 10000) / 10000.0;
      randomBias = randomBias * randomBias;
      int range = offMaxHold - offMinHold;
      holdTime = offMinHold + (int)(randomBias * range);
      
      Serial.print("Broken cable hold OFF: ");
      Serial.println(holdTime);
      // quick fade‐out over ~20ms
      fadeTo(outputCC, currentVolume, 0, 20);
      currentVolume = 0;
    }
  }
}

void brokenCableA()
{
  brokenCable(brokenCableHold_A, brokenCableState_A, OUT_CC_A_VOLUME);
}

void brokenCableB()
{
  brokenCable(brokenCableHold_B, brokenCableState_B, OUT_CC_B_VOLUME);
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

bool checkModeSwitch(byte mode)
{
  bool modeSwitched = false;
  if (noteOn && activeMode == mode)
  {
    noteOn = false;
  }
  else
  {
    noteOn = true;
    modeSwitched = true;
  }
  return modeSwitched;
}

void loop()
{
  // Check if MIDI data is available
  if (midiSerial.available() > 0)
  {
    byte data = midiSerial.read();

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
      modeSwitched = checkModeSwitch(MODE_BROKEN_CABLE);
      activeMode = MODE_BROKEN_CABLE;
    }
    else if (data == IN_CC_SWITCH_2)
    {
      Serial.println("Switch 2 activated");
      modeSwitched = checkModeSwitch(MODE_AB_CONTROL);
      activeMode = MODE_AB_CONTROL;
    }
    else if (data == IN_CC_SWITCH_3)
    {
      Serial.println("Switch 3 activated");
      modeSwitched = checkModeSwitch(MODE_RAMP_LFO);
      activeMode = MODE_RAMP_LFO;
    }
    else if (data == IN_CC_SWITCH_4)
    {
      Serial.println("Switch 4 activated");
      modeSwitched = checkModeSwitch(MODE_VOLUME_CONTROL);
      activeMode = MODE_VOLUME_CONTROL;
    }

    if (!noteOn || modeSwitched)
    {
      if (!noteOn)
      {
        Serial.println("Mode stopped");
        digitalWrite(ledPin, LOW);
      }
      else
      {
        Serial.println("Mode switched");
      }
      stopLFOs();
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
    }
    if (noteOn)
    {
      digitalWrite(ledPin, HIGH);
    }
    // Check for expression pedal - seems to work differently, might need a delay
    if (data == IN_CC_EXP)
    {
      // Wait for the value byte to be available
      delay(5); // Short delay to ensure the next byte arrives
      if (midiSerial.available() > 0)
      {
        byte value = midiSerial.read();
        // Map from pedal range (31-127) to full MIDI range (0-127)
        targetVolume = 127 - mapRange(value, 31, 127, 0, 127);
        Serial.print("Expression pedal value: ");
        Serial.println(targetVolume);
        Serial.print("Raw value: ");
        Serial.println(value);
      }
    }
  }

  // Handle active modes and other logic
  if (activeMode == MODE_VOLUME_CONTROL)
  {
    sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, targetVolume);
  }
  if (noteOn)
  {
    if (activeMode == MODE_AB_CONTROL)
    {
      sendMidiCC(OUT_CHANNEL, OUT_CC_A_VOLUME, targetVolume);
      // 121 - due to the way the pedal is limited
      sendMidiCC(OUT_CHANNEL, OUT_CC_B_VOLUME, 121 - targetVolume);
    }
    if (activeMode == MODE_BROKEN_CABLE)
    {
      brokenCableA();
      brokenCableB();
    }
    else if (activeMode == MODE_RAMP_LFO)
    {
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      if (!lfoActive)
      {
        startRampLFO();
      }
      else
      {
        if (lastVolume != targetVolume)
        {
          sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, mapExpToLFOSpeed(targetVolume));
        }
      }
    }
    lastVolume = targetVolume;
  }
}