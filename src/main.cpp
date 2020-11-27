/*
Function:
Phototransistor pulls selected digital pin to GND on increment from smartmeter. Current setup is for smartmeters which send 10.000 increments per 1 kWh.
*/

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define DEVmessages 1 // change 1 to 0 if you don´t want MQTT DEV messages

// Wifi and mqtt network settings
const char *ssid = "........";               //
const char *password = "........";           //
const char *HostName = "ESP8266-SMdual";     //
const char *mqtt_server = "10.0.0.88";       // ip address or hostname of your mqtt broker
const char *mqtt_user = "........";          //
const char *mqtt_pass = "........";          //
const unsigned int port = 1883;              // regular port is 1883
const char *SensorName = "SMDual";           // will be the name in yout mqtt broker
const char *NTPserver = "0.de.pool.ntp.org"; // use one close to you, sometimes your router acts as one
const char *version = "SMcounter v1.00";     //

// NTP stuff
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTPserver, 3600, 600000);

// Variables
const byte pinD1 = 5;        // digital pins the phototransistors are connected to
const byte pinD2 = 4;        //
long unsigned counterA;      //
long unsigned counterB;      //
long unsigned StartTimeA;    //
long unsigned StartTimeB;    //
long unsigned dailyCounterA; // counts increments for one day, gets reset daily
long unsigned dailyCounterB; //
float dailyPowerA;           // Power that has been used the present day in [kWh]
float dailyPowerB;           //
float tdeltaA;               //
float tdeltaB;               //
float PowerA;                // currently used power in [W]
float PowerB;                //
float upTime;                //

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
  delay(100);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname(HostName);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected!");
  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection... ");

    if (client.connect(SensorName, mqtt_user, mqtt_pass))
    {
      Serial.print("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(6000);
    }
  }
}

ICACHE_RAM_ATTR void IncrementCountA()
{
  counterA++;
  if (counterA == 1)
  {
    StartTimeA = millis();
  }
  dailyCounterA++;
}

ICACHE_RAM_ATTR void IncrementCountB()
{
  counterB++;
  if (counterB == 1)
  {
    StartTimeB = millis();
  }
  dailyCounterB++;
}

void setup()
{
  Serial.begin(9600);
  Serial.println(" ");
  Serial.println("Starting up...");
  pinMode(pinD1, INPUT);
  pinMode(pinD2, INPUT);
  setup_wifi();
  client.setServer(mqtt_server, port); // regular port is 1883
  timeClient.begin();
  Serial.println(version);
  attachInterrupt(digitalPinToInterrupt(pinD1), IncrementCountA, RISING);
  attachInterrupt(digitalPinToInterrupt(pinD2), IncrementCountB, RISING);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();
  timeClient.update();

  // Calculation of current power & MQTT transmit
  if (counterA >= 10)
  {
    tdeltaA = millis() - StartTimeA;
    PowerA = 1 / (tdeltaA / 3600000);
    dailyPowerA = float(dailyCounterA) / 10000;
    counterA = 0;

    upTime = (float(millis()) / (60 * 60 * 24 * 1000));

    client.publish("SMdual/PowerA", String(int(PowerA)).c_str(), true);
    client.publish("SMdual/DailyPowerA", String(dailyPowerA).c_str(), true);

    if (DEVmessages == 1)
    {
      client.publish("SMdual/DEV_dailyCounterA", String(dailyCounterA).c_str(), true);
      client.publish("SMdual/DEV_Wifi_RSSI", String(WiFi.RSSI()).c_str(), true);
      client.publish("SMdual/DEV_Uptime", String(upTime, 2).c_str(), true);
      client.publish("SMdual/DEV_Version", version, true);
    }
  }

  if (counterB >= 10)
  {
    tdeltaB = millis() - StartTimeB;
    PowerB = 1 / (tdeltaB / 3600000);
    dailyPowerB = float(dailyCounterB) / 10000;
    counterB = 0;

    upTime = (float(millis()) / (60 * 60 * 24 * 1000));

    client.publish("SMdual/PowerB", String(int(PowerB)).c_str(), true);
    client.publish("SMdual/DailyPowerB", String(dailyPowerB).c_str(), true);

    if (DEVmessages == 1)
    {
      client.publish("SMdual/DEV_dailyCounterB", String(dailyCounterB).c_str(), true);
      client.publish("SMdual/DEV_Wifi_RSSI", String(WiFi.RSSI()).c_str(), true);
      client.publish("SMdual/DEV_Uptime", String(upTime, 2).c_str(), true);
      client.publish("SMdual/DEV_Version", version, true);
    }
  }

  // Calculation of daily power usage  & MQTT transmit
  if (timeClient.getHours() == 23 && timeClient.getMinutes() == 59 && timeClient.getSeconds() == 59)
  {
    if (upTime >= 1)
    {
      dailyPowerA = float(dailyCounterA) / 10000;
      dailyPowerB = float(dailyCounterB) / 10000;
      client.publish("SMdual/PowerPreviousDayA", String(dailyPowerA).c_str(), true);
      client.publish("SMdual/PowerPreviousDayB", String(dailyPowerB).c_str(), true);
    }

    dailyCounterA = 0;
    dailyCounterB = 0;
    delay(1000);
  }
}