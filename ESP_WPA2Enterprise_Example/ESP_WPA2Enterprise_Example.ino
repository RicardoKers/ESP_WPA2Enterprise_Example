#include <Arduino.h> // To use PlatformIO
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
  #include "wpa2_enterprise.h"
}

/////////////////////////////////////////////////////////////////////////////////
//            INITIAL SETUP
/////////////////////////////////////////////////////////////////////////////////
#define SerialDebug 1 // Enable serial debugging (1 or 0)
#define TelnetDebug 0 // Enables debugging via telnet on port 23 if SerialDebug is 0. It only works after wifi connection. (1 or 0)
#define OTAEmable 0 // Enables OTA update (1 or 0)
#define OTAHostName "Kers-ESP8266-1" // Hostname for the OTA update
#define OTAPAssword "KersPass" // Password for OTA update
#define WFMPortalName "Kers-ESP8266-1" // WIFIManager Configuration Portal Name
/////////////////////////////////////////////////////////////////////////////////

#if SerialDebug == 1
  #define DEBUG_PRINT(x)  Serial.print (x)
  #define DEBUG_PRINTLN(x)  Serial.println (x)
  #define DEBUG_BEGIN(x)  Serial.begin (x)
#else
  #if TelnetDebug == 1
    #include <TelnetStream.h>
    #define DEBUG_PRINT(x)  TelnetStream.print(x)
    #define DEBUG_PRINTLN(x)  TelnetStream.print(x); TelnetStream.print("\r\n")
    #define DEBUG_BEGIN(x)
  #else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_BEGIN(x)
  #endif
#endif

#if OTAEmable == 1
  #include <WiFiUdp.h>
  #include <ArduinoOTA.h>
#endif

char ssid[64];
char pass[64];
char WPA2User[64];
char WPA2Active;

WiFiManager wm;

void leEEPROM()
{
  // Lê as strings da EEPROM
  EEPROM.begin(512);
  for(int i = 0; i < 64; i++)
  {
    ssid[i] = EEPROM.read(i);    
  }
  for(int i = 0; i < 64; i++)
  {
    pass[i] = EEPROM.read(i+64);    
  }
  for(int i = 0; i < 64; i++)
  {
    WPA2User[i] = EEPROM.read(i+128);    
  }
  WPA2Active = EEPROM.read(192);   
  EEPROM.end();
}

void gravaEEPROM()
{       
  // Writes the strings to the EEPROM
  EEPROM.begin(512);
  for(int i = 0; i < 64; i++)
  {
    EEPROM.write(i, ssid[i]);    
  }
  for(int i = 0; i < 64; i++)
  {
    EEPROM.write(i+64, pass[i]);    
  }
  for(int i = 0; i < 64; i++)
  {
    EEPROM.write(i+128, WPA2User[i]);    
  }
  EEPROM.write(192,WPA2Active); 
  EEPROM.commit();
  EEPROM.end();
}

void apagaCredenciais()
{ // Erases EEPROM data and restarts ESP
  strcpy(ssid, "xxxxxxxxxxxxxxxx");
  strcpy(pass, "xxxxxxxxxxxxxxxx");
  strcpy(WPA2User, "xxxxxxxxxxxxxxxx");
  WPA2Active=0;
  gravaEEPROM();
  ESP.restart();
}

void connectWPA2Enterprise()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  

  // Initialize WPA2 Enterprise
  wifi_station_clear_cert_key();
  wifi_station_clear_enterprise_ca_cert();
  wifi_station_clear_enterprise_identity();
  wifi_station_clear_enterprise_username();
  wifi_station_clear_enterprise_password();
  wifi_station_clear_enterprise_new_password();
  wifi_station_set_wpa2_enterprise_auth(1);

  // Set identity and password
  wifi_station_set_enterprise_identity((uint8*)WPA2User, strlen(WPA2User));
  wifi_station_set_enterprise_username((uint8*)WPA2User, strlen(WPA2User));
  wifi_station_set_enterprise_password((uint8*)pass, strlen(pass));

  // Connect to the WiFi network
  WiFi.begin(ssid);
}

