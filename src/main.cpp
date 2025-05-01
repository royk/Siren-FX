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
  Serial.print("Sent MIDI CC: ");
  Serial.print(controller);
  Serial.print(", value: ");
  Serial.println(value);
}

void startStereoLFO()
{
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, 10);
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SINE, 127);
  sendMidiCC(OUT_CHANNEL, OUT_CC_B_LFO_DELAY, 12);
}

void stopStereoLFO() {
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_LFO_SPEED, 0);
}

void brokenCable(float dropChance = 0.5)
{
  
  unsigned long now = millis();
  if (now - lastBrokenCableTime >= brokenCableHold)
  {
    lastValue = random(1000) >= (dropChance * 1000) ? 127 : 0;
    brokenCableHold = random(1, lastValue == 127 ? 300 : 50);
    lastBrokenCableTime = now;
  }
  sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, lastValue);
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

  Serial.println("Waiting for MIDI data...");
}

void loop()
{
  // Check if MIDI data is available
  if (midiSerial.available() > 0)
  {
    byte data1 = midiSerial.read();
    lastMidiReceiveTime = millis();

    // Log what we received
    Serial.print("MIDI status byte: 0x");
    Serial.println(data1);
    if (data1 == IN_CC_SWITCH_1)
    {
      activeMode = 0;
    }
    else if (data1 == IN_CC_SWITCH_2)
    {
      activeMode = 1;
    }
    else if (data1 == IN_CC_SUS_DOWN)
    {
      noteOn = true;
      digitalWrite(ledPin, HIGH);
    }
    else if (data1 == IN_CC_SUS_UP)
    {
      noteOn = false;
      digitalWrite(ledPin, LOW);
      stopStereoLFO();
      sendMidiCC(OUT_CHANNEL, OUT_CC_AB_VOLUME, 127);
      lfoActive = false;
    }
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
      if (!lfoActive)
      {
        startStereoLFO();
        lfoActive = true;
      }
    }
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