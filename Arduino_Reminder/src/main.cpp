#include <Arduino.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include <PubSubClient.h>
#include <LinkedList.h>
#include <Wire.h>
#include <Thread.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define WIFI_SERIAL Serial1
#define PORT 1883

char ssid[] = "Miki_san";
char pass[] = "12345678";
int status = WL_IDLE_STATUS;
const char *mqtt_server = "192.168.14.81";
const char *clientId = "ESP32Client";
const char *mqtt_user = "miki";
const char *mqtt_pass = "091101mikisanR";
char msg[20];
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiEspClient espClient;
PubSubClient client(espClient);
tmElements_t tm;
Thread EthernetThread = Thread();
const char *monthName[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

LinkedList<String> notes;
char compileTime[] = __TIME__;

String sec, min, hou, date, mon, note, last_note = "--";
boolean flag = true, flag1 = true;

void printCurrentNet()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  byte mac[6];
  WiFi.macAddress(mac);
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.print("MAC address: ");
  Serial.println(buf);

  Serial.print("Signal strength (RSSI): ");
  Serial.println(WiFi.RSSI());
}

void wifiConnect()
{
  WiFi.init(&WIFI_SERIAL);
  Serial.print("Serial init OK\r\n");
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    while (true)
      ;
  }
  delay(100);
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void wifiSetup()
{
  wifiConnect();
  printCurrentNet();
  Serial.println();
  Serial.println("WIFI setup completed!");
  Serial.println();
}

void receivedCallback(char *topic, byte *payload, unsigned int length)
{
  String s = String((char *)payload);
  notes.add(s.substring(0, length));
  Serial.print("Note received: ");
  Serial.println(notes.get(notes.size() - 1));
  Serial.println();
}

void mqttConnect()
{
  while (!client.connected())
  {
    Serial.print("MQTT connecting ...");
    if (client.connect(clientId, mqtt_user, mqtt_pass))
    {
      Serial.println("Connected");
      client.publish("reminder/commands", "connected");
      client.subscribe("reminder/notes");
    }
    else
    {
      Serial.print("Failed, status code =");
      Serial.print(client.state());
      Serial.println("Try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttSetup()
{
  client.setKeepAlive(120);
  client.setSocketTimeout(120);
  client.setServer(mqtt_server, PORT);
  client.setCallback(receivedCallback);
  mqttConnect();
  Serial.println("MQTT setup completed!");
  Serial.println();
}

bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3)
    return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3)
    return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++)
  {
    if (strcmp(Month, monthName[monthIndex]) == 0)
      break;
  }
  if (monthIndex >= 12)
    return false;
  tm.Day = Day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(Year);
  return true;
}

void dsSetup()
{
  bool parse = false;
  bool config = false;

  if (getDate(__DATE__) && getTime(__TIME__))
  {
    parse = true;
    if (RTC.write(tm))
    {
      config = true;
    }
  }
  if (parse && config)
  {
    Serial.print("DS1307 configured Time=");
    Serial.print(__TIME__);
    Serial.print(", Date=");
    Serial.println(__DATE__);
  }
  else if (parse)
  {
    Serial.println("DS1307 Communication Error! Please check your circuitry.");
  }
  else
  {
    Serial.print("Could not parse info from the compiler, Time=\"");
    Serial.print(__TIME__);
    Serial.print("\", Date=\"");
    Serial.print(__DATE__);
    Serial.println("\"");
  }
}

String checking()
{
  for (int i = 0; i < notes.size(); i++)
  {
    String s = notes.get(i);
    if (s.substring(1, 3).equals(hou) && s.substring(4, 6).equals(min) && s.substring(8, 10).equals(date) && s.substring(11, 13).equals(mon))
    {
        if(flag1){
          last_note = note;
          flag1 = false;
        }
      return s;
    }
  }
  return "NULL";
}

