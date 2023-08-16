/*
Не забудьте поправить настройки и адреса устройств в зависимости от комплектации!  
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <MCP3221.h>
#include <Adafruit_MCP9808.h>
#include "TLC59108.h"
#include <PCA9634.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

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

#define WIFI_SSID "XXXXXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXX"
#define BOT_TOKEN "XXXXXXXXX:XXXXXXXXXXXXXXXX"

const unsigned long BOT_MTBS = 1000; // период обновления сканирования новых сообщений
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

const byte DEV_ADDR_5 = 0x4D; //(0x4E) настройки датчика температуры и влажности почвы
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

// ссылка для поста фотографии
String test_photo_url = "https://mgbot.ru/upload/logo-r.png";

// отобразить кнопки перехода на сайт с помощью InlineKeyboard
String keyboardJson1 = "[[{ \"text\" : \"Ваш сайт\", \"url\" : \"https://mgbot.ru\" }],[{ \"text\" : \"Перейти на сайт IoTik.ru\", \"url\" : \"https://www.iotik.ru\" }]]";

void setup()
{
  Serial.begin(115200);
  delay(512);
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Инициализация I2C
  Wire.begin();

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
void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}

void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";
    if ((text == "/sensors") || (text == "sensors"))
    {
      setBusChannel(0x04);
      tempsensor.wake();
      float t1 = tempsensor.readTempC();
      float h1 = mcp3221_5.getData();
      h1 = map(h1, a, b, 0, 100);
      float light = lightMeter.readLightLevel();
      setBusChannel(0x07);
      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;
      String welcome = "Показания датчиков:\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      welcome += "☁ Атмосферное давление: " + String(p, 0) + " гПа\n";
      welcome += "☀ Освещенность: " + String(light) + " Лк\n";
      welcome += "🌱 Температура почвы: " + String(t1, 0) + " °C\n";
      welcome += "🌱 Влажность почвы: " + String(h1, 0) + " %\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
    if ((text == "/pumpon") || (text == "pumpon"))
    {
      setBusChannel(0x03);
      pca9536.setState(IO1, IO_LOW);
      bot.sendMessage(chat_id, "Насос включен", "");
    }
    if ((text == "/pumpoff") || (text == "pumpoff"))
    {
      setBusChannel(0x03);
      pca9536.setState(IO1, IO_HIGH);
      bot.sendMessage(chat_id, "Насос выключен", "");
    }
    if (text == "/photo") { // пост фотографии
      bot.sendPhoto(chat_id, test_photo_url, "а вот и фотка!");
    }
    if ((text == "/light") || (text == "light"))
    {
      setBusChannel(0x06);
#ifdef MGL_RGB1
      leds.setBrightness(6, 0x99);
      leds.setBrightness(0, 0x99);
#endif   
#ifdef MGL_RGB23 
      // номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
      testModule.setLedDriverMode(0, PCA9634_LEDON);
      testModule.setLedDriverMode(6, PCA9634_LEDON); 
#endif 
      bot.sendMessage(chat_id, "Свет включен", "");
    }
    if ((text == "/off") || (text == "off"))
    {
      setBusChannel(0x06);
#ifdef MGL_RGB1    
      leds.setBrightness(6, 0x00);
      leds.setBrightness(0, 0x00);
      leds.setBrightness(3, 0x00);
      leds.setBrightness(2, 0x00);
      leds.setBrightness(5, 0x00);
#endif   
#ifdef MGL_RGB23 
      // номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
      testModule.setLedDriverMode(0, PCA9634_LEDOFF);
      testModule.setLedDriverMode(6, PCA9634_LEDOFF); 
      testModule.setLedDriverMode(3, PCA9634_LEDOFF);
      testModule.setLedDriverMode(2, PCA9634_LEDOFF); 
      testModule.setLedDriverMode(5, PCA9634_LEDOFF);
#endif 
      bot.sendMessage(chat_id, "Свет выключен", "");
    }
    if ((text == "/color") || (text == "color"))
    {
      setBusChannel(0x06);
#ifdef MGL_RGB1  
      leds.setBrightness(3, random(0, 255));
      leds.setBrightness(2, random(0, 255));
      leds.setBrightness(5, random(0, 255));
#endif  
#ifdef MGL_RGB23 
      // номер канала может быть другим: см. https://github.com/MAKblC/Codes/tree/master/MGL-RGB3
      testModule.setLedDriverMode(channel, PCA9634_LEDPWM); // установка режима ШИМ   
      testModule.write1(3, random(0, 255));
      testModule.write1(2, random(0, 255));
      testModule.write1(5, random(0, 255));
#endif 
      bot.sendMessage(chat_id, "Включен случайный цвет", "");
    }
    if (text == "/site") // отобразить кнопки в диалоге для перехода на сайт
    {
      bot.sendMessageWithInlineKeyboard(chat_id, "Выберите действие", "", keyboardJson1);
    }
    if (text == "/options") // клавиатура для управления теплицей
    {
      String keyboardJson = "[[\"/light\", \"/off\"],[\"/color\",\"/sensors\"],[\"/pumpon\", \"/pumpoff\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }

    if ((text == "/start") || (text == "start") || (text == "/help") || (text == "help"))
    {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Команды смотрите в меню слева от строки ввода", "");
      String sms = "Команды:\n";
      sms += "/options - пульт управления\n";
      sms += "/site - перейти на сайт\n";
      sms += "/photo - запостить фото\n";
      sms += "/help - вызвать помощь\n";
      bot.sendMessage(chat_id, sms, "Markdown");
    }
  }
}
