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

// include our local header file !

#include "alarmclock.h"


void setup()
{
 	int attempt = 0;

 	// Set up the sound pin
 	pinMode(SOUND_PIN, OUTPUT);

 	Serial.begin(115200);

 	// start EEPROM
	EEPROM.begin(512);
    // load the alarm from the system
    g_AlarmTimeHours = EEPROM.read(ALARM_HOURS_ADDR);
    g_AlarmTimeMinutes = EEPROM.read(ALARM_MINUTES_ADDR);
    g_AlarmTime = g_AlarmTimeHours * 100 + g_AlarmTimeMinutes;
    
    Serial.println("Loaded Alarm data : ");
    Serial.print("Hours ");
    Serial.println(g_AlarmTimeHours);
    Serial.print("Minutes ");
    Serial.println(g_AlarmTimeMinutes);

    g_DisplayBrightness = EEPROM.read(BRIGHTNESS_ADDR);	

    Serial.print("Brightness ");
    Serial.println(g_DisplayBrightness);

    if(!g_DisplayBrightness) g_DisplayBrightness = 1;

 	display.setBrightness(g_DisplayBrightness); //set the diplay to maximum brightness
	timeVal = 0;

 	// attempt connection 
 	display.setSegments(SEG_CONN);

   	//WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    wifiManager.autoConnect("digiclock2");

    beep(NOTE_F7, 100, 100, 5);
 	// Manually connect to WiFi
    //connectToWiFi();

    // progress if wifi is available or has been set up

	Serial.println("Starting UDP");
	udp.begin(localPort);
	Serial.print("Local port: ");
	Serial.println(udp.localPort());

	while(attempt < MAX_ATTEMPTS_FOR_TIME) {
		timeVal = getTime();
		if(timeVal) {// got time !
			beep(NOTE_F7, 100, 100, 5);
			break;
		}

		attempt++; 
	}

	Serial.print("Get time value : ");
	Serial.println(timeVal);


	if(timeVal > 0) {
		setTime(timeVal);
	} else { 
		display.setSegments(SEG_ERR);
	}

	// set up sync provider for getting time every 300 mins
	setSyncProvider(getTime);
	setSyncInterval(SYNC_TIME); 

	t_showDateCounter = t_showMsgCounter = t_showAlarmBlink = millis();
    
    // watchdog timer
    t_WDT = t_AlarmTimer = millis();

    // alarm flag
    g_AlarmFlag = g_AlarmRun = true;

    // When the button is first pressed, call the function onButtonPressed
    resetButton.onPress(onButtonPressed);
    // Once the button has been held for 2 seconds (2000ms) call onButtonHeld. Call it again every 0.5s (1000) until it is let go
    resetButton.onHoldRepeat(2000, 1000, onButtonHeld);
    // When the button is released, call onButtonReleased
    resetButton.onRelease(onButtonReleased);
}
 
 
void loop()
{
	unsigned int t = hour() * 100 + minute();

	// have a beep function at the end of 30 minutes and 1 hour
	// dont do it between 10 PM - 7 AM

	// beep every 30 minutes once & 2 times every hour
	if(t >= 600 && t <= 2200) {
		if(minute() == 29 && second() == 59) {
			beep(NOTE_AS4, 500, 500, 1);
		}	

		if(minute() == 59 && second() == 59) {
			beep(NOTE_AS4, 500, 250, 2);
		}	
	}

	// its alarm time ! need to ensure it does not switch on if  button pressed
	// it will run maximum for 60 seconds, before which t will change 
	if(t == g_AlarmTime && g_AlarmRun && g_ClockMode == MODE_NORMAL) {
		if(millis() - t_AlarmTimer > 300) {
			g_AlarmFlag = !g_AlarmFlag;
			t_AlarmTimer = millis();
		}

		if(g_AlarmFlag) {
			tone(SOUND_PIN, NOTE_G7);
			display.setBrightness(0);
		} else {
			noTone(SOUND_PIN);	
			display.setBrightness(g_DisplayBrightness);
		}
	}

	// reset the flag to run the alarm the next minute after alarm
	// if the alarm was not reset at all, no problem keep it true
	if(t > g_AlarmTime && !g_AlarmRun) {
		g_AlarmRun = true;
	}

	if(g_ClockMode != MODE_NORMAL) {
		// time to switch back to normal mode if its greater than WATCH_DOG_MAX_TIME
		if(millis() - t_WDT > WATCH_DOG_MAX_TIME) {
			t_WDT = millis();
			g_ClockMode = MODE_NORMAL;
			g_AlarmSettingMode = MODE_ALARM_NO_SETTING; 
		}
	}

	if(g_ClockMode == MODE_NORMAL) {
		if(millis() - t_showDateCounter > 500) {
			display.showNumberDecEx(t, second()%2?0xff:0x00, true);		
			t_showDateCounter = millis();
			t_WDT = millis();
		}		
	} else if(g_ClockMode == MODE_ALARM_SETTING) {
		//display.setSegments(SEG_ALAR);
		if(millis() - t_showAlarmBlink > 200) {
			if(g_AlarmSettingMode == MODE_ALARM_MIN_SETTING) {
				// Flash the minute digits when its being set up
				if(g_AlarmBlinkFlag) {
					display.showNumberDecEx(g_AlarmTime, 0xff, true);		
				} else {
					// blank out the minutes to simulate flashing 
					uint8_t SEG_DISPLAY[] = {
							display.encodeDigit(g_AlarmTime/1000),
							display.encodeDigit((g_AlarmTime%1000)/100),
							0,
							0
					};
					display.setSegments(SEG_DISPLAY);
					//display.showNumberDecEx(g_AlarmTime/100, 0xff, false, 0);
				}
			} else if(g_AlarmSettingMode == MODE_ALARM_HOUR_SETTING) {
				// Flash the hour digits when its being set up
				if(g_AlarmBlinkFlag) {
					display.showNumberDecEx(g_AlarmTime, 0xff, true);		
				} else {
					// blank out the hours to simulate flashing 
					uint8_t SEG_DISPLAY[] = {
							0,
							0,
							display.encodeDigit((g_AlarmTime%100)/10),
							display.encodeDigit(g_AlarmTime%10)
					};
					display.setSegments(SEG_DISPLAY);					
				}				
			}
			g_AlarmBlinkFlag = !g_AlarmBlinkFlag;
			t_showAlarmBlink = millis();
		}		
	} else if(g_ClockMode == MODE_BRIGHTNESS_SETTING) {		
		display.setSegments(SEG_BRIT);
	}

	resetButton.update();	
}

