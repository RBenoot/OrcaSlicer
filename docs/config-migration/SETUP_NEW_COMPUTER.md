# Setup Guide - Nieuwe Computer

Volg deze stappen om op een nieuwe computer met het project te kunnen werken.

---

## 1. Clone de Repository

```powershell
cd C:\Users\<JE_USERNAME>\Documents
git clone https://github.com/<je-fork>/OrcaSlicer.git
cd OrcaSlicer
```

---

## 2. Docker Desktop Starten

Zorg dat Docker Desktop draait:
```powershell
# Check of Docker draait
docker ps

# Start de container opnieuw
docker start orca-postgres

# Als de container niet bestaat, maak hem aan:
docker run --name orca-postgres -e POSTGRES_PASSWORD=orca_secret -e POSTGRES_DB=orca_config -p 5432:5432 -d postgres:15
```

---

## 3. Database Initialiseren (Twee Opties)

### Optie A: Automatisch (Aanbevolen)

```powershell
# Voer het setup script uit - dit doet alles in 1 keer
python scripts/sql/setup_database.py
```

Dit script:
- Start/maakt PostgreSQL container
- Laadt database schema
- Voegt vendors toe
- Seed filament presets (1787 stuks)

---

### Optie B: Manueel

Volg de stappen 3.1 tm 3.5 hieronder als je de stappen individueel wilt uitvoeren.

#### 3.1 Database Schema Laden

```powershell
# Kopieer SQL naar container
docker cp scripts/sql/01_schema.sql orca-postgres:/01_schema.sql

# Voer schema uit
docker exec orca-postgres psql -U postgres -d orca_config -f /01_schema.sql
```

#### 3.2 Vendors Toevoegen

```powershell
# Start psql interactief
docker exec -it orca-postgres psql -U postgres -d orca_config

# Voer in psql uit:
INSERT INTO vendors (name, display_name, is_system) VALUES
('BBL', 'Bambu Lab', true),
('Orca', 'OrcaSlicer', true),
('Z-Bolt', 'Z-Bolt', true),
('WonderMaker', 'WonderMaker', true),
('Ginger Additive', 'Ginger Additive', true),
('Co Print', 'Co Print', true),
('Chuanying', 'Chuanying', true),
('Blocks', 'Blocks', true),
('Polymaker', 'Polymaker', true),
('SUNLU', 'SUNLU', true),
('eSUN', 'eSUN', true),
('Numakers', 'Numakers', true),
('Overture', 'Overture', true);
```

Type `\q` om psql te sluiten.

#### 3.3 Seed Script Uitvoeren

```powershell
# Installeer psycopg2 als nog niet geïnstalleerd
pip install psycopg2-binary

# Voer seed script uit
python scripts/sql/seed_filaments.py "C:\Users\<JE_USERNAME>\Documents\OrcaSlicer\resources" "postgresql://postgres:orca_secret@localhost:5432/orca_config"
```

---

## 4. Verificatie

```powershell
docker exec orca-postgres psql -U postgres -d orca_config -c "SELECT COUNT(*) FROM presets;"
```

Verwacht: `1787`

---

## 5. .NET SDK Installeren (voor Fase 2)

Download van https://dotnet.microsoft.com/download (**.NET 8 SDK**)

---

## 6. Alle Volumes Samengevat

**Benodigde software:**
- Docker Desktop
- Python 3.10+
- .NET 8 SDK
- Git

**Environment variables (optioneel):**
```powershell
# Om niet telkens connection string in te typen
$env:ORCA_DB_CONN="postgresql://postgres:orca_secret@localhost:5432/orca_config"
```

---

## Troubleshooting

### Docker container niet startbaar
```powershell
# Verwijder en maak opnieuw
docker rm orca-postgres
docker run --name orca-postgres -e POSTGRES_PASSWORD=orca_secret -e POSTGRES_DB=orca_config -p 5432:5432 -d postgres:15
```

### Port 5432 al in gebruik
```powershell
# Check wat port 5432 gebruikt
netstat -ano | findstr :5432

# Stop andere PostgreSQL diensten of gebruik andere port
```

### Seed script faalt
```powershell
# Check of DB bereikbaar is
docker exec orca-postgres psql -U postgres -d orca_config -c "SELECT 1"

# Check Python versie
python --version
```

---

*Laatst bijgewerkt: 2026-04-24*