# NFC Fobs Feature Architecture

## Overview

MIFARE NFC card/fob support for HomeKey-ESP32, allowing users to register and manage NFC fobs as authentication credentials alongside Apple HomeKey.

## Implementation Status

**✅ Fully Implemented** — Core CRUD operations, NVS persistence, Web UI, REST API, MQTT integration, and LockManager authentication integration are all complete.

## Feature Location in UI

- **System Page** → **NFC Fobs** tab (first tab, before HomeKit)
- Two sub-tabs:
  - **Configuration**: Enable/disable toggle + Save & Apply
  - **Fob List**: Add/delete registered fobs by UID

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Web UI (Frontend)                        │
├─────────────────────────────────────────────────────────────────┤
│  AppMisc.svelte                                                 │
│  ├── NFC Fobs Tab Button (first tab in System page)            │
│  └── <NfcFobs> Component                                       │
│       ├── Configuration Tab                                     │
│       │   ├── enabled toggle switch                             │
│       │   └── Save & Apply button → saveNfcFobs()              │
│       └── Fob List Tab                                        │
│           ├── Add Fob Form (UID + Label) → addNfcFob()         │
│           └── Fob Table → deleteNfcFob() per entry             │
└─────────────────────────────────────────────────────────────────┘
                               │
                               │ REST API (HTTP/JSON)
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Backend (ESP32)                             │
├─────────────────────────────────────────────────────────────────┤
│  WebServerManager.cpp                                           │
│  ├── handleGetNfcFobs()     → GET     /nfc_fobs        (200)   │
│  ├── handleSaveNfcFobs()    → POST    /nfc_fobs        (200)   │
│  ├── handleAddNfcFob()      → POST    /nfc_fobs/add    (201)   │
│  └── handleDeleteNfcFob()   → DELETE  /nfc_fobs/delete  (200)  │
│                              Errors: 400, 404, 409, 500         │
│                                                                 │
│  setNfcFobManager(&nfcFobManager)                              │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  NfcFobManager (main/NfcFobManager.cpp)                         │
│  ├── getFobs()          → serialize nfc_fob_config_t to JSON    │
│  ├── saveConfig()       → validate + save to NVS + apply        │
│  ├── addFob(uid, label) → normalize UID + insert + save         │
│  │                        UID normalized to uppercase           │
│  ├── removeFob(uid)     → find + delete + save                 │
│  ├── isFobRegistered()  → check if UID exists (case-insensitive)│
│  ├── serializeToJson()  → full config to JSON string           │
│  ├── deserializeFromJson() → JSON string to config             │
│  ├── saveToNvs()        → store under NVS "SAVED_DATA"/"NFCFOBDATA" │
│  └── loadFromNvs()      → load + merge with defaults           │
│                                                                 │
│  NVS Storage: "SAVED_DATA" namespace / "NFCFOBDATA" key        │
│      Stores JSON: {"enabled": bool, "fobs": [{"uid","label"}]} │
│      No separate "count" field — count derived from array       │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  LockManager Integration                                        │
│  └── setNfcFobManager(&nfcFobManager)                          │
│      └── isFobRegistered() called during authentication flow   │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  MqttManager Integration                                        │
│  └── setNfcFobManager(&nfcFobManager)                          │
│      └── MQTT topics for remote fob management:                │
│          - nfc_fobs/state        (get state)                    │
│          - nfc_fobs/set_state    (enable/disable)               │
│          - nfc_fobs/add          (add fob)                      │
│          - nfc_fobs/delete       (delete fob)                   │
└─────────────────────────────────────────────────────────────────┘
```

## Data Types

### TypeScript (`data/src/lib/types/api.ts`)

```typescript
export interface NfcFobEntry {
  uid: string;    // Uppercase hex UID (e.g., "A1:B2:C3:D4")
  label: string;  // Optional human-readable label
}

export interface NfcFobConfig {
  enabled: boolean;
  fobs: NfcFobEntry[];
}
```

### C++ (`main/include/config.hpp`)

```cpp
namespace espConfig {
  struct nfc_fob_entry_t {
    std::string uid;
    std::string label;
  };