// btn is a reference to the button that fired the event. That means you can use the same event handler for many buttons
void onButtonPressed(Button& btn){
    Serial.print("button pressed, mode : ");
    Serial.println(g_ClockMode);
    Serial.print("Alarm mode : ");
    Serial.println(g_AlarmSettingMode);
    Serial.print("Alarm time : ");
    Serial.println(g_AlarmTime);
    Serial.print("g_AlarmRun : ");
    Serial.println(g_AlarmRun);    

    // on press will result in watchdog being reset
    // dirty flagging 
    if(g_ClockMode != MODE_NORMAL)
    	t_WDT = millis(); 

    if(g_ClockMode == MODE_ALARM_SETTING) {
    	if(g_AlarmSettingMode == MODE_ALARM_MIN_SETTING) {
    		g_AlarmTimeMinutes = (g_AlarmTimeMinutes + 1) % 60;
    	} else if(g_AlarmSettingMode == MODE_ALARM_HOUR_SETTING) {
    		g_AlarmTimeHours = (g_AlarmTimeHours + 1) % 24;
    	}

    	g_AlarmTime = g_AlarmTimeHours * 100 + g_AlarmTimeMinutes;

    	EEPROM.write(ALARM_MINUTES_ADDR, (int)g_AlarmTimeMinutes);
    	EEPROM.write(ALARM_HOURS_ADDR, (int)g_AlarmTimeHours);

    	// reset alarm to true 
    	g_AlarmRun = true;

		// commit to EEPROM
		if (EEPROM.commit()) {
			Serial.println("EEPROM successfully committed - alarm");
		} else {
			Serial.println("ERROR! EEPROM commit failed - alarm");
		}     	
    }

    if(g_ClockMode == MODE_BRIGHTNESS_SETTING) {
    	g_DisplayBrightness = (g_DisplayBrightness + 1) % 7;
    	display.setBrightness(g_DisplayBrightness);

		// commit to EEPROM
    	EEPROM.write(BRIGHTNESS_ADDR, (int)g_DisplayBrightness);
		if (EEPROM.commit()) {
			Serial.println("EEPROM successfully committed - BRIGHTNESS_ADDR");
		} else {
			Serial.println("ERROR! EEPROM commit failed - BRIGHTNESS_ADDR");
		}        	
    }
}

