# OrcaSlicer Config Database Migration - Project Documentation

## Overzicht

Dit project migreert OrcaSlicer config opslag van JSON bestanden naar PostgreSQL database als single source of truth voor synchronisatie tussen meerdere OrcaSlicer installaties.

---

## 1. Architectuur

```
┌─────────────────────────────────────────────────────────────────┐
│                         OrcaSlicer                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐               │
│  │ AppConfig   │  │PresetBundle│  │   UI        │               │
│  │ (ini/json)  │  │ (presets)  │  │             │               │
│  └──────┬──────┘  └──────┬──────┘  └─────────────┘               │
│         │                │                                       │
│         ▼                ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │              ConfigManager (NIEUW)                        │     │
│  │  - Loading/Saving presets                                │     │
│  │  - Inheritance resolution                                 │     │
│  │  - Sync orchestration                                     │     │
│  └────────────────────────┬────────────────────────────────┘     │
│                           │                                       │
│                           ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │              ConfigRepository (NIEUW)                   │     │
│  │  - Database queries                                      │     │
│  │  - Serialization/Deserialization                        │     │
│  └────────────────────────┬────────────────────────────────┘     │
└───────────────────────────┼─────────────────────────────────────┘
                            │ HTTP/REST
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    .NET Backend (NIEUW)                          │
│  ┌─────────────────┐  ┌─────────────────┐                      │
│  │   API Endpoints  │  │  PostgreSQL      │                      │
│  │   /api/presets   │  │  orca_db         │                      │
│  │   /api/sync      │  │                  │                      │
│  │   /api/auth      │  │                  │                      │
│  └─────────────────┘  └─────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Beslissingen

| Aspect | Beslissing |
|--------|------------|
| **Storage** | Full config opslag in DB (geen delta) |
| **Conflict resolution** | Server timestamp wint, lokale kopie bewaren voor gebruiker |
| **System presets** | `is_system=true`, readonly via sync |
| **Offline mode** | Lokale cache + queue voor later sync |
| **Sync trigger** | Hybride: automatisch bij opstarten/sluiten + manueel |
| **Authenticatie** | Username/password via .NET backend, JWT tokens |
| **API** | .NET Web API (ASP.NET Core) |

---

## 3. Database Schema

### 3.1 Tabellen

**vendors**
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| id | UUID | Primary key |
| name | VARCHAR(100) | Unieke naam (BBL, Orca, etc.) |
| display_name | VARCHAR(200) | Weergavenaam |
| is_system | BOOLEAN | true = readonly ingebouwde presets |
| created_at, updated_at | TIMESTAMP | |

**presets** (single table voor alle types)
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| id | UUID | Primary key |
| vendor_id | UUID | FK naar vendors |
| type | VARCHAR(20) | 'machine', 'process', 'filament' |
| name | VARCHAR(200) | Preset naam |
| inherits | VARCHAR(200) | Parent preset naam (inheritance) |
| config | JSONB | Volledige preset configuratie |
| is_system | BOOLEAN | System preset (readonly) |
| is_template | BOOLEAN | instantiation=false (niet selecteerbaar) |
| filament_id | VARCHAR(100) | Materiaal ID (e.g., "GFA00") |
| setting_id | VARCHAR(100) | Cloud sync ID |
| base_id | VARCHAR(100) | Parent setting_id voor inheritance |
| sync_status | VARCHAR(20) | '', 'create', 'update', 'delete', 'hold', 'save' |
| updated_time | BIGINT | Unix timestamp in milliseconds |
| user_id | VARCHAR(100) | Eigenaar user ID |
| created_at, updated_at | TIMESTAMP | |

**physical_printers**
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| id | UUID | Primary key |
| name | VARCHAR(200) | Unieke printer naam |
| printer_model | VARCHAR(200) | Model type |
| preset_id | UUID | FK naar presets (machine preset) |
| config | JSONB | Extra printer settings |

**user_selections**
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| id | UUID | Primary key |
| user_id | VARCHAR(100) | |
| printer_name | VARCHAR(200) | |
| print_preset_id | UUID | FK naar presets |
| filament_preset_ids | UUID[] | Array voor multi-material |
| config | JSONB | Extra settings per printer |

**sync_state**
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| user_id | VARCHAR(100) | Primary key |
| last_sync_timestamp | BIGINT | |
| sync_cursor | TEXT | Cursor voor paginatie |
| updated_at | TIMESTAMP | |

**users**
| Kolom | Type | Omschrijving |
|-------|------|--------------|
| id | UUID | Primary key |
| username | VARCHAR(100) | Uniek |
| password_hash | VARCHAR(255) | |
| email | VARCHAR(255) | |
| created_at, updated_at | TIMESTAMP | |

### 3.2 Indexes

```sql
CREATE INDEX idx_presets_type ON presets(type);
CREATE INDEX idx_presets_vendor ON presets(vendor_id) WHERE vendor_id IS NOT NULL;
CREATE INDEX idx_presets_name ON presets(name);
CREATE INDEX idx_presets_setting_id ON presets(setting_id) WHERE setting_id IS NOT NULL;
CREATE INDEX idx_presets_filament_id ON presets(filament_id) WHERE filament_id IS NOT NULL;
CREATE INDEX idx_presets_inherits ON presets(inherits) WHERE inherits IS NOT NULL;
CREATE INDEX idx_presets_sync_status ON presets(sync_status) WHERE sync_status != '';
```

---

## 4. Huidige Status

### Fase 1: PostgreSQL Setup ✓ COMPLEET

**Locatie bestanden:**
- `scripts/sql/01_schema.sql` - Database schema
- `scripts/sql/seed_filaments.py` - Seed script voor filament presets

**Wat is gedaan:**
1. PostgreSQL database `orca_config` draait via Docker
2. Schema aangemaakt met alle tabellen en indexes
3. Vendors toegevoegd: BBL, Orca, Z-Bolt, WonderMaker, Ginger Additive, Co Print, Chuanying, Blocks
4. **1787 filament presets** ingeladen uit JSON bestanden

**Database connectie:**
```
postgresql://postgres:orca_secret@localhost:5432/orca_config
```

**Verificatie queries:**
```sql
-- Totaal presets per vendor
SELECT name, (SELECT name FROM vendors WHERE id = vendor_id), COUNT(*)
FROM presets WHERE vendor_id IS NOT NULL
GROUP BY vendor_id ORDER BY COUNT(*) DESC;

