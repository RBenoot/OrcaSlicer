-- ============================================================
-- OrcaSlicer Config Database - Verification Queries
-- Run this to check database status
-- ============================================================

-- 1. Total presets
SELECT COUNT(*) AS total_presets FROM presets;

-- 2. Presets by type
SELECT type, COUNT(*) FROM presets GROUP BY type;

-- 3. Presets by vendor
SELECT
    v.name AS vendor,
    COUNT(p.id) AS preset_count
FROM vendors v
LEFT JOIN presets p ON p.vendor_id = v.id
GROUP BY v.name
ORDER BY COUNT(p.id) DESC;

-- 4. Check inheritance (samples)
SELECT
    name,
    inherits,
    is_system,
    is_template
FROM presets
WHERE inherits IS NOT NULL
LIMIT 10;

-- 5. Find system presets
SELECT name, type FROM presets WHERE is_system = true LIMIT 10;

-- 6. Find templates (instantiation = false)
SELECT name, type FROM presets WHERE is_template = true LIMIT 10;

-- 7. Check a specific preset's full config
SELECT
    name,
    type,
    config
FROM presets
WHERE name = 'Bambu PLA Basic @BBL X1C';

-- 8. Count presets with sync_status (for future sync feature)
SELECT sync_status, COUNT(*) FROM presets GROUP BY sync_status;

-- 9. Check if all vendors have presets
SELECT
    v.name,
    COALESCE(COUNT(p.id), 0) AS count
FROM vendors v
LEFT JOIN presets p ON p.vendor_id = v.id
GROUP BY v.name
HAVING COUNT(p.id) = 0;

-- 10. List all tables
SELECT table_name FROM information_schema.tables WHERE table_schema = 'public';

-- ============================================================
-- Useful for debugging
-- ============================================================

-- Reset/clean all presets (DANGER!)
-- DELETE FROM presets WHERE is_system = false;

-- Check database size
SELECT pg_size_pretty(pg_database_size('orca_config'));

-- List active connections
SELECT * FROM pg_stat_activity WHERE datname = 'orca_config';

-- ============================================================
-- End of verification queries
-- ============================================================