# esp-chat
A ESP32 project for chat on air‑gapped PCs.

## Hardware

- ESP32 development board with Wi‑Fi capability
- USB serial connection to a host computer

## Libraries

The sketch relies on the following Arduino libraries:

- `WiFi.h` and `WiFiClientSecure.h`
- `HTTPClient.h`
- `ArduinoJson`
- `Preferences`
- `esp_log` from ESP‑IDF

## Memory considerations

When fetching messages, the sketch previously allocated a fixed 8 KB
`DynamicJsonDocument`.  Actual payload sizes depend on the number of
messages and their content.  The code now estimates required capacity
using ArduinoJson's `JSON_ARRAY_SIZE`/`JSON_OBJECT_SIZE` macros with an
average message length of ~200 characters.  This reduces waste while
allowing larger payloads.

For very large responses, consider parsing the HTTP stream directly as
shown in `fetchMessages()` to avoid building a large intermediate
string.

## Serial commands

Type `!help` in the serial console to see available commands:

- `!setname [name]` – set display name
- `!setid [id]` – set user ID
- `!setcolor [hex]` – set avatar color
- `!setssid [ssid]` – set Wi‑Fi SSID (requires `!reboot`)
- `!setpass [pass]` – set Wi‑Fi password (requires `!reboot`)
- `!show` – show saved configuration
- `!reset` – reset all settings to defaults
- `!reboot` – reboot the device
- `!ip` – print the current IP address

## Firmware updates

The device queries a `firmware_updates` table in Supabase to check for newer firmware. Create a table with columns:

| Column | Type | Description |
|-------|------|-------------|
| `version` | text | firmware version string |
| `binary_url` | text | HTTPS URL to the compiled `.bin` file |

Upload the new binary and insert a row with its version and URL. The sketch fetches
`https://<project>.supabase.co/rest/v1/firmware_updates?select=version,binary_url&order=version.desc&limit=1` and if the version differs from `CURRENT_VERSION` it downloads and installs the update.

## License

This project is released under the [MIT](LICENSE) license.
