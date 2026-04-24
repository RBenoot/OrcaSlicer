# TODO / Roadmap - OrcaSlicer Config Database Migration

## Fase 1: PostgreSQL Setup ✓ COMPLEET
- [x] PostgreSQL installatie (Docker)
- [x] Database schema (`scripts/sql/01_schema.sql`)
- [x] Vendors seeden
- [x] 1787 filament presets ingeladen

## Fase 2: .NET Web API (IN UITVOERING)
- [x] Project aanmaken (`src/dotnet/OrcaConfigApi/`)
- [x] Entity Framework Core setup
- [x] Auth endpoints
  - [x] POST /api/auth/login
  - [x] POST /api/auth/refresh
  - [x] GET /api/auth/me
- [x] Preset CRUD endpoints
  - [x] GET /api/presets?type=filament
  - [x] GET /api/presets/{id}
  - [x] POST /api/presets
  - [x] PUT /api/presets/{id}
  - [x] DELETE /api/presets/{id}
- [x] Sync endpoints
  - [x] GET /api/sync/pull?cursor=X
  - [x] POST /api/sync/push
  - [x] GET /api/sync/state

## Fase 3: C++ ConfigDatabase (NOG TE DOEN)
- [ ] ConfigDatabase.hpp - Interface
- [ ] ConfigDatabase.cpp - HTTP/REST implementatie
- [ ] JSON serialization/deserialization
- [ ] Connection pooling

## Fase 4: PresetBundle Integratie (NOG TE DOEN)
- [ ] PresetBundle::load_from_db()
- [ ] PresetCollection::load_from_db()
- [ ] PresetBundle::save_to_db()
- [ ] Inheritance resolution van DB

## Fase 5: Sync Logic (NOG TE DOEN)
- [ ] sync_pull() implementatie
- [ ] sync_push() implementatie
- [ ] Conflict resolution (server wins)
- [ ] Offline mode / queue

## Fase 6: UI Integratie (NOG TE DOEN)
- [ ] Sync knop in UI
- [ ] Sync status indicator
- [ ] Conflict resolution dialog

## Fase 7: Testing & Polish (NOG TE DOEN)
- [ ] Unit tests
- [ ] Integration tests
- [ ] Import tool (JSON -> DB)
- [ ] Export tool (DB -> JSON)

---

## Bugs / Issues

- BIQU vendor wordt overgeslagen (niet in vendors tabel)
- Sommige profiles vendors niet geïmporteerd (Creality, Prusa, etc.)
- Voor POC enkel filament presets

---

## Dependencies

### C++ Side
- boost (filesystem, process, log)
- nlohmann/json
- Http.hpp (bestaand)

### .NET Side
- .NET 8 SDK
- Npgsql (PostgreSQL driver)
- Entity Framework Core
- BCrypt (password hashing)
- System.IdentityModel.Tokens.Jwt (JWT)

---

## Testing Checklist

### Database
```sql
-- Verifieer data
SELECT COUNT(*) FROM presets;
SELECT COUNT(*) FROM vendors;
SELECT type, COUNT(*) FROM presets GROUP BY type;
```

### API (Postman/Browser)
```http
POST http://localhost:5000/api/auth/login
Content-Type: application/json
{"username":"admin","password":"admin"}

GET http://localhost:5000/api/presets?type=filament
Authorization: Bearer <token>
```

---

*Laatst bijgewerkt: 2026-04-24*

---

## Project Files (Phase 2)

### Aangemaakt
```
src/dotnet/
├── OrcaSlicer.sln
└── OrcaConfigApi/
    ├── OrcaConfigApi.csproj
    ├── Program.cs
    ├── appsettings.json
    ├── Configuration/
    │   └── JwtSettings.cs
    ├── Controllers/
    │   ├── AuthController.cs
    │   ├── PresetsController.cs
    │   └── SyncController.cs
    ├── Data/
    │   └── OrcaDbContext.cs
    ├── DTOs/
    │   ├── AuthDTOs.cs
    │   ├── PresetDTOs.cs
    │   └── SyncDTOs.cs
    ├── Models/
    │   ├── Preset.cs
    │   ├── User.cs
    │   ├── Vendor.cs
    │   ├── PhysicalPrinter.cs
    │   ├── UserSelection.cs
    │   └── SyncState.cs
    ├── Services/
    │   ├── AuthService.cs
    │   ├── PresetService.cs
    │   └── SyncService.cs
    └── Properties/
        └── launchSettings.json
```

### Starten van de API
```bash
cd src/dotnet/OrcaConfigApi
dotnet run
```

### Default credentials
- Username: `admin`
- Password: `admin`