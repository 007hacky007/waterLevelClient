
#include "DHT.h"
#include "WiFi.h"
#include "driver/adc.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile
#include <Pangodream_18650_CL.h>

#define DHTPIN 22
#define DHTTYPE DHT22
#define DHT_MAX_RETRY 100
#define VOLTMETER_PIN 26

#define DEEP_SLEEP_TIME 15 // in minutes

#define trigPin 23
#define echoPin 19
#define ultraSonicVcc 18

/*
    This code works only with ESP 1.5.0-rc2 version */

DHT dht(DHTPIN, DHTTYPE);
Pangodream_18650_CL BL(VOLTMETER_PIN, 1.745);
RH_ASK driver(2000, 11, 15);

float hum;
float temp;

long duration;
int distance;

int batteryChargeLevel;
double batteryVolts;

void turnOffWifiBt() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
}

void goToDeepSleep(int sleep_time = 1)
{
  Serial.println("Going to sleep...");
  turnOffWifiBt();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  // Configure the timer to wake us up!
  esp_sleep_enable_timer_wakeup(sleep_time * 60L * 1000000L);

  // Go to sleep! Zzzz
  esp_deep_sleep_start();
}

void getTempHum() {
  int retry = 0;
  float hum_new;
  float temp_new;
  while (true) {
    if (retry > DHT_MAX_RETRY) {
      // TODO: handle bad data. Send -1? Or send 0? Would not work in case of temp.
      Serial.println("Max number of retries reached. Aborting");
      goToDeepSleep(1);
    }

    hum_new = dht.readHumidity();
    temp_new = dht.readTemperature();

    if (isnan(hum_new) || isnan(temp_new))
    {
      Serial.println("Could not get DHT data");
      delayMicroseconds(1000000);
    }
    else
    {
      hum = hum_new;
      temp = temp_new;
      Serial.print("Vlhkost: ");
      Serial.print(hum);
      Serial.print(" %, Teplota: ");
      Serial.print(temp);
      Serial.println(" Celsius");
      break;
    }
  }
}

void getDistance() {
  int retry = 0;
  digitalWrite(ultraSonicVcc, HIGH);
  delay(1000);
  while (true) {
    // Clear the trigPin by setting it LOW:
    digitalWrite(trigPin, LOW);

    delayMicroseconds(5);
    // Trigger the sensor by setting the trigPin high for 20 microseconds:
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(trigPin, LOW);

    // Read the echoPin. pulseIn() returns the duration (length of the pulse) in microseconds:
    duration = pulseIn(echoPin, HIGH);

    // Calculate the distance:
    distance = duration * 0.034 / 2;
    retry++;
    if (distance != 0 || retry > 3) {
      break;
    }
  }
  digitalWrite(ultraSonicVcc, LOW);

  // Print the distance on the Serial Monitor (Ctrl+Shift+M):
  Serial.print("Distance = ");
  Serial.print(distance);
  Serial.println(" cm");
}

void getBattInfo() {
  BL.pinRead();
  batteryVolts = BL.getBatteryVolts();
  batteryChargeLevel = BL.getBatteryChargeLevel();
  Serial.print("Volts: ");
  Serial.println(batteryVolts);
  Serial.print("Charge level: ");
  Serial.print(batteryChargeLevel);
  Serial.println("%");
}

void sendData433() {
  String message = "";
  String msg;
  int length;
  char cstr[100];
  message = String(hum) + "," + String(temp) + "," + String(distance) + "," +  String(batteryChargeLevel) + "," + String(batteryVolts);
  length = message.length();
  msg = message;
  msg.toCharArray(cstr, 100);

  driver.send((uint8_t *)cstr, length);
  driver.waitPacketSent();
}

void setup()
{
  turnOffWifiBt();
  setCpuFrequencyMhz(80);
  adc_power_on();
  Serial.begin(115200);
  dht.begin();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ultraSonicVcc, OUTPUT);
  if (!driver.init())
    Serial.println("RH ASK init failed");
}

void loop()
{
  getDistance();
  getTempHum();
  getBattInfo();
  sendData433();
  goToDeepSleep(DEEP_SLEEP_TIME);
}
