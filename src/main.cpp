#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.
const char *ssid = "the dude-net";
const char *password = "iR3DNw8ZFk-t9e3ixVJjhAE-2d9374H9sw5-Sv99fC645C2-6G4359L463tY";
const char *mqtt_server = "10.0.0.10";
const char *SensorName = "SmartMeterCounter";
const char *version = "SMC v1.00 interrupt";

// NTP stuff
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "10.0.0.1", 3600, 600000);

// Variables
const byte pin = 4;
long unsigned counter;
long unsigned StartTime;
long unsigned dailyCounter; // counts increments for one day, gets reset daily
float dailyPower;
float tdelta;
float Power;
float upTime;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
  delay(100);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname("ESP8266-SM");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    if (client.connect(SensorName, "mark", "8749"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 6 seconds before retrying
      delay(6000);
    }
  }
}

ICACHE_RAM_ATTR void IncrementCount()
{
  counter++;
  if (counter == 1)
  {
    StartTime = millis();
  }
  dailyCounter++;
}

void setup()
{
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Starting up...");
  pinMode(pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin), IncrementCount, FALLING);
  setup_wifi();
  client.setServer(mqtt_server, 1885);
  timeClient.begin();
  Serial.println(version);
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
  if (counter == 10)
  {
    tdelta = millis() - StartTime;
    Power = 1 / (tdelta / 3600000);
    dailyPower = float(dailyCounter) / 10000;
    counter = 0;

    upTime = (float(millis()) / (60 * 60 * 24 * 1000));

    client.publish("SmartMeter/Leistung", String(int(Power)).c_str(), true);
    client.publish("SmartMeter/TagesLeistung", String(dailyPower).c_str(), true);
    client.publish("SmartMeter/DEV_dailyCounter", String(dailyCounter).c_str(), true);
    client.publish("SmartMeter/DEV_Wifi_RSSI", String(WiFi.RSSI()).c_str(), true);
    client.publish("SmartMeter/DEV_Uptime", String(upTime, 3).c_str(), true);
    client.publish("SmartMeter/DEV_Version", version, true);
  }

  // Calculation of daily power usage  & MQTT transmit
  if (timeClient.getHours() == 23 && timeClient.getMinutes() == 59 && timeClient.getSeconds() == 59)
  {
    if (upTime >= 1)
    {
      dailyPower = float(dailyCounter) / 10000;
      client.publish("SmartMeter/LeistungVorTag", String(dailyPower).c_str(), true);
    }

    dailyCounter = 0;
    delay(1000);
  }
}

/*
Circuit:
Pin 3V3 --------- R=1kOhm ---------- Pin D2
                    |
                    |
                    |+      -
            Phototransistor -------- Pin GND

Function:
Phototransistor pulls D2 to GND on increment
*/