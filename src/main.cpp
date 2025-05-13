// AB control

// DEFAULT: AB on Volume control. Exp = volume
// SWITCH 1: A on, B off
// SWITCH 2: B on, A off,
// SWITCH 3: Broken cable (mono). Exp = rate of brokenness

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

byte OUT_CHANNEL = 0; // MIDI channel for LFO output (0-15)

// In CC codes
const byte IN_STATUS_BYTE = 176;
const byte IN_CC_SWITCH_1 = 27;
const byte IN_CC_SWITCH_2 = 28;
const byte IN_CC_SWITCH_3 = 29;
const byte IN_CC_SWITCH_4 = 30;
const byte IN_CC_EXP = 31;
bool noteOn = false;

const byte BYTE_STATUS = 0;
const byte BYTE_CC = 1;
const byte BYTE_VALUE = 2;

byte nextExpectedByte = BYTE_STATUS;

byte currentCC = 0;
byte currentValue = 0;

// Modes
const byte MODE_BROKEN_CABLE = 0;
const byte MODE_A_ON_B_OFF = 1;
const byte MODE_B_ON_A_OFF = 2;
const byte MODE_VOLUME_CONTROL = 4;

byte activeMode = MODE_VOLUME_CONTROL;

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
}

void determineMode()
{
  if (currentCC == IN_CC_SWITCH_1 || currentCC == IN_CC_SWITCH_2 || currentCC == IN_CC_SWITCH_3)
  {
    if (currentValue == 1)
    {
      activeMode = MODE_VOLUME_CONTROL;
      return;
    }
    if (currentCC == IN_CC_SWITCH_1)
    {
      activeMode = MODE_A_ON_B_OFF;
    }
    else if (currentCC == IN_CC_SWITCH_2)
    {
      activeMode = MODE_B_ON_A_OFF;
    }
    else if (currentCC == IN_CC_SWITCH_3)
    {
      activeMode = MODE_BROKEN_CABLE;
    }
  }
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

  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);

  Serial.println("Waiting for MIDI data...");
}

void loop()
{
  // Check if MIDI data is available
  if (midiSerial.available() > 0)
  {
    byte data = midiSerial.read();
    Serial.print(nextExpectedByte);
    Serial.print(" ");
    Serial.println(data);
    if (nextExpectedByte == BYTE_STATUS || data == IN_STATUS_BYTE)
    {
      nextExpectedByte = BYTE_CC;
    }
    else if (nextExpectedByte == BYTE_CC)
    {
      currentCC = data;
      nextExpectedByte = BYTE_VALUE;
    }
    else if (nextExpectedByte == BYTE_VALUE)
    {

      if (currentCC == IN_CC_EXP || currentCC == IN_CC_SWITCH_1 || currentCC == IN_CC_SWITCH_2 || currentCC == IN_CC_SWITCH_3)
      {
        if (data <= 127)
        {
          currentValue = data;
        }
      }
      nextExpectedByte = BYTE_STATUS;
    }
  }
  determineMode();
  if (activeMode == MODE_A_ON_B_OFF)
  {
    Serial.println("A on, B off");
    sendMidiCC(OUT_CHANNEL, OUT_CC_A_VOLUME, 127);
    sendMidiCC(OUT_CHANNEL, OUT_CC_B_VOLUME, 0);
  }
  else if (activeMode == MODE_B_ON_A_OFF)
  {
    Serial.println("B on, A off");
    sendMidiCC(OUT_CHANNEL, OUT_CC_A_VOLUME, 0);
    sendMidiCC(OUT_CHANNEL, OUT_CC_B_VOLUME, 127);
  }
  else
  {
    sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, currentValue);
  }
}