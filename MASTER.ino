#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>

#define SS    5
#define RST   26
#define DIO0  33

const char* ssid = "ESP32_Master";
WebServer server(80);

struct Message {
  String senderNode;
  String msgID;
  String type;
  String text;
  String gps;
};

#define MAX_MESSAGES 100
Message messages[MAX_MESSAGES];
int msgCount = 0;

void addMessage(String senderNode, String msgID, String type, String text, String gps = "") {
  if (msgCount < MAX_MESSAGES) {
    messages[msgCount++] = {senderNode, msgID, type, text, gps};
  } else {
    for (int i = 1; i < MAX_MESSAGES; i++) messages[i - 1] = messages[i];
    messages[MAX_MESSAGES - 1] = {senderNode, msgID, type, text, gps};
  }
}

String jsonEscape(const String &s) {
  String out = "";
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (c == '\\') out += "\\\\";
    else if (c == '\"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

void handleMessagesJSON() {
  String json = "[";
  for (int i = 0; i < msgCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"sender\":\"" + jsonEscape(messages[i].senderNode) + "\",";
    json += "\"msgID\":\""  + jsonEscape(messages[i].msgID) + "\",";
    json += "\"type\":\""   + jsonEscape(messages[i].type) + "\",";
    json += "\"text\":\""   + jsonEscape(messages[i].text) + "\",";
    json += "\"gps\":\""    + jsonEscape(messages[i].gps) + "\"";
    json += "}";
  }
  json += "]";
  server.sendHeader("Access-Control-Allow-Origin", "*");  
  server.send(200, "application/json", json);
}

void handleSend() {
  String targetNode = server.arg("target");
  String msg = server.arg("msg");
  String gps = server.arg("gps");

  if (targetNode != "" && msg != "") {
    String msgID = String(millis());
    String packet = "MASTER|" + msgID + "|" + targetNode + "|MSG|" + msg;
    if (gps != "") packet += "|GPS:" + gps;

    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();

    addMessage("Me", msgID, "MSG", msg, gps);
    Serial.println("Sent: " + packet);
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa ready");

  WiFi.softAP(ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/messages", handleMessagesJSON);
  server.on("/send", handleSend);
  server.begin();
  Serial.println("WebServer started");
}

void loop() {
  server.handleClient();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    Serial.println("LoRa RX: " + incoming);

    int firstSep  = incoming.indexOf('|');
    int secondSep = incoming.indexOf('|', firstSep + 1);
    int thirdSep  = incoming.indexOf('|', secondSep + 1);
    int fourthSep = incoming.indexOf('|', thirdSep + 1);
    int fifthSep  = incoming.indexOf('|', fourthSep + 1);

    if (firstSep > 0 && secondSep > firstSep && thirdSep > secondSep && fourthSep > thirdSep) {
      String senderNode = incoming.substring(0, firstSep);
      String msgID      = incoming.substring(firstSep + 1, secondSep);
      String targetNode = incoming.substring(secondSep + 1, thirdSep);
      String type       = incoming.substring(thirdSep + 1, fourthSep);
      String text       = (fifthSep > 0) ? incoming.substring(fourthSep + 1, fifthSep) : incoming.substring(fourthSep + 1);
      String gps        = (fifthSep > 0) ? incoming.substring(fifthSep + 1) : "";

      if (gps.startsWith("GPS:")) gps = gps.substring(4);

      addMessage(senderNode, msgID, type, text, gps);

      if (type == "MSG") {
        String reply = "MASTER|" + msgID + "|" + senderNode + "|ACK|Received";
        LoRa.beginPacket();
        LoRa.print(reply);
        LoRa.endPacket();
      }
    }
  }
}
