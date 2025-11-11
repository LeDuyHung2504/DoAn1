#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>

#include <SoftwareSerial.h>
SoftwareSerial mySerial(D7, D8);

//Thong tin ket noi Wi-Fi va may khach
const char* SSID = "Cat";
const char* Password = "cuato12345";
WiFiClientSecure Client;
WiFiUDP NTPUDP;

//Thong tin ket noi may chu Google Apps Script
const char* Host = "script.google.com";
const char* ScriptID = "AKfycbxe674CYCWcCIdLuHJ1ZZu_0FDbqYCt9nk7h9x_Xejx_9abmLiz7HlkwBb5Srcqb2rQ9w";
const int HTTPSPort = 443;

//Noi dung yeu cau HTTP POST
String Payload = "";
String PayloadBase = "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet1\", \"values\": ";

//Thong tin ket noi may chu NTP va thoi gian
const int TimeOffset = 7 * 3600;  //Chinh mui gio UTC+7
NTPClient TimeClient(NTPUDP, "pool.ntp.org", TimeOffset);
const int SCLPin = D1;
const int SDAPin = D2;
RTC_DS1307 RTC;
char FormatDateTime[20];

//Cam bien nhiet do
const int DHTType = DHT22;
const int TempPin = D3;  //Chan cam bien nhiet do
DHT TempSensor(TempPin, DHTType);
float Temp = 0;

//Cam bien do am
const int HumPin = D4;  //Cham cam bien do am
DHT HumSensor(HumPin, DHTType);
float Hum = 0;

//Cam bien toc do gio
float WindS = 0;

//Cam bien luong mua
const int RainPin = D5;
const int HallPin = D6;
volatile float RainF = 0;     // Luong mua mm
float RainIntensity = 0;      // Cuong do mua mm/phut
bool RainStatus = false;      // Trang thai mua
unsigned long RainStart = 0;  // Thoi gian bat dau mua
unsigned long RainTime = 0;   // Thoi gian mua
float ShutdownTime = 0;       // Thoi gian tat nguon

//Ghi, doc du lieu cam bien luong mua vao EEPROM
const int AddressBase = 0;  // Dia chi co so bo nho EEPROM
int Address = 4;            // Dia chi bo nho EEPROM
float EEPROMRainF = 0;      //Luong mua dung cho EEPROM

// Chong lap du lieu
unsigned long LastTime = 0;          // Thoi gian lat lan truoc
const unsigned long FlipDelay = 50;  // Khoang thoi gian giua moi lan lat
volatile bool FlipStatus = false;    // Flag to indicate ISR has been triggered

// Xu li ngat khi gau nuoc lat
void ICACHE_RAM_ATTR Flip() {
  unsigned long CurrentTime = millis();
  if (CurrentTime - LastTime > FlipDelay) {
    if (digitalRead(RainPin) == LOW || RainStatus == true) {
      RainF += 0.31;
      FlipStatus = true;
      LastTime = CurrentTime;
    }
  }
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  // Khoi tao EEPROM
  EEPROM.begin(4096);

  //Ket noi Wi-Fi
  WiFi.begin(SSID, Password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting");
    delay(500);
  }
  Client.setInsecure();  //Loai bo xac minh chung chi SSL

  //Ket noi may chu NTP
  TimeClient.begin();
  while (!TimeClient.update()) {
    Serial.println("Updating");
    delay(500);
  }

  //Khoi tao va cai dat thoi gian
  Wire.begin(D2, D1);
  RTC.begin();
  unsigned long EpochTime = TimeClient.getEpochTime();
  DateTime Now = DateTime(EpochTime);
  RTC.adjust(Now);

  //Khoi tao cam bien nhiet do
  TempSensor.begin();

  //Khoi tao cam bien do am
  HumSensor.begin();

  //Cai dat chan cam bien do luong mua
  pinMode(RainPin, INPUT);         // Cam bien mua
  pinMode(HallPin, INPUT_PULLUP);  // Cam bien tu
  attachInterrupt(digitalPinToInterrupt(HallPin), Flip, RISING);

  //Doc du lieu tu EEPROM de kiem tra co sap nguon hay khong
  ReadRainFall();
}

void loop() {
  GetDateTime();
  GetTemperature();
  GetHumidity();
  GetRainFall();
  GetWindSpeed();
  POSTRequest();
  PrintData();
  delay(4000);
}
//Ham lay thoi gian
void GetDateTime() {
  DateTime Now = RTC.now();
  //Dinh dang thoi gian
  sprintf(FormatDateTime, "%02d/%02d/%04d %02d:%02d:%02d",
          Now.day(), Now.month(), Now.year(), Now.hour(), Now.minute(), Now.second());
}
//Ham lay nhiet do
void GetTemperature() {
  Temp = TempSensor.readTemperature();
  if (isnan(Temp))
    Temp = 0.0;
}

