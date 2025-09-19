#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <WakeOnLan.h>
#include <HTTPClient.h>
#include <ESP32Ping.h> 
#include <UniversalTelegramBot.h> 

#include <discord.h>
#include <interactions.h>
#include <privateconfig.h>

#define LOGIN_INTERVAL 30000 //Cannot be too short to give time to initially retrieve the gateway API

WiFiMulti wifiMulti;
WiFiUDP UDP;
WakeOnLan WOL(UDP);

// ===== DISCORD CONFIG =====
Discord::Bot discord(botToken, applicationId);

bool botEnabled = true;
bool broadcastAddrSet = false;
bool commandsRegistered = false;
unsigned long lastLoginAttempt = 0;

// ===== TELEGRAM CONFIG =====
WiFiClientSecure secured_client;
UniversalTelegramBot telegramBot(telegramToken, secured_client);
bool telegramEnabled = false; 
unsigned long lastTelegramMessageTime = 0;
const unsigned long TELEGRAM_TIMEOUT = 60000;

unsigned long lastCheckTime = 0;
const unsigned long botPollingInterval = 2000;

// ===== COMMON FUNCTIONS =====
bool update_wifi_status() 
{
    if (wifiMulti.run() == WL_CONNECTED) {
        if (broadcastAddrSet) return true;

        IPAddress broadcastAddr = WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
        Serial.print("[WIFI] Broadcast address set to ");
        broadcastAddr.printTo(Serial);
        Serial.println();
        broadcastAddrSet = true;
        Serial.println("[WIFI] Wi-Fi connection established.");
        return true;
    }
    broadcastAddrSet = false;
    return false;
}

String getWANIP() {
    HTTPClient http;
    http.begin("http://api.ipify.org"); 
    int httpCode = http.GET();
    String payload = "Unknown";

    if (httpCode == 200) {
        payload = http.getString();
    } else {
        payload = "Error: " + String(httpCode);
    }
    http.end();
    return payload;
}

String checkStatus(const char* name, const char* ip) {
    static String msg;
    IPAddress target;
    if (!target.fromString(ip)) {
        msg = String("[ERROR] Invalid IP for ") + name;
        return msg;
    }

    if (Ping.ping(target, 1)) {
        msg = String(name) + " is ONLINE";
    } else {
        msg = String(name) + " is OFFLINE";
    }
    return msg;
}

// ===== DISCORD HANDLER =====
void on_discord_interaction(const char* name, const JsonObject& interaction) {
    Serial.println("[DISCORD] Interaction received.");

    if (strcmp(name, "ping") == 0) {
        Discord::Bot::MessageResponse response;
        response.content = String("ESP32 uplink online.");
        discord.sendCommandResponse(
            Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
            response
        );
    }
    else if (strcmp(name, "wake") == 0) {
        Discord::Bot::MessageResponse response;
        uint64_t id;
        if (interaction.containsKey("member")) {
            id = interaction["member"]["user"]["id"];
        } else {
            id = interaction["user"]["id"];
        }

        bool authorised = false;
        for (int i = 0; i < sizeof(botOwnerIds) / sizeof(botOwnerIds[0]); ++i) {
            if (id != botOwnerIds[i]) continue;

            authorised = true;
            response.content = String("Magic packet sent");
            discord.sendCommandResponse(
                Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
                response
            );
            if (WOL.sendMagicPacket(macAddress)) {
                Serial.println("[WOL] Packet sent.");
            } else {
                Serial.println("[WOL] Packet failed to send.");
            }
        }

        if (!authorised) {
            response.content = String("Access denied.");
            response.flags = Discord::Bot::MessageResponse::Flags::EPHEMERAL;
            discord.sendCommandResponse(
                Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
                response
            );
        }
    }
    else if (strcmp(name, "wanip") == 0) {
        Discord::Bot::MessageResponse response;
        String wanIP = getWANIP();  
        response.content = "Current WAN IP: " + wanIP;
        discord.sendCommandResponse(
            Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
            response
        );
    }
    else if (strcmp(name, "pcstatus") == 0) {
        Discord::Bot::MessageResponse response;
        response.content = checkStatus("PC", PCTargetIP);
        discord.sendCommandResponse(
            Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
            response
        );
    }
    // else if (strcmp(name, "psstatus") == 0) {
    //     Discord::Bot::MessageResponse response;
    //     response.content = checkStatus("PS", PSTargetIP);
    //     discord.sendCommandResponse(
    //         Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
    //         response
    //     );
    // }

    vTaskDelay(500);
}

