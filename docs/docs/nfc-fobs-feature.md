# NFC Fobs Feature Architecture

## Overview

MIFARE NFC card/fob support for HomeKey-ESP32, allowing users to register and manage NFC fobs as authentication credentials alongside Apple HomeKey.

## Feature Location in UI

- **System Page** → **NFC Fobs** tab (positioned before HomeKit tab)
- Two sub-tabs:
  - **Configuration**: Enable/disable toggle + Save & Apply
  - **Fob List**: Add/delete registered fobs by UID

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Web UI (Frontend)                        │
├─────────────────────────────────────────────────────────────────┤
│  AppMisc.svelte                                                 │
│  ├── NFC Fobs Tab Button                                       │
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
│  ├── handleGetNfcFobs()     → GET     /nfc_fobs    (200)       │
│  ├── handleSaveNfcFobs()    → POST    /nfc_fobs/save  (200)    │
│  ├── handleAddNfcFob()      → POST    /nfc_fobs/add    (201)   │
│  └── handleDeleteNfcFob()   → DELETE  /nfc_fobs/delete (200)   │
│                              Errors: 400, 404, 409, 500         │
│  │                                                             │
│  └── setNfcFobManager(&nfcFobManager)                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  NfcFobManager (main/NfcFobManager.cpp)                         │
│  ├── getFobs()          → serialize nfc_fob_config_t to JSON    │
│  ├── saveConfig()       → validate + save to NVS + apply        │
│  ├── addFob(uid, label) → normalize UID + insert + save         │
│  └── removeFob(uid)     → find + delete + save                  │
│  │                                                             │
│  └── NVS Storage: "nfc_fobs" namespace                          │
│      - "enabled" : uint8_t (0/1)                               │
│      - "count"   : uint8_t                                     │
│      - "fobs"    : json array of {uid, label}                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  ConfigManager (main/ConfigManager.cpp)                         │
│  └── NVS get/put for nfc_fob_config_t                          │
│      Keys: NFC_FOB_ENABLED, NFC_FOB_MAX_ENTRIES                 │
│      Default: enabled=false, max=32                             │
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
    bool enabled;
    std::vector<nfc_fob_entry_t> fobs;
    
    nfc_fob_config_t() : enabled(false) {}
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
  "enabled": true,
  "fobs": [
    { "uid": "A1:B2:C3:D4", "label": "Kitchen Key" },
    { "uid": "E5:F6:07:18", "label": "Garage Door" }
  ]
}
```

### POST /nfc_fobs/save

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

## File Reference

| File | Purpose |
|------|---------|
| [`main/NfcFobManager.cpp`](main/NfcFobManager.cpp) | Core fob CRUD logic + NVS persistence |
| [`main/include/NfcFobManager.hpp`](main/include/NfcFobManager.hpp) | NfcFobManager class declaration |
| [`main/WebServerManager.cpp`](main/WebServerManager.cpp) | HTTP handler implementations |
| [`main/include/WebServerManager.hpp`](main/include/WebServerManager.hpp) | Handler declarations + setNfcFobManager() |
| [`main/main.cpp`](main/main.cpp) | NfcFobManager instantiation + wiring |
| [`main/CMakeLists.txt`](main/CMakeLists.txt) | NfcFobManager.cpp in SRCS list |
| [`main/include/config.hpp`](main/include/config.hpp) | nfc_fob_entry_t and nfc_fob_config_t structs |
| [`main/include/defaults.h`](main/include/defaults.h) | NFC_FOB_ENABLED and NFC_FOB_MAX_ENTRIES defaults |
| [`data/src/lib/components/NfcFobs.svelte`](data/src/lib/components/NfcFobs.svelte) | Frontend UI component |
| [`data/src/lib/components/AppMisc.svelte`](data/src/lib/components/AppMisc.svelte) | System page with NFC Fobs tab |
| [`data/src/lib/types/api.ts`](data/src/lib/types/api.ts) | TypeScript type definitions |
| [`data/src/lib/services/api.ts`](data/src/lib/services/api.ts) | API client functions |

## Key Design Decisions

1. **UID Normalization**: All UIDs are normalized to uppercase for case-insensitive matching
2. **NVS Persistence**: Fob config stored in NVS under "nfc_fobs" namespace
3. **Max Entries**: Configurable via `NFC_FOB_MAX_ENTRIES` (default 32)
4. **Standalone Manager**: NfcFobManager is a static instance (not unique_ptr) for simple lifetime management
5. **RESTful API**: Standard HTTP methods and status codes for CRUD operations
6. **UI Placement**: NFC Fobs tab positioned before HomeKit in the system page tabs

## Future Considerations

- Integration with authentication flow (checking tapped fob UID against registered list)
- Bulk import/export of fob lists
- Fob activity logging
- Support for additional MIFARE card types beyond UID-based auth