//Ham lay do am
void GetHumidity() {
  Hum = HumSensor.readHumidity();
  if (isnan(Hum))
    Hum = 0.0;
}
//Ham lay luong mua
void GetRainFall() {
  //Ghi nhan thoi gian bat dau mua
  if (digitalRead(RainPin) == LOW && RainStatus == false) {
    RainStart = GetEpochTimeRTC();
    RainStatus = true;
  }
  //Ghi du lieu cam bien luong mua vao EEPROM
  if (FlipStatus == true) {
    FlipStatus = false;
    WriteRainFall();
  }
  //Xoa du lieu sau khi cam bien mua ngung hoat dong > 5 phut
  if (digitalRead(RainPin) == HIGH && RainStatus == true) {
    if (GetEpochTimeRTC() - RainTime > 300) {
      RainStatus = false;
      RainF = 0;
      RainIntensity = 0;
      RainStart = 0;
      RainTime = 0;
    }
  }
}
//Ham lay toc do gio
void GetWindSpeed() {
  String rec = mySerial.readString();
  WindS = rec.toFloat();
  delay(100);
}
//HTTP POST Request
void POSTRequest() {
  //Ket noi toi may chu Google Apps Script
  if (!Client.connected())
    Client.connect(Host, HTTPSPort);
  //Noi dung yeu cau HTTP POST duoc gui
  Payload = PayloadBase + "\"" + FormatDateTime + "," + Temp + "," + Hum + "," + RainF + "," + WindS + "\"}";
  //Gui yeu cau HTTP POST
  Client.print(String("POST ") + "/macros/s/" + ScriptID + "/exec" + " HTTP/1.1\r\n" + "Host: " + Host + "\r\n" + "Content-Type: application/json\r\n" + "Content-Length: " + Payload.length() + "\r\n" + "Connection: close\r\n\r\n" + Payload);
  Client.stop();
}
//Ham in du lieu
void PrintData() {
  Serial.print("Payload: ");
  Serial.println(Payload);
  Serial.print("Date and Time: ");
  Serial.println(FormatDateTime);
  Serial.print("Temperature: ");
  Serial.println(Temp);
  Serial.print("Humidity: ");
  Serial.println(Hum);
  Serial.print("Rainfall: ");
  Serial.println(RainF);
  Serial.print("Wind speed: ");
  Serial.println(WindS);
}
//Ham ghi du lieu cam bien luong mua vao EEPROM
void WriteRainFall() {
  RainTime = GetEpochTimeRTC();
  //Kiem tra chong chia cho 0
  if (RainTime > RainStart) {
    RainIntensity = RainF / ((RainTime - RainStart) / 60.0);
  } else {
    RainIntensity = 0;
  }
  EEPROM.put(Address, RainIntensity);
  EEPROMRainF = RainF;
  EEPROM.put(Address + 4, EEPROMRainF);
  EEPROM.put(Address + 8, RainTime);
  EEPROM.put(AddressBase, Address);
  EEPROM.commit();
  Address += 12;
  if (Address > 4083) {
    Address = 4;
  }
}
//Ham doc du lieu cam bien luong mua tu EEPROM
void ReadRainFall() {
  if (digitalRead(RainPin) == LOW) {
    EEPROM.get(AddressBase, Address);
    EEPROM.get(Address + 8, RainTime);
    ShutdownTime = GetEpochTimeRTC() - RainTime;
    //Neu thoi gian sap nguon duoi 2 tieng, lay du lieu tu EEPROM
    if (ShutdownTime < 7200) {
      EEPROM.get(Address, RainIntensity);
      EEPROM.get(Address + 4, EEPROMRainF);
      //Tinh toan luong mua hien tai sau khi sap nguon
      RainF = EEPROMRainF + (RainIntensity * (ShutdownTime / 60.0));
    }
  }
}
//Ham lay thoi gian Unix tu DS1307
unsigned long GetEpochTimeRTC() {
  if (RTC.isrunning()) {
    DateTime Now = RTC.now();
    return Now.unixtime();
  } else
    return GetEpochTimeNTP();
}
//Ham lay thoi gian Unix tu may chu NTP
unsigned long GetEpochTimeNTP() {
  TimeClient.update();
  return TimeClient.getEpochTime();
}