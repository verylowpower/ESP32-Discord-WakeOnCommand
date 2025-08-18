# ESP32 Discord Wake-On-Command Bot
![Screenshot 2023-08-02 110951](https://github.com/Blocker226/ESP32-Discord-WakeOnCommand/assets/6292676/a28d357d-0267-4ea9-b922-60c7b87f66b8)

Send a Discord command or DM, and wake a target device anywhere, anytime!

## Description
A self-contained Discord Bot that runs on an ESP32 development board and sends a Wake-On-Lan signal to a hardcoded target device from a slash command. Built for ESP32 devices using Arduino C++.

## Features
- Simple ping command to poll responsiveness
- Wake command to send WOL packet to a specific device's MAC address
    - Limited access to specific users
- Based on a expandable ESP32 Discord Bot framework (to be published separately)
    - Built-in command registration

## Dependencies
- [WakeOnLan](https://github.com/a7md0/WakeOnLan) 1.1.7
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 6.21.2
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) 2.4.1

## Installation
1. Download the source code and open it in Visual Studio Code.
2. Use the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) to setup dependencies and build environments, or do it manually.
3. Configure Wifi, Discord Bot token, and your own user IDs in `privateconfig.template`, and rename the file to `privateconfig.h`.
4. Plug in your ESP32 development board to your PC via USB, then build and upload the code.

## Configuration (`privateconfig.h`)

The `include/privateconfig.h` file stores sensitive information required for the bot to function. You must create this file by renaming `include/privateconfig.template` to `include/privateconfig.h` and then fill in your specific details.

Here's the expected format:

```cpp
#ifndef PRIVATE_CONFIG_H
#define PRIVATE_CONFIG_H

// WiFi Credentials
const char* wifiSSID = "YOUR_WIFI_SSID";
const char* wifiPassword = "YOUR_WIFI_PASSWORD";

// Discord Bot Token
// IMPORTANT: Keep this token secure and do not share it publicly.
const char* botToken = "YOUR_DISCORD_BOT_TOKEN";

// MAC Address of the device to wake up (e.g., "00:11:22:33:44:55")
const char* macAddress = "00:11:2A:3E:54:68"

// Discord User IDs of authorized bot owners (for /wake command)
// You can add multiple IDs: {1234567890ULL, 9876543210ULL}
const uint64_t botOwnerIds[] = {YOUR_DISCORD_USER_ID_1ULL};

#endif // PRIVATE_CONFIG_H
```

### How to Obtain Your Discord Bot Token

1.  **Go to the Discord Developer Portal:** Open your web browser and navigate to [https://discord.com/developers/applications](https://discord.com/developers/applications).
2.  **Select Your Application:** Click on the application that corresponds to your Discord bot. If you don't have one, create a "New Application".
3.  **Navigate to the Bot Section:** In the left-hand sidebar, click on "Bot".
4.  **Add a Bot (if necessary):** If you haven't already, click "Add Bot" and confirm.
5.  **Reset and Copy Token:**
    *   Under the "TOKEN" section, click the "Reset Token" button. Confirm the action if prompted.
    *   **Immediately** click the "Copy" button to copy the *newly generated* token. This token is only shown once.
    *   Paste this token into the `botToken` variable in your `privateconfig.h` file.

### How to Obtain Your Discord User ID(s)

To get your Discord User ID (and the IDs of any other users you want to authorize for the `/wake` command):

1.  **Enable Developer Mode in Discord:**
    *   Open your Discord client (desktop app or web).
    *   Go to "User Settings" (the gear icon next to your username).
    *   Navigate to "App Settings" -> "Advanced".
    *   Toggle "Developer Mode" to ON.
2.  **Copy User ID:**
    *   Go to any Discord server or DM.
    *   Right-click on your own username (or the username of another user whose ID you need).
    *   Select "Copy ID".
    *   Paste this ID into the `botOwnerIds` array in your `privateconfig.h` file. Remember to append `ULL` to the end of each ID (e.g., `123456789012345678ULL`).

## Usage

Once the bot is online and connected to Discord, invite it to a server and use the slash command in the server, or DM the bot.

### Commands
- `/ping` - Checks for responsiveness. The bot will reply with "Uplink online."
- `/wake` - Sends a WOL packet to the target MAC address specified in `privateconfig.h`. This only works for the user ids specified in the file, and access will be denied for anyone else attempting to use the command.
- '/wanIP' - Check WanIP

### Troubleshooting
If the LED turns red, it could be for 3 reasons:

1. Unable to connect to Wi-Fi.
2. Unable to connect to Discord.
3. The WOL packet failed to send.

Plug your ESP32 board into a PC, reboot and check serial if needed.

## Contributing

If you've found a reproducible bug or error, or you have a cool feature to suggest, do file an issue! Further contributing guidelines will be made when necessary.