void sendingMQTT()
{
  char *cstr = new char[note.length() + 1];
  strcpy(cstr, note.c_str());
  char *buffer = new char[strlen(cstr) + strlen("delete: ") + 1];
  strcpy(buffer, "delete: ");
  strcat(buffer, cstr);
  client.publish("reminder/commands", buffer);
  flag = false;
  delete[] buffer;
  delete[] cstr;
}

void doEthernet()
{

  switch (status)
  {
  case WL_CONNECTED:
    switch (client.state())
    {
    case MQTT_CONNECTED:
      lcd.setCursor(0, 0);
      note = checking();
      if (note == "NULL"){
        lcd.print("Ready           ");
      }else if(!last_note.equals(note)){ 
        flag = true;
        for(int i = 0; i < notes.size(); i++){
          if(notes.get(i).equals(last_note)){
            notes.remove(i);
            break;
          }
        }
        last_note = note;
      }else{
        if (flag){
          sendingMQTT();
        }
        lcd.print(note.substring(21, note.length() - 2) + "     ");
      }
      break;

    default:
      mqttConnect();
      break;
    }
    break;

  default:
    wifiConnect();
    break;
  }
}

void readTime(tmElements_t tms)
{
  lcd.setCursor(0, 1);
  if (RTC.read(tms))
  {
    sec = "";
    min = "";
    hou = "";
    date = "";
    mon = "";
    //lcd.clear();
    if (tms.Second >= 0 && tms.Second < 10)
    {
      sec += "0";
    }
    if (tms.Minute >= 0 && tms.Minute < 10)
    {
      min += "0";
    }
    if (tms.Hour >= 0 && tms.Hour < 10)
    {
      hou += "0";
    }
    if (tms.Day >= 0 && tms.Day < 10)
    {
      date += "0";
    }
    if (tms.Month >= 0 && tms.Month < 10)
    {
      mon += "0";
    }

    sec += tms.Second;
    min += tms.Minute;
    hou += tms.Hour;
    date += tms.Day;
    mon += tms.Month;

    lcd.print(hou);
    lcd.print(":");
    lcd.print(min);
    lcd.print(":");
    lcd.print(sec);
    lcd.setCursor(11, 1);
    lcd.print(date);
    lcd.print("/");
    lcd.print(mon);
  }
  else
  {
    if (RTC.chipPresent())
    {
      Serial.println("The DS1307 is stopped!");
      Serial.println();
    }
    else
    {
      Serial.println("DS1307 read error!  Please check the circuitry.");
      Serial.println();
    }
    delay(9000);
  }
}

char getInt(const char* string, int startIndex) {
  return int(string[startIndex] - '0') * 10 + int(string[startIndex+1]) - '0';
}
 
void EEPROMWriteInt(int address, int value)
{
  EEPROM.write(address, lowByte(value));
  EEPROM.write(address + 1, highByte(value));
}
 
unsigned int EEPROMReadInt(int address)
{
  byte lowByte = EEPROM.read(address);
  byte highByte = EEPROM.read(address + 1);
 
  return (highByte << 8) | lowByte;
}

void setup()
{
  Serial.begin(115200);
  WIFI_SERIAL.begin(9600);

  lcd.init();
  lcd.backlight();
  wifiSetup();
  mqttSetup();
  byte hour = getInt(compileTime, 0);
  byte minute = getInt(compileTime, 3);
  byte second = getInt(compileTime, 6);
  unsigned int hash =  hour * 60 * 60 + minute  * 60 + second; 
  if (EEPROMReadInt(0) != hash) {
    EEPROMWriteInt(0, hash);
    dsSetup();
  }
  
  Serial.println("Setup completed!");
  Serial.println("___________________________");
  Serial.println();
  EthernetThread.onRun(doEthernet);
  EthernetThread.setInterval(500);
  lcd.clear();
}

void loop()
{
  client.loop();
  readTime(tm);
  if (EthernetThread.shouldRun())
  {
    EthernetThread.run();
  }
}
