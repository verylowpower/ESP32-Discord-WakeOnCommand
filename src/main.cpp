#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>  
#include "privateconfig.h"

// -------- Wi-Fi & WOL --------
WiFiMulti wifiMulti;
WiFiUDP UDP;
WakeOnLan WOL(UDP);

// -------- Telegram Bot --------
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);

unsigned long lastCheckTime = 0;
const unsigned long botPollingInterval = 2000;

bool broadcastAddrSet = false;

// -------- Functions --------
bool update_wifi_status() {
  if (wifiMulti.run() == WL_CONNECTED) {
    if (!broadcastAddrSet) {
      IPAddress broadcastAddr = WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
      Serial.print("[WIFI] Broadcast address set to ");
      broadcastAddr.printTo(Serial);
      Serial.println();
      broadcastAddrSet = true;
      Serial.println("[WIFI] Wi-Fi connected.");
    }
    return true;
  }
  broadcastAddrSet = false;
  return false;
}

String getWANIP() {
  HTTPClient http;
  http.begin("http://api.ipify.org");
  int httpCode = http.GET();
  String payload = "";
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
  }
  http.end();
  return payload;
}

bool isAuthorized(String chat_id) {
  for (int i = 0; i < sizeof(botOwnerIds) / sizeof(botOwnerIds[0]); ++i) {
    if (chat_id == botOwnerIds[i]) return true;
  }
  return false;
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    Serial.printf("[TELEGRAM] Received: %s from %s\n", text.c_str(), chat_id.c_str());

    if (text == "/ping") {
      bot.sendMessage(chat_id, "ESP32 uplink online.", "");
    } 
    else if (text == "/wanip") {
      String wan = getWANIP();
      bot.sendMessage(chat_id, "Current WAN IP: " + wan, "");
    } 
    else if (text == "/wake") {
      if (isAuthorized(chat_id)) {
        IPAddress broadcastAddr = WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
        if (WOL.sendMagicPacket(macAddress, broadcastAddr)) {
          bot.sendMessage(chat_id, "Magic packet sent", "");
        } else {
          bot.sendMessage(chat_id, "Failed to send magic packet.", "");
        }
      } else {
        bot.sendMessage(chat_id, "Access denied.", "");
      }
    }
    else if (text == "/pc_status") {
      if (Ping.ping(PCTargetIP, 2)) {   
        bot.sendMessage(chat_id, "PC is ON ", "");
      } else {
        bot.sendMessage(chat_id, "PC is OFF ", "");
      }
    }
    else if (text == "/ps_status") {
      if (Ping.ping(PSTargetIP, 2)) {   
        bot.sendMessage(chat_id, "PS is ON ", "");
      } else {
        bot.sendMessage(chat_id, "PS is OFF ", "");
      }
    }
    else {
      bot.sendMessage(chat_id, "Unknown command.", "");
    }
  }
}

// -------- Setup & Loop --------
void setup() {
  Serial.begin(115200);
  Serial.println("[STATUS] ESP32 Booting...");

  wifiMulti.addAP(wifiSSID, wifiPassword);

  secured_client.setInsecure(); 

  Serial.print("[CONFIG] Target MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", macAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println("\n[CONFIG] Telegram Bot initialized.");
}

void loop() {
  if (!update_wifi_status()) {
    Serial.println("[WIFI] Waiting for Wi-Fi...");
    delay(5000);
    return;
  }

  unsigned long now = millis();
  if (now - lastCheckTime > botPollingInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = now;
  }
}
