/*
Не забудьте поправить настройки и адреса устройств в зависимости от комплектации!  
*/

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <MCP3221.h>
#include <Adafruit_MCP9808.h>
#include "TLC59108.h"
#include <PCA9634.h>

#define BLYNK_TEMPLATE_ID "XXXXXXXX"
#define BLYNK_DEVICE_NAME "XXXXXXXX"
#define BLYNK_AUTH_TOKEN "XXXXXXXXXXXXXXXXXXXXXX"

char ssid[] = "XXXXXX"; // логин Wi-Fi
char pass[] = "XXXXXXXXX"; // пароль Wi-Fi
char auth[] = BLYNK_AUTH_TOKEN;

BlynkTimer timer_update; // Таймер обновления данных

// Выберите модуль вашей сборки (ненужные занесите в комментарии)
#define MGL_RGB1 1
//#define MGL_RGB23 1 // MGL-RGB2 или MGL-RGB3

#ifdef MGL_RGB1
#define HW_RESET_PIN 0 // Только програмнный сброс
#define I2C_ADDR TLC59108::I2C_ADDR::BASE
TLC59108 leds(I2C_ADDR + 7); // Без перемычек добавляется 3 бита адреса
// TLC59108 leds(I2C_ADDR + 0); // когда стоят 3 перемычки
#endif
#ifdef MGL_RGB23
PCA9634 testModule(0x08); // (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
#endif

const byte DEV_ADDR_5 = 0x4D; // (0x4E) настройки датчика температуры и влажности почвы
MCP3221 mcp3221_5(DEV_ADDR_5);
int a = 2312;
int b = 1165;
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

BH1750 lightMeter; // Датчик освещенности

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
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);

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
#ifdef MGL_RGB1
  Wire.setClock(10000L);
  leds.init(HW_RESET_PIN);
  leds.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
  byte pwm = 0;
  leds.setAllBrightness(pwm);
#endif
#ifdef MGL_RGB23
  testModule.begin();
  for (int channel = 0; channel < testModule.channelCount(); channel++)
  {
    testModule.setLedDriverMode(channel, PCA9634_LEDOFF); // выключить все светодиоды в режиме 0/1
  }
#endif
  
  lightMeter.begin(); 

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
  Blynk.virtualWrite(V9, String(h1, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V7, String(t1, 1)); delay(25); // отправка данных в проект

  float light = lightMeter.readLightLevel(); //считывание датчика освещенности
  Blynk.virtualWrite(V17, String(light, 1)); delay(25); // отправка данных в проект

  setBusChannel(0x07);
  float t = bme280.readTemperature();  // Считывание датчика температуры/влажности/давления
  float h = bme280.readHumidity();
  float p = bme280.readPressure() / 133.3F;
  Blynk.virtualWrite(V14, String(t, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V15, String(h, 1)); delay(25); // отправка данных в проект
  Blynk.virtualWrite(V16, String(p, 1)); delay(25); // отправка данных в проект
}

BLYNK_WRITE(V6) { //запуск насоса
  int state = param.asInt();
  setBusChannel(0x03);
  if (state == 1) {
    pca9536.setState(IO1, IO_LOW);
  }
  else {
    pca9536.setState(IO1, IO_HIGH);
  }
}

BLYNK_WRITE(V5) { //запуск светодиодов
  int state = param.asInt();
  setBusChannel(0x06);
  if (state == 1) {
#ifdef MGL_RGB1
    leds.setBrightness(6, 0x99);
    leds.setBrightness(0, 0x99);
    leds.setBrightness(1, 0x99);
    leds.setBrightness(4, 0x99);
#endif
#ifdef MGL_RGB23 
    // номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
    testModule.setLedDriverMode(0, PCA9634_LEDON);
    testModule.setLedDriverMode(6, PCA9634_LEDON); 
    testModule.setLedDriverMode(4, PCA9634_LEDON);
    testModule.setLedDriverMode(1, PCA9634_LEDON);
#endif   
  }
  else {
#ifdef MGL_RGB1
    leds.setBrightness(6, 0x00);
    leds.setBrightness(0, 0x00);
    leds.setBrightness(1, 0x00);
    leds.setBrightness(4, 0x00);
#endif
#ifdef MGL_RGB23 
    // номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
    testModule.setLedDriverMode(0, PCA9634_LEDOFF);
    testModule.setLedDriverMode(6, PCA9634_LEDOFF); 
    testModule.setLedDriverMode(4, PCA9634_LEDOFF);
    testModule.setLedDriverMode(1, PCA9634_LEDOFF);
#endif 
  }
}

BLYNK_WRITE(V1) { //запуск RGB
  int r = param.asInt();
  setBusChannel(0x06);
#ifdef MGL_RGB1
  leds.setBrightness(3, r);
#endif
#ifdef MGL_RGB23 
// номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
  testModule.setLedDriverMode(channel, PCA9634_LEDPWM); // установка режима ШИМ  
  testModule.write1(3, r);
#endif  
}
BLYNK_WRITE(V8) { //запуск RGB
  int g = param.asInt();
  setBusChannel(0x06);
#ifdef MGL_RGB1
  leds.setBrightness(2, g);
#endif
#ifdef MGL_RGB23
// номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
  testModule.setLedDriverMode(channel, PCA9634_LEDPWM); // установка режима ШИМ  
   testModule.write1(2, g);
#endif
}
BLYNK_WRITE(V3) { //запуск RGB
  int b = param.asInt();
  setBusChannel(0x06);
#ifdef MGL_RGB1
  leds.setBrightness(5, b);
#endif
#ifdef MGL_RGB23
// номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
  testModule.setLedDriverMode(channel, PCA9634_LEDPWM); // установка режима ШИМ  
  testModule.write1(5, b);
#endif
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
