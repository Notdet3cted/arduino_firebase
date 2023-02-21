#include <EEPROM.h>
#include "GravityTDS.h"
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUDP.h>

#include <WiFiManager.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

#define TdsSensorPin 33
#define EEPROM_SIZE 512

GravityTDS gravityTds;

//LCD
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

//NTP TIME---------
#define NTP_OFFSET   7 * 60 * 60      // In seconds
#define NTP_ADDRESS  "pool.ntp.org"
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET);

String jam ;
String menit ;
String detik ;
String t;


//Firebase---------
#define FIREBASE_HOST ""
#define FIREBASE_AUTH ""
FirebaseData firebaseData;  //Declare the Firebase Data object in the global scope
FirebaseJson json;

//WiFi
//const char* ssid = "";  //replace
//const char* password =  ""; //replace

float temperature = 25, tdsValue;


const int relay = 2;
const int relay2 = 4;

int tds_up, tds_down;

String ket;

void setup()
{
    Serial.begin(115200);

    pinMode(relay, OUTPUT);
    pinMode(relay2, OUTPUT);
    digitalWrite(relay, HIGH);                
    digitalWrite(relay2, HIGH);
    
    EEPROM.begin(EEPROM_SIZE);  //Initialize EEPROM
    
    gravityTds.setPin(TdsSensorPin);
    gravityTds.setAref(3.3);  //reference voltage on ADC, default 5.0V on Arduino UNO
    gravityTds.setAdcRange(4095.0);  //1024 for 10bit ADC;4096 for 12bit ADC
    gravityTds.begin();  //initialization

    lcd.init();          
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Menunggu");
    lcd.setCursor(0, 2);
    lcd.print("Koneksi)");
    delay(500);
    
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
  
    if (!res)
    {
      lcd.print("Connected Failed");
      return;
      // ESP.restart();
    }
    else
    {
      // if you get here you have connected to the WiFi
      lcd.setCursor(0, 0);
      lcd.print("Connected");
      lcd.setCursor(0, 2);
      lcd.print("YEAY....:)");
      delay(1000);
    }

    
//    lcd.setCursor(0, 0);
//    lcd.print("Please wait.........");
//    lcd.setCursor(0, 2);
//    lcd.print("Connecting To WiFi..");

  
    uint32_t notConnectedCounter = 0;
    
//    WiFi.begin(ssid, password);                                  
//    Serial.print("Connecting to ");
//    Serial.print(ssid);
//    while (WiFi.status() != WL_CONNECTED) {
//    Serial.print(".");
//    delay(500);
//    notConnectedCounter++;
//    if (notConnectedCounter>30) {
//    ESP.restart(); }
//    }
//  
//    Serial.print("IP address: ");
//    Serial.println(WiFi.localIP());
  
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please wait.........");
    lcd.setCursor(0, 2);
    lcd.print("Initialize Data.....");
  
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); //FIREBASE BEGIN
    Firebase.reconnectWiFi(true);                 //AUTO RECONNECT
    Firebase.setMaxRetry(firebaseData, 3);    //Optional, set number of error retry
    Firebase.setMaxErrorQueue(firebaseData, 30);    //Optional, set number of error resumable queues

    Firebase.getInt(firebaseData, "/data/max"); // TAKE DATA MAX
    tds_up = firebaseData.intData();
    Firebase.getInt(firebaseData, "/data/min"); // TAKE DATA MIN
    tds_down = firebaseData.intData();


    //TIME CLIENT
    timeClient.update();                              
    String newtime = timeClient.getFormattedTime();
    Serial.print("the time is : ");
    Serial.println(newtime);
    Serial.print("Hour    : ");
    Serial.println((newtime.substring(0,2)).toInt());
    Serial.print("Minute  : ");
    Serial.println((newtime.substring(3,5)).toInt());
    Serial.print("Seconds : ");
    Serial.println((newtime.substring(6,8)).toInt());
  
    delay(1000);

    //SET TIME
    setTime((newtime.substring(0,2)).toInt(),(newtime.substring(3,5)).toInt(),(newtime.substring(6,8)).toInt(),1,1,20); 
    delay(2000);  

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("*******WELCOME******");
  
    String IPaddress =  WiFi.localIP().toString();
    Serial.println(IPaddress);
    
    lcd.setCursor(0, 1);
    lcd.print(IPaddress);
    
    
}

void loop()
{
    //temperature = readTemperature();  //add your temperature sensor and read it
    gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
    gravityTds.update();  //sample and calculate
    tdsValue = gravityTds.getTdsValue();  // then get the value
    Serial.print(tdsValue,0);
    Serial.println("ppm");
    

    
    

    //SEND DATA TDS TO FIREBASE
    Firebase.setInt(firebaseData,"/data/PPM",tdsValue);

    if(tdsValue <= tds_down)
    {
      ket = "KURANG NUTRISI";
      Firebase.setString(firebaseData,"/data/kondisi","KURANG");
      Firebase.setBool(firebaseData,"/data/status",true);
      
      digitalWrite(relay, LOW); 
      digitalWrite(relay2, LOW);
      logger();
      } 
    else
    {
      ket = "CUKUP NUTRISI";
      Firebase.setString(firebaseData,"/data/kondisi","CUKUP");
      Firebase.setBool(firebaseData,"/data/status",false);

      digitalWrite(relay, HIGH);                
      digitalWrite(relay2, HIGH);
      }

  //Program tampilan lcd dan serial
  lcd.clear(); 
  lcd.setCursor(0, 0);  
  lcd.print("TDS : "); lcd.print(tdsValue,0); lcd.print(" ppm ");
  lcd.setCursor(0,1);
  lcd.print(ket);

  delay(1000); 

    
}

void logger() {

  jam = String(hour());
  menit = String(minute());
  detik = String(second());
  
  if (hour()<10)
  jam = "0"+jam;
  if (minute()<10)
  menit = "0"+menit;
  if (second()<10)
  detik = "0"+detik;
  t = jam+":"+menit+":"+detik;

    
    json.set("tds", tdsValue);
    json.set("time", t);
    
    Firebase.pushJSON(firebaseData,"/database",json);
}
