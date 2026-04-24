-- ============================================================
-- OrcaSlicer Config Database Schema
-- Phase 1: Proof of Concept - Filament Presets
-- ============================================================

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- ============================================================
-- TABLES
-- ============================================================

-- Vendors table
CREATE TABLE vendors (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(100) NOT NULL UNIQUE,
    display_name VARCHAR(200),
    description TEXT,
    is_system BOOLEAN DEFAULT true,
    version VARCHAR(50),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Presets table (single table for all preset types)
CREATE TABLE presets (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    vendor_id UUID REFERENCES vendors(id) ON DELETE SET NULL,

    -- Preset identification
    type VARCHAR(20) NOT NULL CHECK (type IN ('machine', 'process', 'filament')),
    name VARCHAR(200) NOT NULL,
    inherits VARCHAR(200),

    -- The full config as JSONB
    config JSONB NOT NULL DEFAULT '{}',

    -- Preset metadata
    is_system BOOLEAN DEFAULT false,
    is_template BOOLEAN DEFAULT false,
    filament_id VARCHAR(100),
    setting_id VARCHAR(100),
    base_id VARCHAR(100),

    -- Sync tracking
    sync_status VARCHAR(20) DEFAULT '' CHECK (sync_status IN ('', 'create', 'update', 'delete', 'hold', 'save')),
    updated_time BIGINT DEFAULT 0,
    user_id VARCHAR(100),

    -- Timestamps
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),

    CONSTRAINT unique_vendor_type_name UNIQUE (vendor_id, type, name)
);

-- Physical printers
CREATE TABLE physical_printers (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(200) NOT NULL UNIQUE,
    printer_model VARCHAR(200),
    preset_id UUID REFERENCES presets(id) ON DELETE SET NULL,
    config JSONB DEFAULT '{}',
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- User selections
CREATE TABLE user_selections (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id VARCHAR(100) NOT NULL,
    printer_name VARCHAR(200),
    print_preset_id UUID REFERENCES presets(id) ON DELETE SET NULL,
    filament_preset_ids UUID[] DEFAULT '{}',
    config JSONB DEFAULT '{}',
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    CONSTRAINT unique_user_printer UNIQUE (user_id, printer_name)
);

-- Sync state
CREATE TABLE sync_state (
    user_id VARCHAR(100) PRIMARY KEY,
    last_sync_timestamp BIGINT DEFAULT 0,
    sync_cursor TEXT,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Users table
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(100) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================
-- INDEXES
-- ============================================================

CREATE INDEX idx_presets_type ON presets(type);
CREATE INDEX idx_presets_vendor ON presets(vendor_id) WHERE vendor_id IS NOT NULL;
CREATE INDEX idx_presets_name ON presets(name);
CREATE INDEX idx_presets_setting_id ON presets(setting_id) WHERE setting_id IS NOT NULL;
CREATE INDEX idx_presets_filament_id ON presets(filament_id) WHERE filament_id IS NOT NULL;
CREATE INDEX idx_presets_inherits ON presets(inherits) WHERE inherits IS NOT NULL;
CREATE INDEX idx_presets_sync_status ON presets(sync_status) WHERE sync_status != '';
CREATE INDEX idx_user_selections_user ON user_selections(user_id);

-- ============================================================
-- FUNCTIONS
-- ============================================================

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER update_vendors_updated_at BEFORE UPDATE ON vendors FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_presets_updated_at BEFORE UPDATE ON presets FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_physical_printers_updated_at BEFORE UPDATE ON physical_printers FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_user_selections_updated_at BEFORE UPDATE ON user_selections FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
CREATE TRIGGER update_sync_state_updated_at BEFORE UPDATE ON sync_state FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

COMMENT ON TABLE vendors IS 'Vendor organizations (BBL, Orca, etc.)';
COMMENT ON TABLE presets IS 'Configuration presets for machines, processes, and filaments';
COMMENT ON TABLE physical_printers IS 'Physical printer configurations';
COMMENT ON TABLE user_selections IS 'User preset selections per printer';
COMMENT ON TABLE sync_state IS 'Sync state per user for cursor-based pagination';
COMMENT ON TABLE users IS 'User accounts for authentication';
COMMENT ON COLUMN presets.config IS 'Full preset configuration as JSONB';
COMMENT ON COLUMN presets.inherits IS 'Parent preset name for inheritance chain';
COMMENT ON COLUMN presets.is_template IS 'True if instantiation=false (base template, not selectable)';
COMMENT ON COLUMN presets.sync_status IS 'Empty=unchanged, create/update/delete=pending sync, hold=paused';