/////////////////////////////////////////////////////////////////////////////////
//          S E T U P
/////////////////////////////////////////////////////////////////////////////////
void setup() {
  DEBUG_BEGIN(115200);
  DEBUG_PRINTLN("\nSTARTING...:");

  leEEPROM();
  
  DEBUG_PRINTLN("\nREAD FROM EEPROM:");
  DEBUG_PRINTLN(ssid);
  //DEBUG_PRINTLN(pass);
  DEBUG_PRINTLN(WPA2User);
  
  WiFiManagerParameter custom_WPA2User("User", "User (WPA2 Enterprise)", WPA2User, 40);  
  wm.addParameter(&custom_WPA2User);

  if(WPA2Active==1)
  {
    connectWPA2Enterprise();
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
  }

  if(WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    // Didn't connect with saved credentials, loading WIFIManager
    DEBUG_PRINTLN("Calling WIFIManager");
    wm.setConfigPortalTimeout(90);
    int WMRes = wm.startConfigPortal(WFMPortalName);

    strcpy(ssid, wm.getWiFiSSID().c_str());
    strcpy(pass, wm.getWiFiPass().c_str());
    strcpy(WPA2User, custom_WPA2User.getValue());
    
    DEBUG_PRINTLN(ssid);
    //DEBUG_PRINTLN(pass);
    DEBUG_PRINTLN(WPA2User);

    if (WMRes == 0) {
      // Unable to connect, may require WPA2 Enterprise
      DEBUG_PRINTLN("Tentando com WPA2Enterprise");
      connectWPA2Enterprise();
      if(WiFi.waitForConnectResult() != WL_CONNECTED) 
      {
        // It also failed to connect, restarting the ESP
        DEBUG_PRINTLN("Impossivel conectar reiniciando em 5 segundos.");
        delay(5000);
        ESP.restart();
      }
      else
      {
        // Connected using WPA2Enterprise
        WPA2Active=1;
        gravaEEPROM(); // Saves WIFIManeger data in the EEPROM
        DEBUG_PRINTLN("Connected using WPA2Enterprise.");
      }      
    }
    else
    {
      // Connected without WPA2 Enterprise
      WPA2Active=0;
      gravaEEPROM(); // Saves WIFIManeger data in the EEPROM
      DEBUG_PRINTLN("Connected with WIFIManager data.");
    }
  }
  else
  {
    // Connected using EEPROM read credentials
    DEBUG_PRINTLN("Logged in with EEPROM credentials.");
  }

  #if TelnetDebug == true
    TelnetStream.begin();
    DEBUG_PRINTLN("Telnet Started:");
  #endif  

  #if OTAEmable == true
    ArduinoOTA.setHostname(OTAHostName);
    ArduinoOTA.setPassword(OTAPAssword);
    ArduinoOTA.begin();
    DEBUG_PRINTLN("OTA Enabled");
  #endif

  DEBUG_PRINT("IP: ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINTLN("Ready!");
}

/////////////////////////////////////////////////////////////////////////////////
//          L O O P
/////////////////////////////////////////////////////////////////////////////////
void loop() {
  // To manage the OTA connection
  #if OTAEmable == true
    ArduinoOTA.handle();
  #endif

  // Check if button 0 is pressed, optional
  if(digitalRead(0)==0)
  {
    delay(5000);
    if(digitalRead(0)==0)
    { // If the button is still pressed after 5 seconds
      // the user wants to delete the EEPROM credentials.
      DEBUG_PRINT("\Erasing EEPROM...\n");
      apagaCredenciais();
    }
  }
  // Sends the millis() value to serial or telnet every 2 seconds
  // Just to test communication, it can be deleted!
  static unsigned long next;
  if (millis() - next > 2000) {
    next = millis();
    // Executa a cada 2 segundos
    DEBUG_PRINT("millis = ");
    DEBUG_PRINTLN(next);
  }  
}
