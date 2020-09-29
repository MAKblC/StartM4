#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750FVI.h>
#include <MCP3221.h>
#include <Adafruit_MCP9808.h>
#include "TLC59108.h"

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

  setBusChannel(0x05);
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

}

void loop()
{
  // Считывание датчика температуры/влажности почвы
  setBusChannel(0x05);
  tempsensor.wake();
  float t1 = tempsensor.readTempC();
  float h1 = mcp3221_5.getData();
  h1 = map(h1, a, b, 0, 100);
  Serial.println("Temp1 " + String(t1, 1));
  Serial.println("Hum1 " + String(h1, 1));
  delay(2000);
  //считывание датчика освещенности
  float light = bh1750.getAmbientLight();
  Serial.print("Light = ");
  Serial.println(String(light, 1) + " lx");
  delay(2000);
  // Считывание датчика температуры/влажности/давления
  setBusChannel(0x07);
  float air_temp = bme280.readTemperature();
  float air_hum = bme280.readHumidity();
  float air_press = bme280.readPressure() / 100.0F;
  Serial.print("Air temperature = ");
  Serial.println(String(air_temp, 1) + " C");
  Serial.print("Air humidity = ");
  Serial.println(String(air_hum, 1) + " %");
  Serial.print("Air pressure = ");
  Serial.println(String(air_press, 1) + " hPa");
  delay(2000);
  //запуск реле
  setBusChannel(0x03);
  pca9536.setState(IO1, IO_LOW);
  Serial.println("Watering");
  delay(2000);
  pca9536.setState(IO1, IO_HIGH);
  Serial.println("pump off");
  delay(2000);
  // запуск светодиодов
  setBusChannel(0x06);
  leds.setBrightness(6, 0xff);
  leds.setBrightness(0, 0xff);
  Serial.println("white");
  delay(2000);
  leds.setBrightness(6, 0x00);
  leds.setBrightness(0, 0x00);
  leds.setBrightness(1, 0xff);
  leds.setBrightness(4, 0xff);
  Serial.println("ultraviolet");
  delay(2000);
  leds.setBrightness(1, 0x00);
  leds.setBrightness(4, 0x00);
  leds.setBrightness(2, 0xff);
  Serial.println("green");
  delay(2000);
  leds.setBrightness(2, 0x00);
  leds.setBrightness(3, 0xff);
  Serial.println("red");
  delay(2000);
  leds.setBrightness(3, 0x00);
  leds.setBrightness(5, 0xff);
  Serial.println("blue");
  delay(2000);
  leds.setBrightness(5, 0x00);
}
//функция смены I2C-порта
bool setBusChannel(uint8_t i2c_channel)
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
