#define BLYNK_TEMPLATE_ID "TMPL38Em61Mhb"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation"
#define BLYNK_AUTH_TOKEN "HfsAG7y86Big5Qo34np8P8sAOpR_Ec_G"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WIFI
char ssid[] = "S M34";
char pass[] = "1234567890";

// PINS
#define DHT_PIN 4
#define SOIL_PIN 34
#define RELAY 23

#define RELAY_ON HIGH
#define RELAY_OFF LOW

// OBJECTS
DHT dht(DHT_PIN, DHT11);
WiFiClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ThingSpeak
unsigned long channelID = 3338195;
const char *writeAPIKey = "F8YOQQXREKA1R2VY";

// Flask API
const char *server = "http://10.236.101.242:5000/predict";

// VARIABLES
bool manualPump = false;
unsigned long lastRead = 0;

// Blynk button
BLYNK_WRITE(V0)
{
    manualPump = param.asInt();
}

// ML FUNCTION
int getPrediction(float sm, float temp, float hum)
{
    HTTPClient http;
    http.begin(server);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"sm\":" + String(sm) +
                  ",\"temp\":" + String(temp) +
                  ",\"hum\":" + String(hum) + "}";

    int code = http.POST(json);

    Serial.print("HTTP: ");
    Serial.println(code);

    if (code == 200)
    {
        String response = http.getString();
        Serial.println(response);

        if (response.indexOf("\"pump\":1") != -1)
        {
            http.end();
            return 1;
        }
    }

    http.end();
    return 0;
}

void setup()
{
    Serial.begin(115200);

    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, RELAY_OFF);

    dht.begin();

    lcd.init();
    lcd.backlight();

    WiFi.begin(ssid, pass);
    Serial.print("Connecting WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    ThingSpeak.begin(client);
}

void loop()
{
    Blynk.run();

    if (millis() - lastRead > 15000)
    {
        lastRead = millis();

        // STABLE SOIL READING (AVERAGE)
        int sum = 0;
        for (int i = 0; i < 5; i++)
        {
            sum += analogRead(SOIL_PIN);
            delay(10);
        }
        int raw = sum / 5;

        Serial.print("RAW: ");
        Serial.println(raw);

        // CORRECT MAPPING
        int soil = map(raw, 3000, 1500, 100, 0);
        // int soil = map(raw,0, 100, 1500, 3000);

        soil = constrain(soil, 100, 0);

        float temp = dht.readTemperature();
        float hum = dht.readHumidity();

        if (isnan(temp))
            temp = 25;
        if (isnan(hum))
            hum = 50;

        // ML CALL
        int mlPump = getPrediction(soil, temp, hum);
        if (mlPump)
            mlPump = 0;
        else
            mlPump = 1;

        //  FINAL DECISION
        bool pump = manualPump ? 1 : mlPump;

        digitalWrite(RELAY, pump ? RELAY_OFF : RELAY_ON);

        // LCD DISPLAY
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SM:");
        lcd.print(soil);
        lcd.print("%");

        lcd.setCursor(0, 1);
        if (pump)
            lcd.print("Motor: ON ");
        else
            lcd.print("Motor: OFF");

        // BLYNK
        Blynk.virtualWrite(V1, soil);
        Blynk.virtualWrite(V2, temp);
        Blynk.virtualWrite(V3, hum);
        Blynk.virtualWrite(V4, pump);

        // THINGSPEAK
        ThingSpeak.setField(1, soil);
        ThingSpeak.setField(2, temp);
        ThingSpeak.setField(3, hum);
        ThingSpeak.setField(4, pump);
        ThingSpeak.writeFields(channelID, writeAPIKey);

        // SERIAL
        Serial.print("Soil: ");
        Serial.print(soil);
        Serial.print(" Temp: ");
        Serial.print(temp);
        Serial.print(" Hum: ");
        Serial.print(hum);
        Serial.print(" Pump: ");
        Serial.println(pump ? "OFF" : "ON");
    }
}