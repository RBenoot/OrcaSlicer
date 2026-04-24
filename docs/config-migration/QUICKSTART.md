# Quick Start - OrcaSlicer Config Migration Project

## Wat is dit project?

Migratie van OrcaSlicer preset opslag van JSON bestanden naar PostgreSQL database voor synchronisatie tussen meerdere installaties.

---

## Huidige Status

**Fase 1 (PostgreSQL Setup) is COMPLEET**

- 1787 filament presets geladen
- Database: `orca_config`
- 13 vendors actief

---

## Direct Aan de Slag

### 1. Database Draaiend Houden

```powershell
# Check of container draait
docker ps

# Start indien gestopt
docker start orca-postgres

# Connecteer
docker exec -it orca-postgres psql -U postgres -d orca_config
```

### 2. Documentatie Bekijken

```
docs/
├── config-migration/
│   ├── README.md      # Hoofd documentatie (START HIER)
│   ├── analysis.md    # Technische analyse
│   └── TODO.md        # Roadmap en checklist
```

### 3. Volgende Stap: .NET API

Start fase 2 door .NET project aan te maken:

```powershell
cd src/dotnet
dotnet new webapi -n OrcaConfigApi
cd OrcaConfigApi
dotnet add package Npgsql.EntityFrameworkCore.PostgreSQL
dotnet add package BCrypt.Net-Next
dotnet add package Microsoft.AspNetCore.Authentication.JwtBearer
```

---

## Database Config

```
Host: localhost
Port: 5432
Database: orca_config
Username: postgres
Password: orca_secret
Connection String: postgresql://postgres:orca_secret@localhost:5432/orca_config
```

---

## Bestanden Overzicht

### Aangemaakt
```
scripts/sql/
├── 01_schema.sql          # Database schema
└── seed_filaments.py      # Seed script (1787 presets)

docs/config-migration/
├── README.md              # Hoofd documentatie
├── analysis.md             # Technische analyse
└── TODO.md                # Roadmap
```

### Te Aanmaken
```
src/dotnet/OrcaConfigApi/   # .NET Web API project
src/libslic3r/ConfigDatabase.hpp  # C++ DB interface
src/libslic3r/ConfigDatabase.cpp  # C++ DB implementatie
```

---

## Contact / Vragen

Bij vragen of problemen:
1. Bekijk `docs/config-migration/README.md`
2. Check `docs/config-migration/TODO.md` voor volgende stappen
3. Database verification: `docker exec orca-postgres psql -U postgres -d orca_config -c "SELECT COUNT(*) FROM presets;"`

---

*Project gestart: 2026-04-24*