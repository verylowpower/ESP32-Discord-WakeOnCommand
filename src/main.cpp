/*
 * ESP32-Discord-WakeOnCommand v0.1
 * Copyright (C) 2023  Neo Ting Wei Terrence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <HTTPClient.h>


#include <discord.h>
#include <interactions.h>
#include <privateconfig.h>


#define LOGIN_INTERVAL 30000 //Cannot be too short to give time to initially retrieve the gateway API

// This sets Arduino Stack Size - comment this line to use default 8K stack size
//SET_LOOP_TASK_STACK_SIZE(16 * 1024); // 16KB

WiFiMulti wifiMulti;
WiFiUDP UDP;
WakeOnLan WOL(UDP);

Discord::Bot discord(botToken, applicationId);

bool botEnabled = true;
bool broadcastAddrSet = false;
bool commandsRegistered = false; // New: Flag to track if commands have been registered
unsigned long lastLoginAttempt = 0;

bool update_wifi_status() {
    if (wifiMulti.run() == WL_CONNECTED) {
        if (broadcastAddrSet) return true;

        // Attention: 255.255.255.255 is denied in some networks
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

    if (httpCode == 200) { // OK
        payload = http.getString();
    } else {
        payload = "Error: " + String(httpCode);
    }
    http.end();
    return payload;
}

void on_discord_interaction(const char* name, const JsonObject& interaction) {
    Serial.println("[DISCORD] Interaction received.");

    if (strcmp(name, "ping") == 0) {
        Discord::Bot::MessageResponse response;

#ifdef _DISCORD_CLIENT_DEBUG
        String msg("Uplink online. Uptime: ");
        msg += millis();
        msg += "ms, Stack remaining: ";
        msg += uxTaskGetStackHighWaterMark(NULL);
        msg += "b";
        response.content = msg.c_str();
#else
        response.content = "Uplink online.";
#endif
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
        }
        else {
            id = interaction["user"]["id"];
        }

        bool authorised = false;
        for (int i = 0; i < sizeof(botOwnerIds) / sizeof(botOwnerIds[0]); ++i) {
            if (id != botOwnerIds[i]) continue;

            authorised = true;
            response.content = "Command acknowledged. Initiating remote wake sequence.";
            discord.sendCommandResponse(
                Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
                response
            );
            if (WOL.sendMagicPacket(macAddress)) {
                Serial.println("[WOL] Packet sent.");
            }
            else {
                Serial.println("[WOL] Packet failed to send.");
            }
        }

        if (!authorised) {
            response.content = "Access denied.";
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
        String msg = "Current WAN IP: " + wanIP;
        response.content = msg.c_str();
        discord.sendCommandResponse(
            Discord::Bot::InteractionResponse::CHANNEL_MESSAGE_WITH_SOURCE,
            response
        );
    }

    vTaskDelay(500);
}


void registerCommands() {
    Serial.println("Registering commands...");
    Discord::Interactions::ApplicationCommand cmd;
    // 1. /ping
    cmd.name = "ping";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Ping the bot for a response.";
    cmd.default_member_permissions = 2147483648; //Use Application Commands

    uint64_t id = Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
    if (id == 0) {
        Serial.println("Command registration failed!");
    }
    else {
        Serial.print("Registered ping command to id ");
        Serial.println(id);
    }

    //2. /wake
    cmd.name = "wake";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Send a wake signal to the main terminal. Authorized users only.";
    cmd.default_member_permissions = 2147483648;

    id = Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
    if (id == 0) {
        Serial.println("Command registration failed!");
    }
    else {
        Serial.print("Registered wake command to id ");
        Serial.println(id);
    }

    //3. /36IP
    cmd.name = "wanip";
    cmd.type = Discord::Interactions::CommandType::CHAT_INPUT;
    cmd.description = "Get the current WAN IP of the ESP32.";
    cmd.default_member_permissions = 2147483648;

    id = Discord::Interactions::registerGlobalCommand(discord.applicationId(), cmd, botToken);
    if (id == 0) {
        Serial.println("Command registration failed!");
    } else {
        Serial.print("Registered wanip command to id ");
        Serial.println(id);
    }

}

// PROGRAM BEGIN

// put your setup code here, to run once:
void setup() {
    // Clear the serial port buffer and set the serial port baud rate to 115200.
    Serial.begin(115200);
    Serial.println("[STATUS] Standby.");
    Serial.print("[CONFIG] Target MAC address set to ");
    Serial.println(macAddress);
    Serial.print("[CONFIG] Default network set to ");
    Serial.println(wifiSSID);
    wifiMulti.addAP(wifiSSID, wifiPassword);

    discord.onInteraction(on_discord_interaction);
}

#ifdef _DISCORD_CLIENT_DEBUG
long lastStackValue = 0;
long lastStackCheck = 0;
long lastHeapValue = 0;
#endif

void loop() {
    // put your main code here, to run repeatedly:
    unsigned long now = millis();

    //Current record:
    //Loop() - Free Stack Space: 3132
#ifdef _DISCORD_CLIENT_DEBUG
    if (now - lastStackCheck >= 2000) {
        // Print unused stack for the task that is running loop()
        long currentStack = uxTaskGetStackHighWaterMark(NULL);
        if (currentStack != lastStackValue) {
            Serial.print("[STACK CHANGE] Loop() - Free Stack Space: ");
            Serial.print(currentStack);
            Serial.print(" (");
            Serial.print(currentStack - lastStackValue);
            Serial.println(")");
            lastStackValue = currentStack;
        }
        long currentFree = esp_get_free_heap_size();
        if (currentFree != lastHeapValue) {
            Serial.print("[HEAP CHANGE] - Free Heap Space: ");
            Serial.print(currentFree);
            Serial.print(" (");
            Serial.print(currentFree - lastHeapValue);
            Serial.println(")");
            lastHeapValue = currentFree;
        }
        lastStackCheck = now;
    }
#endif

    if (!update_wifi_status()) {
        Serial.println("[WIFI] Wi-Fi connection not established.");
        vTaskDelay(10000);
        return;
    }

    if (botEnabled) {
        if (!discord.online()) {
            Serial.println("[STATUS] Connecting to Discord.");
            if (now > lastLoginAttempt) {
                lastLoginAttempt = now + LOGIN_INTERVAL;
                discord.login(4096); // DIRECT_MESSAGES
            }
        }
        else {
            if (!commandsRegistered) {
                registerCommands();
                commandsRegistered = true;
            }
            //Serial.println("[STATUS] Idle.");
        }
        discord.update(now);
    }
    else {
        Serial.println("[STATUS] Offline Idle.");
    }
}
