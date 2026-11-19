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

1. Paste the Discord webhook URL. It is hidden (password field) and remembered between runs.
2. Optional: type a display name (sent as the webhook `username`).
3. Type the message (max 2000 characters, counter shows `n / 2000`).
4. Click **Send** or press **Ctrl+Enter**.

The status line shows **Sent** on success (the message box is cleared), or the HTTP code and error text on failure.

Only URLs starting with `https://discord.com/api/webhooks/` or `https://discordapp.com/api/webhooks/` are accepted.
Mentions are disabled in the request, so `@everyone` and pings do not notify anyone.

Your webhook URL is stored only on your PC (Qt `QSettings`, Windows registry), never in this repository.
Anyone with a webhook URL can post in that channel: keep it secret, and delete the webhook in Discord if it leaks.

## License

MIT (see `LICENSE`). The release zip bundles the Qt 6 libraries, which are under the LGPL v3.
