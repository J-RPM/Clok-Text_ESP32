/*
        A part for this code was created from the information of D L Bird 2019
                          See more at http://dsbird.org.uk
           https://www.youtube.com/watch?v=RTKQMuWPL5A&ab_channel=G6EJD-David

          Some routines were created from the information of Andreas Spiess
                        https://www.sensorsiot.org/
                  https://www.youtube.com/c/AndreasSpiess/

                                 --- oOo ---
                                
                              Modified by: J_RPM
                               http://j-rpm.com/
                        https://www.youtube.com/c/JRPM
                              November of 2020

               An OLED display is added to show the Time an Date,
                adding some new routines and modifying others.

                              >>> HARDWARE <<<
                  LIVE D1 mini ESP32 ESP-32 WiFi + Bluetooth
                https://es.aliexpress.com/item/32816065152.html
                
            HW-699 0.66 inch OLED display module, for D1 Mini (64x48)
               https://es.aliexpress.com/item/4000504485892.html

             4x Módulos de Control de pantalla Led 8x8 MAX7219 (ROTATE 0)
                https://es.aliexpress.com/item/33038259447.html

            8x8 MAX7219 Led Control Module, 4 in one screen  (ROTATE 90)
               https://es.aliexpress.com/item/4001296969309.html

                           >>> IDE Arduino <<<
                        Model: WEMOS MINI D1 ESP32
       Add URLs: https://dl.espressif.com/dl/package_esp32_index.json
                     https://github.com/espressif/arduino-esp32

 ____________________________________________________________________________________
*/
String HWversion = "(v1.41)"; //ROTATE 0
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>

#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <Adafruit_GFX.h>
//*************************************************IMPORTANT******************************************************************
#include "Adafruit_SSD1306.h" // Copy the supplied version of Adafruit_SSD1306.cpp and Adafruit_ssd1306.h to the sketch folder
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String CurrentTime, CurrentDate, nDay, webpage = "";
bool display_EU = true;
bool display_msg = false;
int matrix_speed = 25;

// Turn on debug statements to the serial output
#define DEBUG  1
#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// ESP32 -> Matrix -> ROTATE 90
//#define DIN_PIN 32   // ROTATE 90
//#define CS_PIN  25   // ROTATE 90   
//#define CLK_PIN 27   // ROTATE 90

// ESP32 -> Matrix -> ROTATE 0
#define DIN_PIN 27   //ROTATE 0
#define CS_PIN  25   //ROTATE 0 
#define CLK_PIN 32   //ROTATE 0

#define NUM_MAX 4
#include "max7219.h"
#include "fonts.h"

// Global message buffers shared by Wifi and Scrolling functions
#define BUF_SIZE  512
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;
const char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\r\n";

// Define the number of bytes you want to access (first is index 0)
#define EEPROM_SIZE 7

////////////////////////// MATRIX //////////////////////////////////////////////
#define MAX_DIGITS 20
bool _scroll = false;
bool display_date = true;
bool animated_time = true;
bool show_seconds = false;
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int dots = 1;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int h,m,s;
int dualChar = 0;
int brightness = 5;  //DUTY CYCLE: 11/32
String mDay;
long timeConnect;
//////////////////////////////////////////////////////////////////////////////

WiFiClient client;
const char* Timezone    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "es.pool.ntp.org";               // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int  gmtOffset_sec      = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 7200; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

