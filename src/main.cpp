#include <Arduino.h>
#include <SoftwareSerial.h>

// Create a software serial port for MIDI (RX, TX)
// Check your MIDI shield to make sure these pins connect to MIDI input
// For Olimex MIDI Shield, RX might be pin 0 or another pin
SoftwareSerial midiSerial(0, 1); // RX, TX

// LED pin
const int ledPin = LED_BUILTIN;
bool noteOn = false;

// LFO settings
const byte LFO_CC = 20;     // 20 is channel A volume
byte lfoChannel = 0;       // MIDI channel for LFO output (0-15)
byte lfoMin = 0;           // Minimum value for LFO
byte lfoMax = 127;         // Maximum value for LFO
unsigned long lfoRate = 250; // LFO rate in milliseconds
bool lfoState = false;     // Current state of LFO (high/low)
unsigned long lastLfoTime = 0; // Last time LFO was updated

// Track when we last received MIDI data
unsigned long lastMidiReceiveTime = 0;

// Function to send MIDI Control Change message
void sendMidiCC(byte channel, byte controller, byte value) {
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

void setup() {
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

void loop() {
  // Check if MIDI data is available
  if (midiSerial.available() > 0) {
    byte statusByte = midiSerial.read();
    lastMidiReceiveTime = millis();
    
    // Log what we received
    Serial.print("MIDI status byte: 0x");
    Serial.println(statusByte, HEX);
    
    // Note On: 0x9n where n is the channel (0-F)
    if ((statusByte & 0xF0) == 0x90) {
      // Wait for the next two bytes (pitch and velocity)
      unsigned long waitStart = millis();
      while (midiSerial.available() < 2) {
        // Timeout after 100ms
        if (millis() - waitStart > 100) {
          Serial.println("Timeout waiting for pitch/velocity");
          return;
        }
      }
      
      byte pitch = midiSerial.read();
      byte velocity = midiSerial.read();
      
      Serial.print("Note On - Pitch: ");
      Serial.print(pitch);
      Serial.print(", Velocity: ");
      Serial.println(velocity);
      
      // If velocity is 0, it's actually a note off
      if (velocity > 0) {
        noteOn = true;
        digitalWrite(ledPin, HIGH);
        Serial.println("LED ON");
        
        // Store the channel for LFO output
        lfoChannel = statusByte & 0x0F;
        
        // Initialize LFO state
        lfoState = false;
        lastLfoTime = millis();
      } else {
        noteOn = false;
        digitalWrite(ledPin, LOW);
        sendMidiCC(lfoChannel, LFO_CC, lfoMax);
        Serial.println("LED OFF (velocity 127)");
      }
    }
    
    // Note Off: 0x8n where n is the channel (0-F)
    else if ((statusByte & 0xF0) == 0x80) {
      // Wait for the next two bytes (pitch and velocity)
      unsigned long waitStart = millis();
      while (midiSerial.available() < 2) {
        // Timeout after 100ms
        if (millis() - waitStart > 100) {
          Serial.println("Timeout waiting for pitch/velocity");
          return;
        }
      }
      
      byte pitch = midiSerial.read();
      byte velocity = midiSerial.read();
      
      Serial.print("Note Off - Pitch: ");
      Serial.print(pitch);
      Serial.print(", Velocity: ");
      Serial.println(velocity);
      
      noteOn = false;
      digitalWrite(ledPin, LOW);
      Serial.println("LED OFF");
    }
    else {
      // Handle other MIDI messages
      Serial.print("Other MIDI message: 0x");
      Serial.println(statusByte, HEX);
    }
  } else {
    // Print a dot every 3 seconds to show the program is running
    static unsigned long lastDotTime = 0;
    if (millis() - lastDotTime > 3000) {
      lastDotTime = millis();
      Serial.println(".");
    }
  }
  
  // Generate square wave LFO if a note is currently on
  if (noteOn) {
    if (millis() - lastLfoTime >= lfoRate) {
      lastLfoTime = millis();
      lfoState = !lfoState;
      
      // Send CC message with alternating values (square wave)
      byte lfoValue = lfoState ? lfoMax : lfoMin;
      sendMidiCC(lfoChannel, LFO_CC, lfoValue);
    }
  }
  
  // Blink LED every second if no MIDI data received recently
  if (millis() - lastMidiReceiveTime > 5000) {
    static unsigned long lastBlinkTime = 0;
    if (millis() - lastBlinkTime > 1000) {
      lastBlinkTime = millis();
      digitalWrite(ledPin, !digitalRead(ledPin));
      //Serial.println("Blink - waiting for MIDI");
    }
  }
} 