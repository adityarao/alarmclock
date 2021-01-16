/****
MIT License

Copyright (c) 2020 Aditya Ramamurthy Rao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Alarm clock using a TM1637 4 7-segment display + buzzer + single button to set alarms. 
Usage: Long press to set the alarm and set brightness

****/

#include <ESP8266WiFi.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

// controls the 7 segment display
#include <TM1637Display.h>

// enables UDP
#include <WiFiUdp.h>

// time management libraries - uses millis() to manage time
#include <Time.h>
#include <TimeLib.h>

// JSON management library 
#include <ArduinoJson.h>

// button management library
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>

// persist
#include <EEPROM.h>

// 
#define MAX_ATTEMPTS_FOR_TIME 5
#define MAX_CONNECTION_ATTEMPTS 30

// http port 
#define HTTP_PORT 80

#define SOUND_PIN D7
#define BUTTON_PIN D3

#define SYNC_TIME 180000

#define MODE_NORMAL 0
#define MODE_ALARM_SETTING 1
#define MODE_BRIGHTNESS_SETTING 2

#define MODE_ALARM_NO_SETTING 0
#define MODE_ALARM_MIN_SETTING 1
#define MODE_ALARM_HOUR_SETTING 2
#define MODE_ALARM_STATE_ON 3

// Watch dog timer to return to clock post entering setup 

#define WATCH_DOG_MAX_TIME 10000

// Max 60 seconds 
#define MAX_ALARM_TIME 600000

// locations for 

#define ALARM_HOURS_ADDR 0
#define ALARM_MINUTES_ADDR 1

#define BRIGHTNESS_ADDR 2

// tones
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978

// global to understand the current clock mode
uint16_t g_ClockMode = MODE_NORMAL; 

uint16_t g_AlarmSettingMode = MODE_ALARM_NO_SETTING; 

// alarm time, will be pulled
uint16_t g_AlarmTime = 2200;
uint16_t g_AlarmTimeHours = g_AlarmTime / 100;
uint16_t g_AlarmTimeMinutes = g_AlarmTime % 100;

boolean g_AlarmBlinkFlag = true;

// Brightness value 0x00 - 0xf
uint16_t g_DisplayBrightness = 0xa;


const size_t MAX_CONTENT_SIZE = 512;

// timezonedb host & URLs 
const char *timezonedbhost = "api.timezonedb.com";
// put your key in xxxx	
const char *timezoneparams = "/v2/get-time-zone?key=xxxx&by=zone&zone=Asia/Kolkata&format=json";
 
const int CLK = D2; //Set the CLK pin connection to the display
const int DIO = D1; //Set the DIO pin connection to the display

String localIP = "";

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

// 4 segment display 
TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

// Push button
PushButton resetButton = PushButton(BUTTON_PIN);

time_t timeVal; 

bool connectToWiFi(); // connect to WiFi
unsigned long sendNTPpacket(IPAddress&);
time_t getTime();
time_t getTimeFromTimeZoneDB(const char*, const char*);

unsigned long int t_SENSE_TEMP = 0;

unsigned long t_showDateCounter = 0, t_showMsgCounter = 0, t_showAlarmBlink = 0;

// watch dog timer to return to clock 
unsigned long t_WDT = 0; 
bool g_DirtyFlag = false; 

unsigned long t_AlarmTimer = 0; 
bool g_AlarmFlag = false;
bool g_AlarmRun = false;


const uint8_t SEG_CONN[] = {
	SEG_A | SEG_D | SEG_E | SEG_F,                   // C
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_C | SEG_E | SEG_G,           				 // n
};

const uint8_t SEG_ERR[] = {
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
	SEG_E | SEG_G,   						// r
	SEG_E | SEG_G,                         // r
	0    									
};

const uint8_t SEG_SYNC[] = {
	SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,   // S
	SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,  // y
	SEG_C | SEG_E | SEG_G,                  // n
	SEG_A | SEG_D | SEG_E | SEG_F           // C
};

const uint8_t SEG_GOOD[] = {
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,  // g
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G   		// d
};

const uint8_t SEG_DAY[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,  		// d
	SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
	SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,          // y
	0  		
};

const uint8_t SEG_NITE[] = {
	SEG_C | SEG_E | SEG_G,  						// n
	SEG_B | SEG_C ,  								// I
	SEG_D | SEG_E | SEG_F | SEG_G, 					// t
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G   		// E
};

const uint8_t SEG_DATE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,  		// d
	SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
	SEG_D | SEG_E | SEG_F | SEG_G, 					// t
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G   		// E
};


const uint8_t SEG_BRIT[] = {
	SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,   		// b
	SEG_E | SEG_G,  								// r
	SEG_B | SEG_C,				 					// I
	SEG_D | SEG_E | SEG_F | SEG_G 			   		// t
};


const uint8_t SEG_ALAR[] = {
	SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
	SEG_D | SEG_E | SEG_F,  						// L
	SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,	// A
	SEG_E | SEG_G 				 			   		// r
};



int g_melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int g_noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};



// btn is a reference to the button that fired the event. That means you can use the same event handler for many buttons
void onButtonPressed(Button& btn);

// duration reports back how long it has been since the button was originally pressed.
// repeatCount tells us how many times this function has been called by this button.
void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);

// duration reports back the total time that the button was held down
void onButtonReleased(Button& btn, uint16_t duration);



time_t getTime() ;

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address);


// wrapper API for getTimeFromTimeZoneDB
time_t getTimeBackup() ;

// connect to timezone db !
time_t getTimeFromTimeZoneDB(const char* host, const char* params) ;

// beep function
void beep(uint16_t note, uint16_t duration, uint16_t gap, uint16_t times);


