#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750FVI.h>
#include <MCP3221.h>
#include <Adafruit_MCP9808.h>
#include "TLC59108.h"

char ssid[] = "LOGIN"; // логин Wi-Fi
char pass[] = "PASSWORD"; // пароль Wi-Fi
char auth[] = "TOKEN"; // Ваш токен из почты
IPAddress blynk_ip(139, 59, 206, 133);
BlynkTimer timer_update; // Таймер обновления данных

#define HW_RESET_PIN 0 // Только програмнный сброс
#define I2C_ADDR TLC59108::I2C_ADDR::BASE
TLC59108 leds(I2C_ADDR + 7); // Без перемычек добавляется 3 бита адреса
// TLC59108 leds(I2C_ADDR + 0); // когда стоят 3 перемычки

const byte DEV_ADDR_5 = 0x4D; // настройки датчика температуры и влажности почвы
MCP3221 mcp3221_5(DEV_ADDR_5);
int a = 2312;
int b = 1165;
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

BH1750FVI bh1750; // Датчик освещенности

Adafruit_BME280 bme280; // Датчик температуры/влажности и атмосферного давления

#define I2C_HUB_ADDR        0x70 // настройки I2C для платы MGB-I2C63EN
#define EN_MASK             0x08
#define DEF_CHANNEL         0x00
#define MAX_CHANNEL         0x08

// I2C порт 0x07 - выводы D4 (SDA), D5 (SCL)
// I2C порт 0x06 - выводы D6 (SDA), D7 (SCL)
// I2C порт 0x05 - выводы D8 (SDA), D9 (SCL)
// I2C порт 0x04 - выводы D10 (SDA), D11 (SCL)
// I2C порт 0x03 - выводы D12 (SDA), D13 (SCL)

#include "PCA9536.h" // Выходы реле
PCA9536 pca9536;

void setup()
{
  Serial.begin(115200);
  delay(512);
  Wire.begin();

  Serial.print("Connecting to "); // запуск соединения с Blynk
  Serial.println(ssid);
  Blynk.begin(auth, ssid, pass, blynk_ip, 8442);
  delay(1024);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
 ////// запуск датчиков и исп. устройств //////
  setBusChannel(0x04);
  mcp3221_5.setAlpha(DEFAULT_ALPHA);
  mcp3221_5.setNumSamples(DEFAULT_NUM_SAMPLES);
  mcp3221_5.setSmoothing(ROLLING_AVG);
  if (!tempsensor.begin(0x18))
  {
    Serial.println("Couldn't find MCP9808!");
    while (1);
  }
  tempsensor.setResolution(3);

  setBusChannel(0x03);
  pca9536.reset();
  pca9536.setMode(IO_OUTPUT);
  pca9536.setState(IO1, IO_HIGH);
  pca9536.setState(IO0, IO_HIGH);

  setBusChannel(0x06);
  Wire.setClock(10000L);
  leds.init(HW_RESET_PIN);
  leds.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
  byte pwm = 0;
  leds.setAllBrightness(pwm);

  bh1750.begin();
  bh1750.setMode(Continuously_High_Resolution_Mode);

  setBusChannel(0x07);
  bool bme_status = bme280.begin();
  if (!bme_status)
    Serial.println("Could not find a valid BME280 sensor, check wiring!");

  timer_update.setInterval(1000, readSendData); // настройка таймера с параметрами (период обновления, вызываемая функция)
}

void loop() // отслеживание работы таймера и Blynk
{
  Blynk.run(); 
  timer_update.run();
}
void readSendData() // обновление данных каждую секунду
{
  setBusChannel(0x04);
  tempsensor.wake(); // Считывание датчика температуры/влажности почвы
  float t1 = tempsensor.readTempC();
  float h1 = mcp3221_5.getData();
  h1 = map(h1, a, b, 0, 100);
  Blynk.virtualWrite(V7, String(h1, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V6, String(t1, 1)); delay(25); // отправка данных в проект

  float light = bh1750.getAmbientLight(); //считывание датчика освещенности
  Blynk.virtualWrite(V8, String(light, 1)); delay(25); // отправка данных в проект

  setBusChannel(0x07);
  float t = bme280.readTemperature();  // Считывание датчика температуры/влажности/давления
  float h = bme280.readHumidity();
  float p = bme280.readPressure() / 133.3F;
  Blynk.virtualWrite(V3, String(t, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V4, String(h, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V5, String(p, 1)); delay(25); // отправка данных в проект
}

BLYNK_WRITE(V0) { //запуск насоса
  int state = param.asInt();
  setBusChannel(0x03);
  if (state == 1) {
    pca9536.setState(IO1, IO_LOW);
  }
  else {
    pca9536.setState(IO1, IO_HIGH);
  }
}

BLYNK_WRITE(V1) { //запуск светодиодов
  int state = param.asInt();
  setBusChannel(0x06);
  if (state == 1) {
    leds.setBrightness(6, 0x99);
    leds.setBrightness(0, 0x99);
    leds.setBrightness(1, 0x99);
    leds.setBrightness(4, 0x99);
  }
  else {
    leds.setBrightness(6, 0x00);
    leds.setBrightness(0, 0x00);
    leds.setBrightness(1, 0x00);
    leds.setBrightness(4, 0x00);
  }
}

BLYNK_WRITE(V2) { //запуск RGB
  int r = param[0].asInt();
  int g = param[1].asInt();
  int b = param[2].asInt();
  setBusChannel(0x06);
  leds.setBrightness(3, r);
  leds.setBrightness(2, g);
  leds.setBrightness(5, b);
}

bool setBusChannel(uint8_t i2c_channel) //функция смены I2C-порта
{
  if (i2c_channel >= MAX_CHANNEL)
  {
    return false;
  }
  else
  {
    Wire.beginTransmission(I2C_HUB_ADDR);
    Wire.write(i2c_channel | EN_MASK);
    Wire.endTransmission();
    return true;
  }
}
