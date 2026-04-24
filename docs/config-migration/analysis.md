# OrcaSlicer Config Storage Analyse

## Originele Implementatie

### Bestandsstructuur

**System Presets (readonly, gebundeld met app)**
```
<resources_dir>/profiles/
├── BBL/
│   ├── filament/           # ~300+ JSON bestanden
│   ├── process/            # ~200+ JSON bestanden
│   └── machine/            # ~10+ JSON bestanden
├── Orca/
├── Prusa/
└── ...
```

**User Presets (writable)**
```
<data_dir>/
├── slic3r.ini              # AppConfig (JSON formaat)
└── user/
    └── default/
        ├── filament/       # User filament presets
        ├── process/        # User print presets
        └── machine/        # User printer presets
```

### Belangrijke Classes

#### AppConfig (src/libslic3r/AppConfig.cpp)
- Applicatie-brede settings (niet preset-specifiek)
- Slaat op: `slic3r.ini` (JSON formaat)
- Bevat: vendor selections, recente bestanden, UI preferences
- Key methodes: `load()`, `save()`

#### PresetBundle (src/libslic3r/PresetBundle.cpp)
- Container voor alle preset collections
- Properties:
  - `prints` - Print preset collection
  - `filaments` - Filament preset collection
  - `printers` - Printer preset collection
  - `vendors` - Vendor map
- Key methodes:
  - `load_presets()` - Laadt system presets
  - `load_user_presets()` - Laadt user presets
  - `load_selections()` - Restoreert geselecteerde presets

#### Preset (src/libslic3r/Preset.hpp)
- Individuele preset instantie
- Properties:
  - `type` - TYPE_PRINT, TYPE_FILAMENT, TYPE_PRINTER
  - `name` - Preset naam
  - `file` - Bestand pad
  - `config` - DynamicPrintConfig met key-value pairs
  - `inherits` - Parent preset naam
  - `setting_id` - Cloud sync ID
  - `filament_id` - Material ID
  - `base_id` - Parent cloud ID
  - `sync_info` - Sync status

#### PresetCollection (src/libslic3r/Preset.hpp)
- Manageert collection van presets van zelfde type
- Key methodes:
  - `load_presets()` - Load vanuit directory
  - `save_user_preset()` - Sla wijzigingen op
  - `find_preset()` - Zoek preset bij naam
  - `get_selected_preset()` - Huidige selectie

---

## Config Opslag Formaat

### JSON Preset Voorbeeld (`Bambu PLA Basic @BBL X1C.json`)
```json
{
  "type": "filament",
  "name": "Bambu PLA Basic @BBL X1C",
  "inherits": "Bambu PLA Basic @base",
  "from": "system",
  "setting_id": "GFSA00",
  "instantiation": "true",
  "filament_max_volumetric_speed": ["21", "21"],
  "nozzle_temperature": ["220", "220"],
  "compatible_printers": [
    "Bambu Lab X1 Carbon 0.4 nozzle",
    "Bambu Lab X1 Carbon 0.6 nozzle"
  ]
}
```

### Inheritance Mechanisme
1. Preset kan `inherits` veld hebben dat verwijst naar parent
2. Bij laden: child config = parent config + child diff (delta storage)
3. Bij saven: alleen verschillen opslaan
4. `inherits` kan verwijzen naar preset in andere vendor

---

## Sync Infrastructure (Bestaand)

OrcaSlicer heeft reeds sync-gerelateerde velden:

```cpp
// Preset.hpp (sync velden)
std::string setting_id;      // Cloud database ID
std::string filament_id;      // Material ID
std::string user_id;         // Eigenaar
std::string base_id;         // Parent cloud ID
std::string sync_info;       // 'create', 'update', 'delete', 'hold', 'save', ''

// PresetCollection methodes
void set_sync_info_and_save(std::string name, std::string setting_id, std::string syncinfo, long long update_time);
bool need_sync(std::string name, std::string setting_id, long long update_time);
std::vector<Preset> get_user_presets();
```

### Cloud Service Agent
- `OrcaCloudServiceAgent` (src/slic3r/Utils/OrcaCloudServiceAgent.hpp)
- Implementeert `ICloudServiceAgent` interface
- HTTP/REST communicatie met backend
- JWT token authenticatie
- Sync methodes: `sync_pull()`, `sync_push()`

---

## Verwijderde/Systeem Presets

- `is_system=true` - Readonly systeem presets
- `is_template=true` - `instantiation=false` - Base templates (niet selecteerbaar)

---

## Belangrijke Constants

```cpp
// Preset.hpp
#define PRESET_SYSTEM_DIR    "system"
#define PRESET_USER_DIR      "user"
#define PRESET_FILAMENT_NAME "filament"
#define PRESET_PRINT_NAME    "process"
#define PRESET_PRINTER_NAME  "machine"

// JSON keys
#define BBL_JSON_KEY_VERSION        "version"
#define BBL_JSON_KEY_NAME           "name"
#define BBL_JSON_KEY_TYPE           "type"
#define BBL_JSON_KEY_FROM           "from"
#define BBL_JSON_KEY_INHERITS       "inherits"
#define BBL_JSON_KEY_INSTANTIATION  "instantiation"
#define BBL_JSON_KEY_FILAMENT_ID    "filament_id"
#define BBL_JSON_KEY_SETTING_ID     "setting_id"
```

---

## Data Types (ConfigOption)

| Type | Enum | PostgreSQL |
|------|------|------------|
| coFloat | 1 | REAL |
| coFloats | 0x4001 | REAL[] |
| coInt | 2 | INTEGER |
| coInts | 0x4002 | INTEGER[] |
| coString | 3 | TEXT |
| coStrings | 0x4003 | TEXT[] |
| coBool | 8 | BOOLEAN |
| coBools | 0x4008 | BOOLEAN[] |
| coPercent | 4 | REAL |
| coPoint | 6 | POINT |
| coPoint3 | 7 | TEXT (serialized) |
| coEnum | 9 | INTEGER of TEXT |

---

## Aanpassingen voor Migratie

1. **ConfigDatabase class** - Nieuwe klasse om met DB te praten
2. **PresetBundle** - Aanpasbaar voor DB loading i.p.v. file loading
3. **Sync trigger** - Combinatie van:
   - Bij opstarten (sync_pull)
   - Bij sluiten (sync_push)
   - Manueel (knop in UI)

4. **Conflict resolution** - Server timestamp wint
   - Als server nieuwer is: update lokaal
   - Als lokaal nieuwer is: push naar server
   - Als conflict: bewaar beide versies

5. **Offline mode**
   - Laatste sync bewaren in `sync_state` tabel
   - Wijzigingen queue voor later sync
   - Fallback naar lokale bestanden als DB unreachable

---

## Key Files te Wijzigen

1. `src/libslic3r/PresetBundle.cpp` - Load/save van DB
2. `src/libslic3r/Preset.cpp` - Sync info velden gebruiken
3. `src/libslic3r/AppConfig.cpp` - Backend URL en auth token opslaan
4. `src/libslic3r/Config.cpp` - JSON parsing

## Key Files te Aanmaken

1. `src/libslic3r/ConfigDatabase.hpp` - Interface
2. `src/libslic3r/ConfigDatabase.cpp` - Implementatie
3. `src/dotnet/OrcaConfigApi/` - .NET Web API project

---

*Laatst bijgewerkt: 2026-04-24*