-- Check inheritance
SELECT name, inherits, is_system, is_template
FROM presets WHERE name LIKE '%PLA%' LIMIT 5;

-- Tel alle presets
SELECT COUNT(*) FROM presets;
```

---

## 5. API Endpoints (Te implementeren)

### Authenticatie
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/login` | Login met username/password, retourneer JWT |
| POST | `/api/auth/refresh` | Token vernieuwen |
| GET | `/api/auth/me` | Huidige gebruiker info |

### Presets
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/presets?type=filament` | Alle presets van een type |
| GET | `/api/presets/{id}` | Single preset ophalen |
| POST | `/api/presets` | Nieuwe preset aanmaken |
| PUT | `/api/presets/{id}` | Preset updaten |
| DELETE | `/api/presets/{id}` | Preset verwijderen |

### Sync
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/sync/pull?cursor=X` | Changes ophalen sinds cursor |
| POST | `/api/sync/push` | Lokale changes pushen |
| GET | `/api/sync/state` | Huidige sync status |

---

## 6. Sync Protocol

### Pull (server → client)
```json
GET /api/sync/pull?cursor=abc123

Response:
{
  "next_cursor": "def456",
  "upserts": [
    {
      "id": "uuid-...",
      "name": "Generic PLA",
      "type": "filament",
      "content": { /* full preset config */ },
      "updated_at": "2026-04-24T10:00:00Z",
      "created_at": "..."
    }
  ],
  "deletes": ["setting_id_1", "setting_id_2"]
}
```

### Push (client → server)
```json
POST /api/sync/push
{
  "changes": [
    {
      "id": "local-uuid",
      "setting_id": "cloud-id-or-empty",
      "name": "My Custom PLA",
      "type": "filament",
      "content": { /* full config */ },
      "sync_status": "create/update/delete",
      "updated_at": 1713950400000
    }
  ]
}

Response:
{
  "results": [
    {
      "success": true,
      "setting_id": "cloud-new-id",
      "new_updated_at": "..."
    }
  ]
}
```

---

## 7. Implementatie Phases

### Phase 1: PostgreSQL Setup ✓ COMPLEET
- [x] PostgreSQL installatie (Docker)
- [x] Database schema
- [x] Seed script
- [x] Verificatie

### Phase 2: .NET Web API (NOG TE DOEN)
- [ ] Project structuur aanmaken
- [ ] Entity Framework Core met Npgsql
- [ ] Auth endpoints (JWT)
- [ ] Preset CRUD endpoints
- [ ] Sync pull/push endpoints

### Phase 3: C++ ConfigDatabase class (NOG TE DOEN)
- [ ] ConfigDatabase.hpp interface
- [ ] ConfigDatabase.cpp implementatie
- [ ] HTTP client voor API calls
- [ ] JSON serialization

### Phase 4: PresetBundle integratie (NOG TE DOEN)
- [ ] PresetBundle aanpassen voor DB loading
- [ ] PresetCollection.load_from_db()
- [ ] AppConfig backend URL + auth token opslag

