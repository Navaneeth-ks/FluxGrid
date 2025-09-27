#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SS   5
#define RST  26
#define DIO0 33

const char* ssid = "Node2";
const char* masterNode = "MASTER";
const String GPS_COORDS = "10.727944,76.289472";

WebServer server(80);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

#define BUTTON_PIN 18
unsigned long messageStartTime = 0;
const unsigned long messageDuration = 3000;
bool showingSOS = false;

struct Message {
  String senderNode;
  String msgID;
  String type;
  String text;
  String gps;
  bool received;
};

#define MAX_MESSAGES 50
Message messages[MAX_MESSAGES];
int msgCount = 0;

void addMessage(String senderNode, String msgID, String type, String text, String gps = "", bool received = false) {
  if (msgCount < MAX_MESSAGES) {
    messages[msgCount++] = {senderNode, msgID, type, text, gps, received};
  } else {
    for (int i = 1; i < MAX_MESSAGES; i++) messages[i - 1] = messages[i];
    messages[MAX_MESSAGES - 1] = {senderNode, msgID, type, text, gps, received};
  }
}

void markReceived(String msgID) {
  for (int i = 0; i < msgCount; i++) {
    if (messages[i].msgID == msgID && messages[i].senderNode == "Me") {
      messages[i].received = true;
      break;
    }
  }
}

void showMessage(const char* msg) {
  display1.clearDisplay();
  display1.setTextSize(1);
  display1.setTextColor(SSD1306_WHITE);
  display1.setCursor(0, 20);
  display1.println(msg);
  display1.display();

  display2.clearDisplay();
  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);
  display2.setCursor(0, 20);
  display2.println(msg);
  display2.display();
}

void clearDisplays() {
  display1.clearDisplay();
  display1.display();
  display2.clearDisplay();
  display2.display();
}

void handleRoot() {
  String html = R"rawliteral(
  <!doctype html><html><head><meta charset="utf-8">
  <title>ESP32 Pod Dashboard</title>
  <style>
    body { font-family: "Segoe UI", sans-serif; background: #e6f2ff; padding: 20px; }
    #output { padding: 10px; border: 1px solid #003366; background: #fff; height: 300px; overflow-y: auto; white-space: pre-wrap; }
  </style></head><body>
    <h2>ESP32 Pod Node</h2>
    <form action='/send'>Message: <input name='msg' type='text'>
    <input type='submit' value='Send'></form>
    <hr><h3>Messages:</h3>
    <div id="output">Waiting for data...</div>
    <script>
      async function fetchData() {
        const res = await fetch("/data"); const text = await res.text();
        document.getElementById("output").innerText = text;
        const outDiv=document.getElementById("output"); outDiv.scrollTop=outDiv.scrollHeight;
      }
      setInterval(fetchData,2000); fetchData();
    </script></body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleSend() {
  String msg = server.arg("msg");
  if (msg != "") {
    String msgID = String(millis());
    String packet = String(ssid) + "|" + msgID + "|" + masterNode + "|MSG|" + msg + "|GPS:" + GPS_COORDS;
    LoRa.beginPacket(); LoRa.print(packet); LoRa.endPacket();
    addMessage("Me", msgID, "MSG", msg, GPS_COORDS, false);
    Serial.println("Sent: " + packet);
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleData() {
  String out = "";
  for (int i = 0; i < msgCount; i++) {
    out += messages[i].senderNode + ": " + messages[i].text;
    if (messages[i].gps != "") out += " [GPS: " + messages[i].gps + "]";
    if (messages[i].senderNode == "Me" && messages[i].received) out += " ✅ Received";
    out += "\n";
  }
  server.send(200, "text/plain", out);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(21, 22);

  if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for (;;);
  if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) for (;;);
  clearDisplays();
  showMessage("press button for SOS");

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!"); while (1);
  }
  Serial.println("LoRa ready");

  WiFi.softAP(ssid);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.on("/data", handleData);
  server.begin();
  Serial.println("WebServer started");
}

void loop() {
  server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW && !showingSOS) {
    String msgID = String(millis());
    String packet = String(ssid) + "|" + msgID + "|" + masterNode + "|SOS|Emergency!|GPS:" + GPS_COORDS;
    for (int i = 0; i < 3; i++) {
      LoRa.beginPacket();
      LoRa.print(packet);
      LoRa.endPacket();
      delay(200);
    }
    addMessage("Me", msgID, "SOS", "Emergency!", GPS_COORDS, false);
    Serial.println("SOS Sent: " + packet);
    showMessage("SOS signal sent");
    messageStartTime = millis();
    showingSOS = true;
    delay(300);
  }

  if (showingSOS && millis() - messageStartTime >= messageDuration) {
    clearDisplays();
    showMessage("press button for SOS");
    showingSOS = false;
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    Serial.println("LoRa RX: " + incoming);

    int firstSep  = incoming.indexOf('|');
    int secondSep = incoming.indexOf('|', firstSep + 1);
    int thirdSep  = incoming.indexOf('|', secondSep + 1);
    int fourthSep = incoming.indexOf('|', thirdSep + 1);

    if (firstSep > 0 && secondSep > firstSep && thirdSep > secondSep && fourthSep > thirdSep) {
      String senderNode = incoming.substring(0, firstSep);
      String msgID      = incoming.substring(firstSep + 1, secondSep);
      String targetNode = incoming.substring(secondSep + 1, thirdSep);
      String type       = incoming.substring(thirdSep + 1, fourthSep);
      String text       = incoming.substring(fourthSep + 1);

      if (type == "ACK") {
        markReceived(msgID);
      } else if (targetNode == ssid && (type == "MSG" || type == "SOS")) {
        addMessage(senderNode, msgID, type, text);
      }
    }
  }
}