WiFiServer server(80);
WebServer server2(80); 

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void setup() {
  
  Serial.begin(115200);
  timeConnect = millis();
 
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,brightness);

  // Test MATRIX
  sendCmdAll(CMD_DISPLAYTEST, 1);
  delay(500);
  sendCmdAll(CMD_DISPLAYTEST, 0);
 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15,0);   
  display.println(F("NTP"));
  display.setCursor(6,16);   
  display.println(F("TIME"));

  display.setTextSize(1);   
  display.setCursor(10,32);   
  display.println(HWversion);
  display.setCursor(10,40);   
  display.println(F("Sync..."));
  display.display();
  
  printStringWithShift((String("  NTP Time  ")+ HWversion).c_str(), matrix_speed);
  delay(2000);
  
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  WiFiManager wifiManager;
  display_AP_wifi();

  // A new OOB ESP32 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
  // wifiManager.resetSettings();
  // Then restart the ESP32 and connect your PC to the wireless access point called 'ESP32_AP' or whatever you call it below
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);
  
  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP32_AP" and waits in a blocking loop for configuration
  if (!wifiManager.autoConnect("ESP32_AP")) {
    PRINTS("\nFailed to connect and timeout occurred");
    display_AP_wifi();
    display_flash();
    reset_ESP32();
  }
  // At this stage the WiFi manager will have successfully connected to a network,
  // or if not will try again in 180-seconds
  //---------------------------------------------------------
  // 
  PRINT("\n>>> Connection Delay(ms): ",millis()-timeConnect);
  if(millis()-timeConnect > 30000) {
    PRINTS("\nTimeOut connection, restarting!!!");
    reset_ESP32();
  }
  
  // Print the IP address
  PRINT("\nUse this URL to connect -> http://",WiFi.localIP());
  PRINTS("/");
  display_ip();
  SetupTime();
  UpdateLocalTime();
  
  // Initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);

  // 0 - Display Status 
  display_EU = EEPROM.read(0);
  PRINT("\ndisplay_EU: ",display_EU);

  // 1 - Display Date  
  display_date = EEPROM.read(1);
  PRINT("\ndisplay_date: ",display_date);
  
  // 2 - Matrix Brightness  
  brightness = EEPROM.read(2);
  PRINT("\nbrightness: ",brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  
  // 3 - Animated Time  
  animated_time = EEPROM.read(3);
  PRINT("\nanimated_time: ",animated_time);

  // 4 - Show Seconds  
  show_seconds = EEPROM.read(4);
  PRINT("\nshow_seconds: ",show_seconds);

  // 5 - Speed Matrix (delay)  
  matrix_speed = EEPROM.read(5);
  PRINT("\nEEPROM_Speed: ",matrix_speed);
  if (matrix_speed < 10 || matrix_speed > 40) {
    matrix_speed = 25;
    EEPROM.write(5, matrix_speed);
  }
  PRINT("\nmatrix_speed: ",matrix_speed);

  // 6 - Init Display whit Time/Message  
  display_msg = EEPROM.read(6);
  PRINT("\ndisplay_msg: ",display_msg);

  // Close EEPROM    
  EEPROM.commit();
  EEPROM.end();

  checkServer();

  curMessage[0] = newMessage[0] = '\0';
 
  // Set up first message 
  String stringMsg = "ESP32_Time_Text_Matrix0_JR " + HWversion + " - IP: " + WiFi.localIP().toString() + "\n";
  stringMsg.toCharArray(curMessage, BUF_SIZE);
  PRINT("\ncurMessage >>> ", curMessage);

}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void loop() {
  // Wait for a client to connect and when they do process their requests
  if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
 
  if (display_msg == false) {
    // Update and refresh of the date and time on the displays
    if (millis() % 60000) UpdateLocalTime();
    Oled_Time();
    if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
    matrix_time();
 
    // Serial display time and date & dots flashing
    if(millis()-dotTime > 500) {
      //PRINT("\n",CurrentTime);
      //PRINT("\n",mDay);
      dotTime = millis();
      dots = !dots;
    }
    
    // Show date on matrix display
    if (display_date == true) {
      if(millis()-clkTime > 30000 && !del && dots) { // clock for 30s, then scrolls for about 5s
        _scroll=true;
        Oled_Time();
        printStringWithShift((String("     ")+ mDay + "           ").c_str(), matrix_speed);
        clkTime = millis();
        _scroll=false;
      }
    }
  
  // Display MESSAGE
  }else {
    // Update and refresh of the date and time on OLED display
    if (millis() % 60000) UpdateLocalTime();
    Oled_Time();

    // Refresh message on matrix display
    if (newMessageAvailable) {
      strcpy(curMessage, newMessage);
      newMessageAvailable = false;
    }

    if(millis()-clkTime > 2000) { 
      _scroll=true;
      Oled_Time();
      printStringWithShift((String("     ")+ curMessage + "           ").c_str(), matrix_speed);
      clkTime = millis();
      _scroll=false;
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", Timezone, 1);                                                  // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(2000);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//////////////////////////////////////////////////////////////////////////////
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);

  //See http://www.cplusplus.com/reference/ctime/strftime/
  char output[30];
  strftime(output, 30, "%A", localtime(&now));
  nDay = output;
  mDay = nDay;
  nDay = (nDay.substring(0,3)) + ". ";
  if (display_EU == true) {
    strftime(output, 30,"%d-%m", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30,", %d %B %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%H:%M:%S", localtime(&now));
    CurrentTime = output;
  }
  else { 
    strftime(output, 30, "%m-%d", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30, ", %B %d, %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%r", localtime(&now));
    CurrentTime = output;
  }
  return true;
}
/////////////////////////////////////////////////////
//------------------ OLED DISPLAY -----------------//
/////////////////////////////////////////////////////
void Oled_Time() { 
  display.clearDisplay();
  display.setCursor(2,0);   // center date display
  display.setTextSize(1);   
  display.println(CurrentDate);

  display.setTextSize(2);   
  display.setCursor(8,16);  // center Time display
  if (CurrentTime.startsWith("0")){
    display.println(CurrentTime.substring(1,5));
  }else {
    display.setCursor(0,16);
    display.println(CurrentTime.substring(0,5));
  }
  
  if (display_EU == true) {
    display.setCursor(7,33); // center Time display
    if (_scroll) {
      display.print("----");
    }else{
      display.print("(" + CurrentTime.substring(6,8) + ")");
    }
  }else {
    if (_scroll) {
      display.print("----");
    }else{
      display.print("(" + CurrentTime.substring(6,8) + ")");
    }
    display.setTextSize(1);
    display.setCursor(40,33); 
    display.print(CurrentTime.substring(8,11));
  }
  display.display();
}
/////////////////////////////////////////////////////
//------------------ MATRIX DISPLAY ---------------//
/////////////////////////////////////////////////////
void matrix_time() {
  h = (CurrentTime.substring(0,2)).toInt();
  m = (CurrentTime.substring(3,5)).toInt();
  s = (CurrentTime.substring(6,8)).toInt();

  if (show_seconds == true) {
    if (animated_time == true) {
      showAnimSecClock(); //With ROTATE 90 send 1 times + delay(4)
      //delay(4);
      showAnimSecClock(); //With ROTATE 0 send 4 times, without delay
      showAnimSecClock();
      showAnimSecClock();
    }else {
      showSecondsClock();
    }
  } else {
    if (animated_time == true) {
      showAnimClock();  //With ROTATE 90 send 1 times + delay(4)
      //delay(4);
      showAnimClock();  //With ROTATE 0 send 4 times, without delay
      showAnimClock();
      showAnimClock();
    }else {
      showSimpleClock();
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
// A short method of adding the same web page header to some text
//////////////////////////////////////////////////////////////////////////////
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html>"; 
  webpage += "<style>html { font-family:tahoma; display:inline-block; margin:0px auto; text-align:center;}";
  webpage += "#mark      {border: 5px solid #316573 ; width: 1120px;} "; 
  webpage += "#header    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:36px;}";
  webpage += "#section   {background-color:#E6F5F9; width:1100px; padding:10px; color:#0D7693 ; font-size:14px;}";
  webpage += "#footer    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:24px; clear:both;}";
 
  webpage += ".button {box-shadow: 0px 10px 14px -7px #276873; background:linear-gradient(to bottom, #599bb3 5%, #408c99 100%);";
  webpage += "background-color:#599bb3; border-radius:8px; color:white; padding:13px 32px; display:inline-block; cursor:pointer;";
  webpage += "text-decoration:none;text-shadow:0px 1px 0px #3d768a; font-size:50px; font-weight:bold; margin:2px;}";
  webpage += ".button:hover {background:linear-gradient(to bottom, #408c99 5%, #599bb3 100%); background-color:#408c99;}";
  webpage += ".button:active {position:relative; top:1px;}";
 
  webpage += ".button2 {box-shadow: 0px 10px 14px -7px #8a2a21; background:linear-gradient(to bottom, #f24437 5%, #c62d1f 100%);";
  webpage += "background-color:#f24437; text-shadow:0px 1px 0px #810e05; }";
  webpage += ".button2:hover {background:linear-gradient(to bottom, #c62d1f 5%, #f24437 100%); background-color:#f24437;}";
  
  webpage += ".line {border: 3px solid #666; border-radius: 300px/10px; height:0px; width:80%;}";
  
  webpage += "input[type=\"text\"] {font-size:42px; width:90%; text-align:left;}";
  
  webpage += "input[type=range]{height:61px; -webkit-appearance:none;  margin:10px 0; width:70%;}";
  webpage += "input[type=range]:focus {outline:none;}";
  webpage += "input[type=range]::-webkit-slider-runnable-track {width:70%; height:30px; cursor:pointer; animate:0.2s; box-shadow: 2px 2px 5px #000000; background:#C3E0E8;border-radius:10px; border:1px solid #000000;}";
  webpage += "input[type=range]::-webkit-slider-thumb {box-shadow:3px 3px 6px #000000; border:2px solid #FFFFFF; height:50px; width:50px; border-radius:15px; background:#316573; cursor:pointer; -webkit-appearance:none; margin-top:-11.5px;}";
  webpage += "input[type=range]:focus::-webkit-slider-runnable-track {background: #C3E0E8;}";
  webpage += "</style>";
 
  webpage += "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/animate.css/4.1.1/animate.min.css\"/>";
  webpage += "<html><head><title>ESP32 NTP Clock</title>";
  webpage += "</head>";
  webpage += "<script>";
  webpage += "function SendData()";
  webpage += "{";
  webpage += "  strLine = \"\";";
  webpage += "  nocache = \"/&nocache=\" + Math.random() * 1000000;";
  webpage += "  var request = new XMLHttpRequest();";
  webpage += "  strLine = \"&MSG=\" + document.getElementById(\"data_form\").Message.value;";
  webpage += "  strLine = strLine + \"/&SP=\" + document.getElementById(\"data_form\").Speed.value;";
  webpage += "  request.open(\"GET\", strLine + nocache, false);";
  webpage += "  request.send(null);";
  webpage += "}";
  webpage += "function SendBright()";
  webpage += "{";
  webpage += "  strLine = \"\";";
  webpage += "  var request = new XMLHttpRequest();";
  webpage += "  strLine = \"BRIGHT=\" + document.getElementById(\"bright_form\").Bright.value;";
  webpage += "  request.open(\"GET\", strLine, false);";
  webpage += "  request.send(null);";
  webpage += "}";
  webpage += "</script>";
  webpage += "<div id=\"mark\">";
  webpage += "<div id=\"header\"><h1 class=\"animate__animated animate__flash\">NTP - Local Time Clock " + HWversion + "</h1>";
}
//////////////////////////////////////////////////////////////////////////////
void button_Home() {
  webpage += "<p><a href=\"\\HOME\"><type=\"button\" class=\"button\">Refresh WEB</button></a></p>";
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ESP32_set_message() {
  client = server.available();
  append_webpage_header();
  webpage += "<h5 class=\"animate__animated animate__fadeInLeft\"><p>MSG: ";
  if (newMessageAvailable) {
    webpage += newMessage;
  }else{
    webpage += curMessage;
  }
  webpage += "</p>";
  webpage += "<p>Speed: ";
  webpage += matrix_speed;
  webpage += " ms. delay</p></h5>";
  webpage += "</div>";

  webpage += "<div id=\"section\">";
  webpage += "<form id=\"data_form\" name=\"frmText\">";
  webpage += "<a>NEW message<br><input type=\"text\" name=\"Message\" maxlength=\"255\"><br><br>";
  webpage += "Speed (ms. delay)<br>Fast(10)<input type=\"range\" name=\"Speed\" min=\"10\" max=\"40\" value=\"";
  webpage += matrix_speed;
  webpage += "\">(40)Slow</a>";

  webpage += "<br><br>";
  webpage += "<p><a href=\"\"><type=\"button\" onClick=\"SendData()\" class=\"button\">Send Data</button></a></p>";
  webpage += "<br><hr class=\"line\"><br>";
  webpage += "<p><a href=\"\\CLOCK_TOGGLE\"><type=\"button\" class=\"button\">CLOCK</button></a></p>";
  webpage += "<br></form>";
  webpage += "</div>"; 
  webpage += "<div id=\"footer\">Copyright &copy; J_RPM 2020</div></div></html>\r\n";
  client.println(WebResponse);
  client.println(webpage);
  PRINTS("\n>>> ESP32_set_message() OK! ");
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void NTP_Clock_home_page() {
  append_webpage_header();
  webpage += "<p><h3 class=\"animate__animated animate__fadeInLeft\">Display Mode is ";
  if (display_EU == true) webpage += "EU"; else webpage += "USA";
  webpage += "</p>[";
  if (animated_time == true) webpage += "Animated "; else webpage += "Normal ";
  if (show_seconds == true) webpage += "(hh:mm:ss) "; else webpage += "(HH:MM) ";
  if (display_date == true) webpage += "& Date ";
  webpage += "- B:";     
  webpage += brightness;
  webpage += "]</h3></div>";

  webpage += "<div id=\"section\">";
  button_Home();
  webpage += "<p><a href=\"\\DISPLAY_MODE_USA\"><type=\"button\" class=\"button\">USA mode</button></a>";
  webpage += "<a href=\"\\DISPLAY_MODE_EU\"><type=\"button\" class=\"button\">EU mode</button></a></p>";

  webpage += "<p><a href=\"\\TIME_ANIM\"><type=\"button\" class=\"button\">Animated Time</button></a>";
  webpage += "<a href=\"\\TIME_NORMAL\"><type=\"button\" class=\"button\">Normal Time</button></a></p>";

  webpage += "<p><a href=\"\\TIME_MINUTE\"><type=\"button\" class=\"button\">HH:MM</button></a>";
  webpage += "<a href=\"\\TIME_SECOND\"><type=\"button\" class=\"button\">hh:mm:ss</button></a></p>";

  webpage += "<p><a href=\"\\DISPLAY_DATE\"><type=\"button\" class=\"button\">Show Date</button></a>";
  webpage += "<a href=\"\\DISPLAY_NO_DATE\"><type=\"button\" class=\"button\">Only Time</button></a></p>";
  
  webpage += "<form id=\"bright_form\">";
  webpage += "<a>Brightness<br>MIN(0)<input type=\"range\" name=\"Bright\" min=\"0\" max=\"15\" value=\"";
  webpage += brightness;
  webpage += "\">(15)MAX</a></form>";
  webpage += "<p><a href=\"\"><type=\"button\" onClick=\"SendBright()\" class=\"button\">Send Brightness</button></a></p>";

  webpage += "<hr class=\"line\"><br>";
  webpage += "<p><a href=\"\\RESTART\"><type=\"button\" class=\"button\">Restart ESP32</button></a></p>";
  webpage += "<p><a href=\"\\MSG_TOGGLE\"><type=\"button\" class=\"button\">MESSAGE</button></a></p>";
  webpage += "<br><p><a href=\"\\RESET_WIFI\"><type=\"button\" class=\"button button2\">Reset WiFi</button></a></p>";
  webpage += "</div>";
  end_webpage();
}
//////////////////////////////////////////////////////////////////////////////
void reset_wifi() {
  append_webpage_header();
  webpage += "<p><h2>New WiFi Connection</h2></p></div>";
  webpage += "<div id=\"section\">";
  webpage += "<p>&#149; Connect WiFi to SSID: <b>ESP32_AP</b></p>";
  webpage += "<p>&#149; Next connect to: <b><a href=http://192.168.4.1/>http://192.168.4.1/</a></b></p>";
  webpage += "<p>&#149; Make the WiFi connection</p>";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();      // RESET WiFi in ESP32
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void web_reset_ESP32() {
  append_webpage_header();
  webpage += "<p><h2>Restarting ESP32...</h2></p></div>";
  webpage += "<div id=\"section\">";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void end_webpage(){
  webpage += "<div id=\"footer\">Copyright &copy; J_RPM 2020</div></div></html>\r\n";
  if (display_msg == false) {
    server2.send(200, "text/html", webpage);
  }else {
    client = server.available();
    client.println(WebResponse);
    client.println(webpage);
  }
  PRINTS("\n>>> end_webpage() OK! ");
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void display_mode_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, display_EU);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_date_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(1, display_date);
  end_Eprom();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_matrix() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(2, brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_mode() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(3, animated_time);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_view() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(4, show_seconds);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_matrix_speed() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(5, matrix_speed);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_init_msg() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(6, display_msg);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void end_Eprom() {
  del=0;
  dots=1;
  EEPROM.commit();
  EEPROM.end();
}
//////////////////////////////////////////////////////////////
void reset_ESP32() {
  sendCmdAll(CMD_SHUTDOWN,0);
  ESP.restart();
  delay(5000);
}
//////////////////////////////////////////////////////////////
void display_AP_wifi () {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(4,0);   
  display.println(F("ENTRY"));
  display.setCursor(10,16);   
  display.println(F("WiFi"));
  display.setTextSize(1);   
  display.setCursor(8,32);   
  display.println("ESP32_AP");
  display.setCursor(0,40);   
  display.println(F("192168.4.1"));
  display.display();
}
//////////////////////////////////////////////////////////////
void display_flash() {
  for (int i=0; i<8; i++) {
    display.invertDisplay(true);
    display.display();
    sendCmdAll(CMD_SHUTDOWN,0);
    delay (150);
    display.invertDisplay(false);
    display.display();
    sendCmdAll(CMD_SHUTDOWN,1);
    delay (150);
  }
}
//////////////////////////////////////////////////////////////
void display_ip() {
  // Print the IP address MATRIX
  printStringWithShift((String("  http:// ")+ WiFi.localIP().toString()).c_str(), matrix_speed);

  // Display OLED & MATRIX
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(4,8);   
  display.print("ENTRY");
  display.setTextSize(1);   
  display.setCursor(0,26);   
  display.print("http://");
  display.print(WiFi.localIP());
  display.println("/");
  display.display();
  display_flash();
}
//////////////////////////////////////////////////////////////////////////////
void showSimpleClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig6x8);
   }else {
    showDigit(h/10,  0, dig6x8);
   }
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showSecondsClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig4x8r);
   }else {
    showDigit(h/10,  0, dig4x8r);
   }
  showDigit(h%10,  5, dig4x8r);
  showDigit(m/10, 11, dig4x8r);
  showDigit(m%10, 16, dig4x8r);
  showDigit(s/10, 23, dig4x8r);
  showDigit(s%10, 28, dig4x8r);
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showAnimClock(){
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
  if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
}
//////////////////////////////////////////////////////////////////////////////
void showAnimSecClock(){
  byte digPos[6]={0,5,11,16,23,28};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig4x8r);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig4x8r);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig4x8r);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
  if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
}
//////////////////////////////////////////////////////////////////////////////
void showDigit(char ch, int col, const uint8_t *data){
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}
//////////////////////////////////////////////////////////////////////////////
void setCol(int col, byte v){
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}
//////////////////////////////////////////////////////////////////////////////
int showChar(char ch, const uint8_t *data){
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}
//////////////////////////////////////////////////////////////////////////////
unsigned char convertPolish(unsigned char _c){
  unsigned char c = _c;
  if(c==196 || c==197 || c==195) {
    dualChar = c;
    return 0;
  }
  if(dualChar) {
    switch(_c) {
      case 133: c = 1+'~'; break; // 'ą'
      case 135: c = 2+'~'; break; // 'ć'
      case 153: c = 3+'~'; break; // 'ę'
      case 130: c = 4+'~'; break; // 'ł'
      case 132: c = dualChar==197 ? 5+'~' : 10+'~'; break; // 'ń' and 'Ą'
      case 179: c = 6+'~'; break; // 'ó'
      case 155: c = 7+'~'; break; // 'ś'
      case 186: c = 8+'~'; break; // 'ź'
      case 188: c = 9+'~'; break; // 'ż'
      //case 132: c = 10+'~'; break; // 'Ą'
      case 134: c = 11+'~'; break; // 'Ć'
      case 152: c = 12+'~'; break; // 'Ę'
      case 129: c = 13+'~'; break; // 'Ł'
      case 131: c = 14+'~'; break; // 'Ń'
      case 147: c = 15+'~'; break; // 'Ó'
      case 154: c = 16+'~'; break; // 'Ś'
      case 185: c = 17+'~'; break; // 'Ź'
      case 187: c = 18+'~'; break; // 'Ż'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  switch(_c) {
    case 185: c = 1+'~'; break;
    case 230: c = 2+'~'; break;
    case 234: c = 3+'~'; break;
    case 179: c = 4+'~'; break;
    case 241: c = 5+'~'; break;
    case 243: c = 6+'~'; break;
    case 156: c = 7+'~'; break;
    case 159: c = 8+'~'; break;
    case 191: c = 9+'~'; break;
    case 165: c = 10+'~'; break;
    case 198: c = 11+'~'; break;
    case 202: c = 12+'~'; break;
    case 163: c = 13+'~'; break;
    case 209: c = 14+'~'; break;
    case 211: c = 15+'~'; break;
    case 140: c = 16+'~'; break;
    case 143: c = 17+'~'; break;
    case 175: c = 18+'~'; break;
    default:  break;
  }
  return c;
}
//////////////////////////////////////////////////////////////////////////////
void printCharWithShift(unsigned char c, int shiftDelay) {
  // To check WiFi inputs faster 
  shiftDelay = shiftDelay / 4;
  
  c = convertPolish(c);
  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
    delay(shiftDelay);
    if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
    delay(shiftDelay);
    if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
    delay(shiftDelay);
    if (display_msg == false) {server2.handleClient();}else{handleWiFi();}
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}
//////////////////////////////////////////////////////////////////////////////
void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}
//////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
const char *err2Str(wl_status_t code){
  switch (code){
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("PASSWORD_ERROR"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}
/////////////////////////////////////////////////////////////////
uint8_t htoi(char c) {
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}
/////////////////////////////////////////////////////////////////

////////////////////////////////////////////////
// Check clock config
////////////////////////////////////////////////
void _display_mode_usa() {
  display_EU = false;
  PRINTS("\n-> DISPLAY_MODE_USA");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_eu() {
  display_EU = true;
  PRINTS("\n-> DISPLAY_MODE_EU");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_anim() {
  animated_time = true;
  PRINTS("\n-> TIME_ANIM");
  display_time_mode();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_normal() {
  animated_time = false;
  PRINTS("\n-> TIME_NORMAL");
  display_time_mode();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_minute() {
  show_seconds = false;
  PRINTS("\n-> TIME_MINUTE");
  display_time_view();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_second() {
  show_seconds = true;
  PRINTS("\n-> TIME_SECOND");
  display_time_view();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_date() {
  display_date = true;
  PRINTS("\n-> DISPLAY_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_no_date() {
  display_date = false;
  PRINTS("\n-> DISPLAY_NO_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _bright_0() {
  PRINTS("\n-> BRIGHT=0");
  brightness = 0;     //DUTY CYCLE: 1/32 (MIN) 
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_1() {
  PRINTS("\n-> BRIGHT=1");
  brightness = 1;    
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_2() {
  PRINTS("\n-> BRIGHT=2");
  brightness = 2;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_3() {
  PRINTS("\n-> BRIGHT=3");
  brightness = 3;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_4() {
  PRINTS("\n-> BRIGHT=4");
  brightness = 4;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_5() {
  PRINTS("\n-> BRIGHT=5");
  brightness = 5;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_6() {
  PRINTS("\n-> BRIGHT=6");
  brightness = 6;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_7() {
  PRINTS("\n-> BRIGHT=7");
  brightness = 7;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_8() {
  PRINTS("\n-> BRIGHT=8");
  brightness = 8;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_9() {
  PRINTS("\n-> BRIGHT=9");
  brightness = 9;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_10() {
  PRINTS("\n-> BRIGHT=10");
  brightness = 10;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_11() {
  PRINTS("\n-> BRIGHT=11");
  brightness = 11;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_12() {
  PRINTS("\n-> BRIGHT=12");
  brightness = 12;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_13() {
  PRINTS("\n-> BRIGHT=13");
  brightness = 13;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_14() {
  PRINTS("\n-> BRIGHT=14");
  brightness = 14;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_15() {
  PRINTS("\n-> BRIGHT=15");
  brightness = 15;     //DUTY CYCLE: 31/32 (MAX)
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _save_bright(){
  brightness_matrix();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _msg_toggle() {
  display_msg = true;
  PRINTS("\n-> MSG_TOGGLE");
  display_init_msg();
  display_msg = false;
  _restart();
}
/////////////////////////////////////////////////////////////////
void _clock_toggle() {
  display_msg = false;
  PRINTS("\n-> CLOCK_TOGGLE");
  display_init_msg();
  display_msg = true;
  _restart();
}
/////////////////////////////////////////////////////////////////
void _restart() {
  PRINTS("\n-> RESTART");
  web_reset_ESP32();
}
/////////////////////////////////////////////////////////////////
void _reset_wifi() {
  PRINTS("\n-> RESET_WIFI");
  reset_wifi();
}
/////////////////////////////////////////////////////////////////
void _home() {
  PRINTS("\n-> HOME");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void responseWeb(){
  PRINTS("\nS_RESPONSE");
  if (display_msg == false) {
    NTP_Clock_home_page();
  }else {
    ESP32_set_message();
  }
  PRINT("\n>>> SendPage display_msg: ",display_msg);
}
/////////////////////////////////////////////////////////////////
void checkServer(){
  if (display_msg) {
    server.begin();  // Start the WiFiServer
    PRINTS("\nWiFiServer started");
  }else {
    server2.begin();  // Start the WebServer
    PRINTS("\nWebServer started");
    // Define what happens when a client requests attention
    server2.on("/", _home);
    server2.on("/DISPLAY_MODE_USA", _display_mode_usa); 
    server2.on("/DISPLAY_MODE_EU", _display_mode_eu); 
    server2.on("/TIME_ANIM", _time_anim); 
    server2.on("/TIME_NORMAL", _time_normal); 
    server2.on("/TIME_MINUTE", _time_minute); 
    server2.on("/TIME_SECOND", _time_second); 
    server2.on("/DISPLAY_DATE", _display_date);
    server2.on("/DISPLAY_NO_DATE", _display_no_date);
    server2.on("/MSG_TOGGLE", _msg_toggle);
    server2.on("/CLOCK_TOGGLE", _clock_toggle);
    server2.on("/BRIGHT=0", _bright_0); 
    server2.on("/BRIGHT=1", _bright_1); 
    server2.on("/BRIGHT=2", _bright_2); 
    server2.on("/BRIGHT=3", _bright_3); 
    server2.on("/BRIGHT=4", _bright_4); 
    server2.on("/BRIGHT=5", _bright_5); 
    server2.on("/BRIGHT=5", _bright_6); 
    server2.on("/BRIGHT=7", _bright_6); 
    server2.on("/BRIGHT=8", _bright_8); 
    server2.on("/BRIGHT=9", _bright_9); 
    server2.on("/BRIGHT=10", _bright_10); 
    server2.on("/BRIGHT=11", _bright_11); 
    server2.on("/BRIGHT=12", _bright_12); 
    server2.on("/BRIGHT=13", _bright_13); 
    server2.on("/BRIGHT=14", _bright_14); 
    server2.on("/BRIGHT=15", _bright_15); 
    server2.on("/HOME", _home);
    server2.on("/RESTART", _restart);  
    server2.on("/RESET_WIFI", _reset_wifi);
  }
}
/////////////////////////////////////////////////////////////////
void getData(char *szMesg, uint16_t len) {
  char *pStart, *pEnd;      // pointer to start and end of text

  ////////////////////////////////////////////////
  // Check clock config
  ////////////////////////////////////////////////

  //-----------------------------------------------
  pStart = strstr(szMesg, "CLOCK_TOGGLE");
  if (pStart != NULL) {
    _clock_toggle();
    return; 
  }
  /////////////////////////////////////////////////
  // Check text message
  // Message may contain data for:
  // New text (/&MSG=)
  // Speed (/&SP=)
  /////////////////////////////////////////////////
  pStart = strstr(szMesg, "/&MSG=");
  
  if (pStart != NULL){
    char *psz = newMessage;

    pStart += 6;  // skip to start of data
    pEnd = strstr(pStart, "/&");

    if (pEnd != NULL) {
      while (pStart != pEnd) {
        if ((*pStart == '%') && isdigit(*(pStart + 1))) {
          // replace %xx hex code with the ASCII character
          char c = 0;
          pStart++;
          c += (htoi(*pStart++) << 4);
          c += htoi(*pStart++);
          *psz++ = c;
        } else *psz++ = *pStart++;
      }

      *psz = '\0'; // terminate the string
      newMessageAvailable = (strlen(newMessage) != 0);
      PRINT("\nNew Msg: ", newMessage);
    }
  }

  // check speed
  pStart = strstr(szMesg, "/&SP=");
  if (pStart != NULL) {
    pStart += 5;  // skip to start of data

    int speed = atoi(pStart);
    matrix_speed = speed;
    display_matrix_speed();
    PRINT("\n-> Speed: ",matrix_speed );
    return;
  }
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void handleWiFi(void) {
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static uint32_t timeStart;


  switch (state)
  {
  case S_IDLE:   // initialise
    PRINTS("\n-> S_IDLE");
    state = S_WAIT_CONN;
    idxBuf = 0;
    break;

  case S_WAIT_CONN:   // waiting for connection
    client = server.available();
    if (!client) break;
    if (!client.connected()) break;

    #if DEBUG
        char szTxt[20];
        sprintf(szTxt, "%01d.%01d.%01d.%01d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
        PRINT("\nNew client @ ", szTxt);
    #endif

    state = S_READ;
    timeStart = millis();
    break;
    
  case S_READ:   // read message
    PRINTS("\nS_READ ");

    //.................................
    while (client.available()) {
      char c = client.read();
      if ((c == '\r') || (c == '\n')) {
        szBuf[idxBuf] = '\0';
        PRINT("\nOK -> Recv: ", szBuf);
        state = S_EXTRACT;
        client.flush();
      } else {
        szBuf[idxBuf++] = (char)c;
      }
    }
    //.................................
    
    if (millis() - timeStart > 300) {
      PRINTS("\nErr -> Wait timeout");
      state = S_DISCONN;
    }
    break;

  case S_EXTRACT:   // extract message
    PRINTS("\nS_EXTRACT");
    state = S_RESPONSE;
    // Extract the string from the message if there is one
    getData(szBuf, BUF_SIZE);
    break;

  case S_RESPONSE:   // response to the client
    // Return the response to the client (web page)
    state = S_DISCONN;
    responseWeb();
    break;

  case S_DISCONN:   // close client
    PRINTS("\nS_DISCONN");
    state = S_IDLE;
    client.flush();
    client.stop();
    break;


  default:  state = S_IDLE;
  }
}
////////////////////////// END //////////////////////////////////
/////////////////////////////////////////////////////////////////
