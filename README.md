# Webhook Sender

Small Qt 6 Widgets app that posts a message to a Discord webhook.

## Download

Get `WebhookSender-Windows.zip` from the [latest release](../../releases/latest), unzip it and run `WebhookSender.exe`. No install needed.

## Build

Requirements (the paths in `build.bat`; change them if your Qt is elsewhere):
- Qt 6.12.0 MinGW kit: `C:/Qt/6.12.0/mingw_64`
- MinGW: `C:/Qt/Tools/mingw1310_64/bin`
- CMake: `C:/Qt/Tools/CMake_64/bin/cmake.exe`
- Ninja: `C:/Qt/Tools/Ninja/ninja.exe`

Run:

    build.bat

This configures with Ninja, builds, and runs `windeployqt` (including the `tls` plugin).
Output: `build\WebhookSender.exe`. Double-click it to run.

## Use

1. Paste the Discord webhook URL (hidden; **Show** reveals it). The last one used is remembered.
2. **Saved**: press **Save** to keep the URL, display name and avatar under a name, then pick it from the list next time. **Delete** removes it from your PC (the webhook stays in Discord).
3. Optional **Display name** and **Avatar URL** (a link to a png/jpg/gif, with a preview) for this message.
4. **Set webhook name + picture from file...** changes the webhook's own default name and picture in Discord.
5. Type the message (Markdown works, counter `n / 2000`).
6. **Attach files...** adds up to 10 files.
7. **Embed**: tick it for a card with title, text, color and image.
8. Click **Send** or press **Ctrl+Enter**.

The status line shows **Sent** on success (message and files are cleared), or the HTTP code and error text on failure.

Only Discord webhook URLs (`https://discord.com/api/webhooks/...`, also discordapp/canary/ptb) are accepted.
Mentions are disabled in the request, so `@everyone` and pings do not notify anyone.

Your webhook URLs (last used and saved ones) are stored only on your PC (Qt `QSettings`, Windows registry), never in this repository.
Anyone with a webhook URL can post in that channel: keep it secret, and delete the webhook in Discord if it leaks.

## License

MIT (see `LICENSE`). The release zip bundles the Qt 6 libraries, which are under the LGPL v3.
