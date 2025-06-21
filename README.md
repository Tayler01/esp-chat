# esp-chat
A esp32 project for chat on airgapped PCs

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