void registerCommands() {
    Serial.println("Registering commands...");
    Discord::Interactions::ApplicationCommand cmd;

    cmd.name = "ping";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Ping the bot for a response.";
    Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);

    cmd.name = "wake";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Send wake signal to PC.";
    Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);

    cmd.name = "wanip";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Get WAN IP address.";
    Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);

    cmd.name = "pcstatus";
    cmd.description = "Check if PC is online.";
    Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);

    // cmd.name = "psstatus";
    // cmd.description = "Check if PS is online.";
    // Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
}

// ===== TELEGRAM HANDLER =====
bool isAuthorized(String chat_id) {
  for (int i = 0; i < sizeof(telegramOwnerIds) / sizeof(telegramOwnerIds[0]); ++i) {
    if (chat_id == String(telegramOwnerIds[i])) return true;
  }
  return false;
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(telegramBot.messages[i].chat_id);
    String text = telegramBot.messages[i].text;
    Serial.printf("[TELEGRAM] Received: %s from %s\n", text.c_str(), chat_id.c_str());

    if (text == "/ping") {
      telegramBot.sendMessage(chat_id, "ESP uplink online.", "");
    } 
    else if (text == "/wanip") {
      telegramBot.sendMessage(chat_id, "WAN IP is: " + getWANIP(), "");
    } 
    else if (text == "/wake") {
      if (isAuthorized(chat_id)) {
        if (WOL.sendMagicPacket(macAddress)) {
          telegramBot.sendMessage(chat_id, "Magic packet sent", "");
        } else {
          telegramBot.sendMessage(chat_id, "Failed to send magic packet.", "");
        }
      } else {
        telegramBot.sendMessage(chat_id, "Access denied.", "");
      }
    }
    else if (text == "/pcstatus") {
      telegramBot.sendMessage(chat_id, checkStatus("PC", PCTargetIP), "");
    }
    // else if (text == "/psstatus") {
    //   telegramBot.sendMessage(chat_id, checkStatus("PS", PSTargetIP), "");
    // }
    // else if (text == "/start") {
    //     telegramEnabled = true;
    //     botEnabled = false;  // tắt Discord
    //     lastTelegramMessageTime = millis(); // reset timeout
    //     telegramBot.sendMessage(chat_id, "✅ Telegram bot is ON.\n⛔ Discord bot is OFF.", "");
    // }
    // else if (text == "/stop") {
    //     telegramEnabled = false;
    //     botEnabled = true;   // bật lại Discord khi Telegram tắt
    //     telegramBot.sendMessage(chat_id, "🛑 Telegram bot is OFF.\n✅ Discord bot is ON.", "");
    // }

    else {
      telegramBot.sendMessage(chat_id, "Unknown command.", "");
    }
  }
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    Serial.println("[STATUS] Standby.");
    Serial.print("[CONFIG] Target MAC address: ");
    Serial.println(macAddress);
    Serial.print("[CONFIG] Default network: ");
    Serial.println(wifiSSID);
    wifiMulti.addAP(wifiSSID, wifiPassword);

    secured_client.setInsecure(); // Disable SSL validation for Telegram

    discord.onInteraction(on_discord_interaction);
}

// ===== LOOP =====
void loop() {
    unsigned long now = millis();

    if (!update_wifi_status()) {
        Serial.println("[WIFI] Not connected.");
        delay(1000);
        return;
    }

    // ===== Discord Handling =====
    if (botEnabled) {
        if (!discord.online() && millis() > lastLoginAttempt) {
            lastLoginAttempt = millis() + LOGIN_INTERVAL;
            discord.login(4096);
        }

        discord.update(millis());

        // ✅ Đăng ký slash command một lần khi bot online
        if (discord.online() && !commandsRegistered) {
            registerCommands();
            commandsRegistered = true;
            Serial.println("[DISCORD] Commands registration attempted.");
        }
    }
    
    // ===== Telegram Handling =====
    if (millis() - lastCheckTime > botPollingInterval) {
        int numNewMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
        if (numNewMessages) {
            handleNewMessages(numNewMessages); // handle /start, /stop...
            lastTelegramMessageTime = millis(); // cập nhật thời điểm nhận tin nhắn mới
        }
        lastCheckTime = millis();
    }

    // Timeout nếu không có tin nhắn trong TELEGRAM_TIMEOUT
    if (telegramEnabled && (millis() - lastTelegramMessageTime > TELEGRAM_TIMEOUT)) {
        telegramEnabled = false;
        botEnabled = true; // bật lại Discord
        Serial.println("[TELEGRAM] Timeout, no new messages, turn OFF Telegram bot, turn ON Discord.");
    }
}
