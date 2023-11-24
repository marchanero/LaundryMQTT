#include <Arduino.h>
// led on pin 8
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// *********************************************************************************************************;
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//*********************************************** OLED ***********************************************************
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//*********************************************** Wifi and MQTT ***********************************************************
// WiFi
// const char *ssid = "DIGIFIBRA-cF5T";
// const char *password = "P92sKt3FGfsy";

const char *ssid = "Armadillo";
const char *password = "armadillolan";

// MQTT Broker
const char *mqtt_broker = "192.168.1.43";
const char *topic = "casa/lavadora";
const char *mqtt_username = "root";
const char *mqtt_password = "orangepi.hass";
const int mqtt_port = 1883;

String client_id = "lavadora-";

// *********************************************************************************************************;

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
void displayMQTTerror(int mqtterror);
void loop2(void *parameter);

#define LED 8

unsigned long ShakeTime;
unsigned long NoShakeTime;
unsigned long CurrentTime;
unsigned long PreviousTime;

// stablish D6 as the pin where the sensor is connected
#define ShakeSensorDig 10
#define ShakeSensorAn 0

int sensorState = 0;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(ShakeSensorAn, INPUT);
  pinMode(ShakeSensorDig, INPUT);

  Serial.println("Iniciando...");
  // Connecting to a WiFi network
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  // WiFi
  // WiFi.mode(WIFI_STA); // Optional
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println("\nConnecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED, HIGH);
    delay(500);
    Serial.println("Connecting to WiFi..");
    // show in display oled
    digitalWrite(LED, LOW);
    display.setCursor(0, 20);
    display.println("Connecting to WiFi..");
    display.display();
  }

  Serial.println("Connected to the Wi-Fi network");
  // print the local IP address to serial monitor
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // print mac address
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  display.display();

  delay(5000);

  client.setKeepAlive(20860);
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  client_id = client_id + String(WiFi.macAddress());
  while (!client.connected())
  {
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    Serial.println();
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Conectado a Casa MQTT Broker!");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.println(client.state());
      Serial.println("Trying again in 5 seconds");
    }
    delay(2000);
  }

  // Publish and subscribe
  client.publish(topic, "Hi, I'm Lavadora ^^");
}

int counter = 0;

void loop()
{
  Serial.println("Shake Sensor");
  Serial.println("============");
  Serial.print("Analog Value: ");
  Serial.println(analogRead(ShakeSensorAn));
  Serial.println("============");
  Serial.println("");

  if (analogRead(ShakeSensorAn) < 3500)
  {
    Serial.println("Shake Detected");
    Serial.println("==============");
    Serial.println("");
    sensorState = 1;
    client.publish(topic, "Shake Detected");
    counter++;
    Serial.println("Counter: " + String(counter));
    Serial.println("==============");
  }
  else
  {
    Serial.println("No Shake Detected");
    Serial.println("=================");
    Serial.println("");
    sensorState = 0;
  }
  delay(50);
  // create if no shake detected in 10 minutes send message to finish using millis
  // if shake detected reset millis
  // if millis > 10 minutes send message to finish using millis

  if (sensorState == 1)
  {
    CurrentTime = millis();
    if (CurrentTime - PreviousTime > 600000)
    {
      Serial.println("10 minutes without shake");
      Serial.println("Sending message to finish");
      client.publish(topic, "La lavadora ha terminado");
      PreviousTime = CurrentTime;
    }
  }
  else
  {
    PreviousTime = millis();
  }
}

void reconnect()
{
  int contador_error = 0;
  Serial.println("Bucle Reconectar");
  // Loop hasta que estemos reconectados
  while (!client.connected())
  {
    Serial.println("Intentando conexión MQTT...");
    // Intentar conectar
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("conectado");
    }
    else
    {
      Serial.print("COmunicacion MQTT falló, rc=");
      Serial.println(client.state());
      Serial.println("Intentar de nuevo en 5 segundos");
      delay(500);

      // Esperar 5 segundos antes de volver a intentar
      displayMQTTerror(client.state());

      delay(5000);
      contador_error++;
      if (contador_error > 10)
      {
        Serial.println("Reiniciando ESP");

        ESP.restart();
      }
      Serial.println("Intento " + String(contador_error));
    }
  }
}
bool checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}
void displayMQTTerror(int mqtterror)
{
  display.clearDisplay();
  display.setCursor(0, 5);
  display.println("MQTT: ");
  display.setCursor(0, 20);
  display.println("Error: " + String(mqtterror));
  display.setCursor(0, 30);
  display.println("Reintentando...");
  display.display();
  delay(2000);
}