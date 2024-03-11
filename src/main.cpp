#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "creds.h"

WebServer server(80);

const int led = 13;

int pinModes[256]; // max 256 pins

void handleRoot();
void handleNotFound();
void handleData();
void handleDataGet(int pini, bool digital);
void handleDataPut(int pini, bool digital);
void handleSettings();
bool handleSettingsIo(String q);
int getIo(int pin);
bool setIo(int pin, String val);
int stof(String s);
bool send400ifnull(String name, String value);
void send400args(String name, String value);
void send405();
void drawGraph();

void setup(void)
{
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/test.svg", drawGraph);
  server.on("/inline", []()
            { server.send(200, "text/plain", "this works as well"); });
  server.on("/data", handleData);
  server.on("/opts", handleSettings);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void)
{
  server.handleClient();
  delay(2); // allow the cpu to switch to other tasks
}

void handleRoot()
{
  digitalWrite(led, 1);
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP32!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",

           hr, min % 60, sec % 60);
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound()
{
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void handleData()
{
  String pin = server.arg("pin");
  if (send400ifnull("pin", pin))
    return;

  int pini = pin.toInt();
  if (pini == 0)
  {
    send400args("pin", pin);
    return;
  }

  String mode = server.arg("mode");
  bool digital = false;

  if (mode == "digital")
  {
    digital = true;
  }
  else if (mode != "digital")
  {
    send400args("mode", mode);
  }

  switch (server.method())
  {
  case HTTP_GET:
    handleDataGet(pini, digital);
    break;
  case HTTP_PUT:
    handleDataPut(pini, digital);
    break;
  default:
    send405();
    break;
  }
}

void handleDataGet(int pini, bool digital) {
  if (digital) {
    server.send(501, "text/plain", "Not Implemented\n\nanalog works tho");
  }
  String len = server.arg("len");
  if (send400ifnull("len", len))
    return;

  int leni = len.toInt();
  
  if (leni == 0)
  {
    send400args("len", len);
    return;
  }

  String resp = "";

  for (size_t i = 0; i < leni; i++)
  {
    resp += String(analogRead(pini), DEC) + ",";
  }

  server.send(200, "audio/csv", resp.substring(0, resp.length() - 1));
}

void handleDataPut(int pini, bool digital) {
  // TODO: implement this
  server.send(501, "text/plain", "Not Implemented");

  // TODO: add arg for form of data, raw, frequency, fft

  /*
  String val = server.arg("val");
  if (send400ifnull("val", val))
    return;

  int vali = stof(val);
  if (vali == 0)
  {
    send400args("val", val);
    return;
  }
  */
}

void handleSettings()
{

  String q = server.arg("q");
  if (send400ifnull("q", q))
    return;

  if (handleSettingsIo(q))
    return; // if (q != io) return
}

bool handleSettingsIo(String q)
{
  if (q != "io" && q != "IO" && q != "Io" && q != "iO")
  {
    return 1;
  }
  String pin = server.arg("pin");
  if (send400ifnull("pin", pin))
    return 0;

  int pini = pin.toInt();
  if (pini == 0)
  {
    send400args("pin", pin);
    return 0;
  }

  String val = server.arg("val");

  switch (server.method())
  {
  case HTTP_GET:;
    server.send(200, "text/plain", String((getIo(pini))));
    break;
  case HTTP_POST:
    if (send400ifnull("val", val))
      return 0;

    if (!setIo(pini, val))
    {
      send400args("pin;val", pin + ";" + val);
      return 0;
    }
    server.send(200, "text/plain", "OK");
    break;
  default:
    send405();
    break;
  }
  return 0;
}

int getIo(int pin)
{
  return pinModes[pin];
}

bool setIo(int pin, String val)
{
  int vali = val.toInt();
  switch (vali)
  {
  case (INPUT | OUTPUT | PULLUP | PULLDOWN | INPUT_PULLUP | INPUT_PULLDOWN | OPEN_DRAIN | OUTPUT_OPEN_DRAIN | ANALOG):
    pinMode(pin, vali);
    pinModes[pin] = vali;
    return true;
  }
  int valsi = stof(val);
  if (valsi == -1)
    return false;
  pinMode(pin, valsi);
  pinModes[pin] = valsi;
  return true;
}

int stof(String val)
{
  if (val == "INPUT")
    return INPUT;
  if (val == "OUTPUT")
    return OUTPUT;
  if (val == "PULLUP")
    return PULLUP;
  if (val == "PULLDOWN")
    return PULLDOWN;
  if (val == "INPUT_PULLUP")
    return INPUT_PULLUP;
  if (val == "INPUT_PULLDOWN")
    return INPUT_PULLDOWN;
  if (val == "OPEN_DRAIN")
    return OPEN_DRAIN;
  if (val == "OUTPUT_OPEN_DRAIN")
    return OUTPUT_OPEN_DRAIN;
  if (val == "ANALOG")
    return ANALOG;
  return -1;
}

bool send400ifnull(String name, String value)
{
  if (value != "")
    return false;
  server.send(405, "text/plain", "Bad Request\n\n" + name + " is empty\n"); // Bad Request \n\n q is empty
  return true;
}

void send400args(String name, String value)
{
  server.send(405, "text/plain", "Bad Request\n\n" + name + "= \"" + value + "\"\n"); // Bad Request \n\n q = "example"
}

void send405()
{
  server.send(405, "text/plain", "Method Not Allowed");
}

void drawGraph()
{
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10)
  {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}
