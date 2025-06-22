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

SQL migrations for the Supabase setup live in `supabase/migrations`.  They create
the `firmware_updates` table and grant anonymous read access.  To configure a new
project manually, follow these steps:

1. **Create a Storage bucket** named `firmware` and enable public access.  Upload
   compiled `.bin` files here.
2. **Create the tracking table** by running the migration or executing:

   ```sql
   create table firmware_updates (
     id bigserial primary key,
     version text not null,
     binary_url text not null,
     created_at timestamp with time zone default now()
   );
   alter table firmware_updates enable row level security;
   create policy "Allow public read" on firmware_updates for select using (true);
   ```
3. **Insert an initial firmware row** pointing to your uploaded binary:

   ```sql
   insert into firmware_updates (version, binary_url)
   values ('1.1.0',
     'https://<project>.supabase.co/storage/v1/object/public/firmware/chatbox-v1.1.0.bin');
   ```

Devices fetch
`https://<project>.supabase.co/rest/v1/firmware_updates?select=version,binary_url&order=version.desc&limit=1`
and apply an update when the returned version differs from `CURRENT_VERSION`.

## License

This project is released under the [MIT](LICENSE) license.
