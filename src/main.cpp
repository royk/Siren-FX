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
const byte MODE_SINE_LFO = 1;
const byte MODE_RAMP_LFO = 2;
const byte MODE_VOLUME_CONTROL = 3;

unsigned int counter = 0;

boolean brokenCableState = false;

byte activeMode = 0;

byte targetVolume = 127;
byte lastVolume = 127;

bool lfoActive = false;

unsigned long brokenCableHold = 100;

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
  lfoActive = true;
}

void stopLFOs()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, 0);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_RAMP, 0);
  lfoActive = false;
}

void brokenCable(float dropChance = 0.7)
{

  if (brokenCableHold == 0)
  {
    brokenCableState = !brokenCableState;
    brokenCableHold = random(1, brokenCableState ? 300 : 10);
    Serial.print("Broken cable hold: ");
    Serial.println(brokenCableHold);
  }
  brokenCableHold--;
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, brokenCableState ? targetVolume : 0);
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
      modeSwitched = checkModeSwitch(MODE_SINE_LFO);
      activeMode = MODE_SINE_LFO;
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
    if (activeMode == MODE_BROKEN_CABLE)
    {
      brokenCable();
    }
    else if (activeMode == MODE_SINE_LFO || activeMode == MODE_RAMP_LFO)
    {
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      if (!lfoActive)
      {
        if (activeMode == MODE_SINE_LFO)
        {
          startSineLFO();
        }
        else
        {
          startRampLFO();
        }
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