  struct nfc_fob_config_t {
    bool enabled = NFC_FOB_ENABLED;  // default: false
    std::vector<nfc_fob_entry_t> fobs;
  };
}
```

### Defaults (`main/include/defaults.h`)

```cpp
#define NFC_FOB_ENABLED false
#define NFC_FOB_MAX_ENTRIES 32
```

## API Endpoints

### GET /nfc_fobs

Get current NFC fob configuration.

**Response (200 OK):**
```json
{
  "success": true,
  "data": {
    "enabled": true,
    "fobs": [
      { "uid": "A1:B2:C3:D4", "label": "Kitchen Key" },
      { "uid": "E5:F6:07:18", "label": "Garage Door" }
    ]
  },
  "message": "NFC fobs retrieved"
}
```

### POST /nfc_fobs

Save full NFC fob configuration.

**Request Body:**
```json
{
  "enabled": true,
  "fobs": [
    { "uid": "A1:B2:C3:D4", "label": "Kitchen Key" }
  ]
}
```

**Response (200 OK):**
```json
{ "success": true, "message": "NFC fob configuration saved" }
```

### POST /nfc_fobs/add

Add a single fob entry.

**Request Body:**
```json
{ "uid": "A1:B2:C3:D4", "label": "Kitchen Key" }
```

**Response (201 Created):**
```json
{ "success": true, "message": "Fob added successfully" }
```

**Conflict (409):** If fob with same UID already exists.

### DELETE /nfc_fobs/delete

Delete a fob entry by UID.

**Request Body:**
```json
{ "uid": "A1:B2:C3:D4" }
```

**Response (200 OK):**
```json
{ "success": true, "message": "Fob deleted successfully" }
```

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `nfc_fobs/state` | Publish | Current NFC fob configuration |
| `nfc_fobs/set_state` | Subscribe | Enable/disable NFC fob authentication |
| `nfc_fobs/add` | Subscribe | Add a new NFC fob (`{"uid":"A1B2C3D4","label":"My Fob"}`) |
| `nfc_fobs/delete` | Subscribe | Delete an NFC fob (`{"uid":"A1B2C3D4"}`) |

## File Reference

| File | Purpose |
|------|---------|
| [`main/NfcFobManager.cpp`](main/NfcFobManager.cpp) | Core fob CRUD logic + NVS persistence |
| [`main/include/NfcFobManager.hpp`](main/include/NfcFobManager.hpp) | NfcFobManager class declaration |
| [`main/WebServerManager.cpp`](main/WebServerManager.cpp) | HTTP handler implementations |
| [`main/include/WebServerManager.hpp`](main/include/WebServerManager.hpp) | Handler declarations + setNfcFobManager() |
| [`main/main.cpp`](main/main.cpp) | NfcFobManager instantiation + wiring |
| [`main/LockManager.cpp`](main/LockManager.cpp) | NFC fob authentication integration |
| [`main/MqttManager.cpp`](main/MqttManager.cpp) | NFC fob MQTT topic integration |
| [`main/include/config.hpp`](main/include/config.hpp) | nfc_fob_entry_t and nfc_fob_config_t structs |
| [`main/include/defaults.h`](main/include/defaults.h) | NFC_FOB_ENABLED and NFC_FOB_MAX_ENTRIES defaults |
| [`data/src/lib/components/NfcFobs.svelte`](data/src/lib/components/NfcFobs.svelte) | Frontend UI component |
| [`data/src/lib/components/AppMisc.svelte`](data/src/lib/components/AppMisc.svelte) | System page with NFC Fobs tab |
| [`data/src/lib/types/api.ts`](data/src/lib/types/api.ts) | TypeScript type definitions |
| [`data/src/lib/services/api.ts`](data/src/lib/services/api.ts) | API client functions |

## Key Design Decisions

1. **UID Normalization**: All UIDs are normalized to uppercase for case-insensitive matching
2. **NVS Persistence**: Fob config stored as JSON under NVS key `"NFCFOBDATA"` in namespace `"SAVED_DATA"`
3. **Max Entries**: Hardcoded to 32 (`MAX_FOB_ENTRIES`) — `NFC_FOB_MAX_ENTRIES` define exists but is not used as a compile-time constant in the manager
4. **Standalone Manager**: NfcFobManager is a static instance (not unique_ptr) for simple lifetime management
5. **RESTful API**: Standard HTTP methods and status codes for CRUD operations
6. **UI Placement**: NFC Fobs tab is the first tab in the System page (before HomeKit, Hardware, Security)
7. **Thread Safety**: All public methods use `std::mutex` for thread-safe access
8. **LockManager Integration**: `isFobRegistered()` called during authentication flow to validate tapped fobs
9. **MQTT Integration**: Remote fob management via MQTT topics for home automation integration
10. **JSON-based NVS**: Configuration stored as JSON string (not binary) for easier debugging and migration

## Implementation Details

### NVS Storage Format

```json
{
  "enabled": true,
  "fobs": [
    { "uid": "A1B2C3D4", "label": "Kitchen Key" }
  ]
}
```

### Lifecycle

1. `NfcFobManager nfcFobManager;` — static global instance in [`main.cpp`](main/main.cpp)
2. `nfcFobManager.begin()` — loads config from NVS during startup
3. `webServerManager->setNfcFobManager(&nfcFobManager)` — passes pointer to WebServerManager
4. `lockManager->setNfcFobManager(&nfcFobManager)` — passes pointer to LockManager
5. `mqttManager->setNfcFobManager(&nfcFobManager)` — passes pointer to MqttManager

### Error Codes

| Return | Meaning |
|--------|---------|
| `ESP_OK` | Success |
| `ESP_ERR_INVALID_STATE` | Duplicate UID (fob already exists) |
| `ESP_ERR_NO_MEM` | Maximum fob entries reached (32) |
| `ESP_ERR_NOT_FOUND` | Fob UID not found for deletion |
| `ESP_FAIL` | JSON parse failure |

## Future Considerations

- Bulk import/export of fob lists
- Fob activity logging
- Support for additional MIFARE card types beyond UID-based auth
- UID prefix matching for wildcard fob groups
- Rate limiting for NFC fob authentication attempts