// duration reports back how long it has been since the button was originally pressed.
// repeatCount tells us how many times this function has been called by this button.
void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount){

	Serial.print("button has been held for ");
	Serial.print(duration);
	Serial.print(" ms; this event has been fired ");
	Serial.print(repeatCount);
	Serial.println(" times");

	if(repeatCount == 1) {
		if(g_ClockMode != MODE_NORMAL) {
			if(g_ClockMode == MODE_ALARM_SETTING) {
				if(g_AlarmSettingMode == MODE_ALARM_MIN_SETTING) {
					// go to hours setting after min setting
					g_AlarmSettingMode = MODE_ALARM_HOUR_SETTING;
				} else {
					g_ClockMode = MODE_NORMAL;
					g_AlarmSettingMode = MODE_ALARM_NO_SETTING;	
				}				
			}

			if(g_ClockMode == MODE_BRIGHTNESS_SETTING) {
				g_ClockMode = MODE_NORMAL;
				g_AlarmSettingMode = MODE_ALARM_NO_SETTING;
			}
			
			Serial.print("Set clock mode to :");
			Serial.println(g_ClockMode);

			Serial.print("Set alarm mode to :");
			Serial.println(g_AlarmSettingMode);

		} else {
			g_ClockMode = MODE_ALARM_SETTING;
			g_AlarmSettingMode = MODE_ALARM_MIN_SETTING;
			Serial.println("Set mode to MODE_ALARM_SETTING");
		}
	}

	if(repeatCount == 3) {
		g_ClockMode = MODE_BRIGHTNESS_SETTING;
		Serial.println("Set mode to MODE_BRIGHTNESS_SETTING");
	}	

	tone(SOUND_PIN, NOTE_F7, 1000);
	delay(10);
	noTone(SOUND_PIN);	
}

// duration reports back the total time that the button was held down
void onButtonReleased(Button& btn, uint16_t duration){
 
	Serial.print("button released after ");
	Serial.print(duration);
	Serial.println(" ms");	

	// release button will result in watchdog being reset
    // dirty flagging 
    if(g_ClockMode != MODE_NORMAL)
    	t_WDT = millis(); 

    // reset the alarm if the button is pressed and released
    if((hour() * 100 + minute()) == g_AlarmTime && g_AlarmRun) {
    	g_AlarmRun = false;
		tone(SOUND_PIN, NOTE_F7);
		delay(100);
		noTone(SOUND_PIN);	
		display.setBrightness(g_DisplayBrightness);    	
    }
}

time_t getTime() 
{
	display.setSegments(SEG_SYNC);

	WiFi.hostByName(ntpServerName, timeServerIP); 
	sendNTPpacket(timeServerIP); // send an NTP packet to a time server

	// wait to see if a reply is available
	delay(3000);

	int cb = udp.parsePacket();

	// introduce a back up for time as its critical for the overall operation
	if (!cb) {
		Serial.println("no packet yet, trying timezonedb as back up!");
		return getTimeBackup();
		// even if this fails, you must connect the system to an RTC module
		// set up time and obtain it. 
	}
	else {
		Serial.print("packet received, length=");
		Serial.println(cb);
		// We've received a packet, read the data from it
		udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

		//the timestamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, esxtract the two words:
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;

		Serial.print("Seconds since Jan 1 1900 = " );
		Serial.println(secsSince1900);

		// now convert NTP time into everyday time:
		Serial.print("Unix time = ");
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;
		// print Unix time:
		Serial.println(epoch);

		epoch += 5*60*60 + 30*60; // IST ahead by 5 hrs 30 mins

		return epoch;
	}	

}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


// wrapper API for getTimeFromTimeZoneDB
time_t getTimeBackup() 
{
	return getTimeFromTimeZoneDB(timezonedbhost, timezoneparams);
}

// connect to timezone db !
time_t getTimeFromTimeZoneDB(const char* host, const char* params) 
{
  Serial.print("Trying to connect to ");
  Serial.println(host);

  // Use WiFiClient for timezonedb 
  WiFiClient client;
  if (!client.connect(host, HTTP_PORT)) {
    Serial.print("Failed to connect to :");
    Serial.println(host);
    return 0;
  }

  Serial.println("connected !....");

  // send a GET request
  client.print(String("GET ") + timezoneparams + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "User-Agent: ESP8266\r\n" +
             "Accept: */*\r\n" +
             "Connection: close\r\n\r\n");

  // bypass HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Header: ");
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  // get the length component
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Body Length: ");
    Serial.println(line);    
      break;
  } 

  String line = "";

  // get the actual body, which has the JSON content
  while (client.connected()) {
    line = client.readStringUntil('\n');
    Serial.print( "Json body: ");
    Serial.println(line);    
    break;
  }   

  // Use Arduino JSON libraries parse the JSON object
  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(8)    // the root object has 8 elements
      + MAX_CONTENT_SIZE;    // additional space for strings

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

  JsonObject& root = jsonBuffer.parseObject(line);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return -1;
  }  

  Serial.println("Parsing a success ! -- ");

  // 'timestamp' has the exact UNIX time for the zone specified by zone param
  Serial.println((long)root["timestamp"]);

  return (time_t)root["timestamp"];
}

void beep(uint16_t note, uint16_t duration, uint16_t gap, uint16_t times)
{
	for(int i = 0; i < times; i++) {
		tone(SOUND_PIN, note);
		delay(duration);
		noTone(SOUND_PIN);
		delay(gap);
	}
}