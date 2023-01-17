#include <LiquidCrystal_I2C.h> // include lcd library
#include <WiFiManager.h>
#include <Arduino.h> // include wifi libarary

#define TdsSensorPin 27
#define VREF 3.3  // analog reference voltage(Volt) of the ADC
#define SCOUNT 30 // sum of sample point

// defined wifi modul
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 2. Define the API Key  */
#define API_KEY "AIzaSyDKcOV8lOojlfBrPJQlgVV_fIrGHSFeTbY"

/* 3. Define the RTDB URL */
#define DATABASE_URL "skripsi-tds-default-rtdb.firebaseio.com/" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "heri@aa.com"
#define USER_PASSWORD "123456"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

unsigned long count = 0;

int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
int lcdColumns = 16;
int lcdRows = 2;
const int relay = 2;
const int relay2 = 4;
float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25; // current temperature for compensation
String ket;

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen)
{
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++)
  {
    for (i = 0; i < iFilterLen - j - 1; i++)
    {
      if (bTab[i] > bTab[i + 1])
      {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
  {
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else
  {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
void setup()
{
  Serial.begin(115200);
  lcd.begin(2, 16);
  lcd.backlight();
  lcd.setCursor(0, 0);
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
    lcd.print("Connected");
    lcd.setCursor(0, 1);
    lcd.print("YEAY....:)");
    delay(1000);
  }

  pinMode(TdsSensorPin, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(relay2, OUTPUT);
  digitalWrite(relay, HIGH);
  digitalWrite(relay2, HIGH);

  lcd.clear();
  lcd.print(" M HERI PURNOMO ");
  lcd.setCursor(0, 1);
  lcd.print("    201851276   ");
  delay(3000);
  lcd.clear();
  lcd.print("INISIALISASI");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);

  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    lcd.print("Waiting");
    delay(500);
    lcd.print(".");
    delay(500);
    lcd.print(".");
    delay(500);
    lcd.print(".");
    delay(500);
  }

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

#if defined(ESP8266)
  // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
  fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
#endif

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  // Comment or pass false value when WiFi reconnection will control by your code or third party library
  Firebase.reconnectWiFi(true);

  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;
}

void loop()
{

  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U)
  { // every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin); // read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
    {
      analogBufferIndex = 0;
    }
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U)
  {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
    {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

      // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
      // temperature compensation
      float compensationVoltage = averageVoltage / compensationCoefficient;
      float tdsFire = 0;

      // convert voltage value to tds value
      tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
      Serial.print("TDS Value:"); // Serial.print("voltage:");
      Serial.print(tdsValue, 0);  // Serial.print(averageVoltage,2);
      Serial.println("ppm");      // Serial.print("V   ");
      delay(1000);

      tdsFire = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
      Firebase.RTDB.setFloat(&fbdo, F("/data/PPM"), tdsFire) ? "ok" : fbdo.errorReason().c_str(); // Add PPM Value
    }
  }
  //   if (Firebase.RTDB.getInt(&fbdo, "/data/min")) {
  //       if (fbdo.dataType() == "int") {
  //         intValue = fbdo.intData();
  //         Serial.println(intValue);
  //       }
  //     }
  //     else {
  //       Serial.println(fbdo.errorReason());
  //     }
  //   }
  // Program Pengkondisian Sinyal ppm
  if (tdsValue <= 500)
  {
    ket = "KURANG NUTRISI";
    Firebase.RTDB.setBool(&fbdo, F("/data/status"), true) ? "ok" : fbdo.errorReason().c_str(); // Add status value ON
    // Firebase.RTDB.setString(&fbdo, F("/data/kondisi"), "kurang") ? "ok" : fbdo.errorReason().c_str(); // Add kondisi Kurang
    digitalWrite(relay, LOW);
    digitalWrite(relay2, LOW);
  }
  else
  {
    ket = "CUKUP NUTRISI";
    Firebase.RTDB.setBool(&fbdo, F("/data/status"), false) ? "ok" : fbdo.errorReason().c_str(); // Add status value OFF
    // Firebase.RTDB.setInt(&fbdo, F("/data/kondisi"), "cukup") ? "ok" : fbdo.errorReason().c_str(); // Add kondisi cukup
    digitalWrite(relay, HIGH);
    digitalWrite(relay2, HIGH);
  }

  // Program tampilan lcd dan serial
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TDS : ");
  lcd.print(tdsValue, 0);
  lcd.print(" ppm ");
  lcd.setCursor(0, 1);
  lcd.print(ket);
  delay(1000);
}
