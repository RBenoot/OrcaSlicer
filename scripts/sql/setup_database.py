#!/usr/bin/env python3
"""
Quick setup script voor nieuwe computer.
Voert alle stappen uit om de database te initialiseren.

Usage: python scripts/sql/setup_database.py
"""

import subprocess
import sys

def run_command(cmd, description):
    print(f"\n{'='*60}")
    print(f"{description}")
    print(f"{'='*60}")
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    print(result.stdout)
    if result.stderr:
        print(result.stderr)
    return result.returncode == 0

def main():
    print("OrcaSlicer Config Database Setup")
    print("================================")

    # 1. Check Docker
    print("\n[1/6] Checking Docker...")
    if not run_command("docker ps", "Docker status"):
        print("ERROR: Docker is not running. Please start Docker Desktop.")
        sys.exit(1)

    # 2. Start/create container
    print("\n[2/6] Starting PostgreSQL container...")
    run_command("docker start orca-postgres 2>nul || echo 'Container not found, creating...'",
                 "Start or create container")

    # 3. Check/create database
    print("\n[3/6] Checking database...")
    result = subprocess.run(
        "docker exec orca-postgres psql -U postgres -c \"SELECT 1\" -d orca_config",
        shell=True, capture_output=True, text=True
    )
    if result.returncode != 0:
        print("Database 'orca_config' not found. Creating...")
        run_command(
            "docker exec -e POSTGRES_DB=orca_config orca-postgres psql -U postgres -c \"CREATE DATABASE orca_config\"",
            "Create database"
        )

    # 4. Load schema
    print("\n[4/6] Loading database schema...")
    run_command("docker cp scripts/sql/01_schema.sql orca-postgres:/01_schema.sql", "Copy schema")
    run_command("docker exec orca-postgres psql -U postgres -d orca_config -f /01_schema.sql", "Apply schema")

    # 5. Add vendors
    print("\n[5/6] Adding vendors...")
    vendor_sql = """
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
    ('Overture', 'Overture', true)
    ON CONFLICT (name) DO NOTHING;
    """
    run_command(f'docker exec orca-postgres psql -U postgres -d orca_config -c "{vendor_sql}"', "Add vendors")

    # 6. Run seed script
    print("\n[6/6] Seeding presets...")
    import os
    resources_dir = os.path.dirname(os.path.abspath(__file__))
    while resources_dir and not os.path.exists(os.path.join(resources_dir, 'resources', 'profiles')):
        parent = os.path.dirname(resources_dir)
        if parent == resources_dir:
            resources_dir = r"C:\Users\Robin\Documents\OrcaSlicer\resources"
            break
        resources_dir = parent

    conn_str = "postgresql://postgres:orca_secret@localhost:5432/orca_config"
    run_command(f'python scripts/sql/seed_filaments.py "{resources_dir}" "{conn_str}"', "Seed presets")

    # Verify
    print("\n" + "="*60)
    print("VERIFICATION")
    print("="*60)
    subprocess.run('docker exec orca-postgres psql -U postgres -d orca_config -c "SELECT COUNT(*) AS total_presets FROM presets;"')

    print("\n Setup complete!")
    print("\nNext steps:")
    print("1. Start Docker Desktop if not running")
    print("2. Run: docker start orca-postgres")
    print("3. Continue with .NET API setup (see docs/config-migration/README.md)")

if __name__ == '__main__':
    main()