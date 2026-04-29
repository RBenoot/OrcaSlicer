#!/usr/bin/env python3
"""Seed script to load OrcaSlicer filament presets from JSON files into PostgreSQL."""

import json
import sys
import os
from pathlib import Path
import hashlib
import time

try:
    import psycopg2
except ImportError:
    print("psycopg2 not installed. Run: pip install psycopg2-binary")
    sys.exit(1)

def compute_setting_id(name, config):
    filament_id = config.get('filament_id', '')
    if filament_id:
        return f"GF{filament_id[1:]}" if filament_id.startswith('G') else filament_id
    hash_str = hashlib.md5(name.encode()).hexdigest()[:6].upper()
    return f"GFS{hash_str}"

def compute_updated_time():
    return int(time.time() * 1000)

def get_vendor_id(conn, vendor_name):
    cur = conn.cursor()
    cur.execute("INSERT INTO vendors (name, display_name, is_system) VALUES (%s, %s, true) ON CONFLICT (name) DO NOTHING RETURNING id", (vendor_name, vendor_name))
    result = cur.fetchone()
    if result: return result[0]
    cur.execute("SELECT id FROM vendors WHERE name = %s", (vendor_name,))
    result = cur.fetchone()
    return result[0] if result else None

def load_json_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)

def process_filament_file(filepath, vendor_id):
    config = load_json_file(filepath)
    name = config.get('name', '')
    inherits = config.get('inherits')
    is_template = config.get('instantiation', 'true').lower() == 'false'
    is_system = config.get('from', 'user') == 'system'
    filament_id = config.get('filament_id', '')
    setting_id = config.get('setting_id', '') or compute_setting_id(name, config)

    config_keys_remove = ['type', 'name', 'inherits', 'from', 'instantiation', 'filament_id', 'setting_id']
    clean_config = {k: v for k, v in config.items() if k not in config_keys_remove}

    return {
        'name': name, 'inherits': inherits, 'config': clean_config,
        'is_system': is_system, 'is_template': is_template,
        'filament_id': filament_id, 'setting_id': setting_id, 'base_id': None
    }

def insert_preset(conn, vendor_id, preset):
    cur = conn.cursor()
    cur.execute("""
        INSERT INTO presets (vendor_id, type, name, inherits, config, is_system, is_template, filament_id, setting_id, base_id, updated_time)
        VALUES (%s, 'filament', %s, %s, %s, %s, %s, %s, %s, %s, %s)
        ON CONFLICT (vendor_id, type, name) DO UPDATE SET
            inherits = EXCLUDED.inherits, config = EXCLUDED.config, is_system = EXCLUDED.is_system,
            is_template = EXCLUDED.is_template, filament_id = EXCLUDED.filament_id,
            setting_id = EXCLUDED.setting_id, base_id = EXCLUDED.base_id, updated_time = EXCLUDED.updated_time,
            updated_at = NOW()
    """, (vendor_id, preset['name'], preset['inherits'], json.dumps(preset['config']),
          preset['is_system'], preset['is_template'], preset['filament_id'],
          preset['setting_id'], preset['base_id'], compute_updated_time()))
    print(f"  OK: {preset['name']}")

def main():
    if len(sys.argv) < 3:
        print("Usage: python seed_filaments.py <resources_dir> <db_connection_string>")
        sys.exit(1)

    resources_dir = sys.argv[1]
    db_conn_str = sys.argv[2]

    conn = psycopg2.connect(db_conn_str)
    conn.autocommit = True

    profiles_dir = Path(resources_dir) / 'profiles'
    if not profiles_dir.exists():
        print(f"Profiles directory not found: {profiles_dir}")
        sys.exit(1)

    total = 0
    for vendor_dir in sorted(profiles_dir.iterdir()):
        if not vendor_dir.is_dir() or vendor_dir.name.startswith('.'):
            continue

        vendor_id = get_vendor_id(conn, vendor_dir.name)
        if not vendor_id:
            print(f"\nSkipping unknown vendor: {vendor_dir.name}")
            continue

        filament_dir = vendor_dir / 'filament'
        if not filament_dir.exists():
            continue

        print(f"\n[{vendor_dir.name}]")
        presets = []
        for json_file in filament_dir.glob('**/*.json'):
            try:
                presets.append(process_filament_file(json_file, vendor_id))
            except Exception as e:
                print(f"  ERROR {json_file.name}: {e}")

        # Resolve base_id (parent's setting_id)
        name_to_preset = {p['name']: p for p in presets}
        for p in presets:
            if p['inherits'] and p['inherits'] in name_to_preset:
                p['base_id'] = name_to_preset[p['inherits']]['setting_id']

        for preset in presets:
            try:
                insert_preset(conn, vendor_id, preset)
                total += 1
            except Exception as e:
                print(f"  DB ERROR {preset['name']}: {e}")

    print(f"\n=== Done: {total} presets processed ===")
    conn.close()

if __name__ == '__main__':
    main()