### Phase 5: Sync Logic (NOG TE DOEN)
- [ ] sync_pull() implementatie
- [ ] sync_push() implementatie
- [ ] Conflict resolution
- [ ] Offline mode

---

## 8. Config Originele Opslag (Referentie)

### Bestandsstructuur (huidig)
```
<data_dir>/
├── slic3r.ini                    # AppConfig (JSON)
└── user/
    └── default/
        ├── filament/            # Filament presets (JSON)
        ├── process/             # Print presets (JSON)
        └── machine/             # Printer presets (JSON)

<resources_dir>/
└── profiles/
    └── BBL/
        ├── filament/            # System presets (JSON)
        ├── process/
        └── machine/
```

### Belangrijke Classes
- **AppConfig** (`src/libslic3r/AppConfig.cpp`) - Application-wide settings
- **PresetBundle** (`src/libslic3r/PresetBundle.cpp`) - Alle preset collections
- **Preset** (`src/libslic3r/Preset.cpp`) - Individuele preset
- **PresetCollection** (`src/libslic3r/Preset.cpp`) - Collection van presets
- **DynamicPrintConfig** (`src/libslic3r/PrintConfig.hpp`) - Key-value config opslag

### Sync velden in Preset class
- `setting_id` - Cloud sync ID
- `filament_id` - Material ID
- `user_id` - Owner
- `base_id` - Parent preset cloud ID
- `sync_info` - 'create', 'update', 'delete', 'hold', 'save', ''

---

## 9. Project Bestanden

### Aangemaakt
```
scripts/
└── sql/
    ├── 01_schema.sql           # Database schema
    └── seed_filaments.py      # Seed script
```

### Te Aanmaken
```
scripts/
└── sql/
    └── (bestaand)

src/
├── libslic3r/
│   ├── ConfigDatabase.hpp      # Nieuw: Interface
│   └── ConfigDatabase.cpp      # Nieuw: Implementatie

src/dotnet/
└── OrcaConfigApi/              # Nieuw: .NET Web API project
    ├── Controllers/
    ├── Models/
    ├── Services/
    ├── Data/
    └── Program.cs
```

---

## 10. Running Commands

### Database
```powershell
# Start PostgreSQL container
docker run --name orca-postgres -e POSTGRES_PASSWORD=orca_secret -e POSTGRES_DB=orca_config -p 5432:5432 -d postgres:15

# Stop/Start
docker stop orca-postgres
docker start orca-postgres

# Connect via psql
docker exec -it orca-postgres psql -U postgres -d orca_config
```

### Seed Script
```powershell
# Herstart seed (na wijzigingen)
python scripts/sql/seed_filaments.py "C:\Users\Robin\Documents\OrcaSlicer\resources" "postgresql://postgres:orca_secret@localhost:5432/orca_config"
```

### Verificatie
```sql
-- Totaal presets
SELECT COUNT(*) FROM presets;

-- Per type
SELECT type, COUNT(*) FROM presets GROUP BY type;

-- Per vendor
SELECT v.name, COUNT(p.id)
FROM vendors v LEFT JOIN presets p ON p.vendor_id = v.id
GROUP BY v.name ORDER BY COUNT(p.id) DESC;
```

---

## 11. Issues & Notas

- BIQU vendor wordt overgeslagen omdat deze niet in de vendors tabel staat
- Sommige vendors in profiles/ zijn nog niet toegevoegd (Creality, Prusa, etc.)
- Voor POC fokus op filament presets alleen
- .NET project moet nog opgezet worden
- C++ integratie moet nog ontwikkeld worden

---

## 12. Volgende Stappen

1. **Fase 2 starten**: .NET Web API project aanmaken
2. **Auth implementeren**: JWT login endpoint
3. **Preset CRUD**: GET/POST/PUT/DEL endpoints
4. **Sync endpoints**: pull/push implementeren
5. **Testen**: Vanaf Postman/browser API testen

---

## 13. Lokaal Testen met Alternative Data Directory

Bij lokaal testen is het handig om met een aparte folder te werken waar presets zijn opgeslagen, zodat de ontwikkelomgeving gescheiden blijft van de normale gebruikersdata.

### Windows: Batch bestand aanmaken

Maak een `.bat` bestand aan in de map waar de `.exe` staat (bijv. `build/`):

```batch
start "" "orca-slicer.exe" --datadir "C:\Mijn_Orca_Dev_Data"
```

Dit start OrcaSlicer met een alternatieve data directory voor presets en configuratie.

### Voordelen

- presets en instellingen blijven gescheiden van productie
- veilig om te experimenteren
- geen risico op het overschrijven van wichtige presets

---

*Laatst bijgewerkt: 2026-04-29*