#include "PresetBundleDB.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"
#include <boost/log/trivial.hpp>

namespace Slic3r {

std::shared_ptr<PresetBundleDB> PresetBundleDB::instance()
{
    static std::shared_ptr<PresetBundleDB> inst(new PresetBundleDB());
    return inst;
}

std::string PresetBundleDB::preset_type_to_string(Preset::Type type)
{
    switch (type) {
        case Preset::Type::TYPE_PRINT: return "print";
        case Preset::Type::TYPE_FILAMENT: return "filament";
        case Preset::Type::TYPE_PRINTER: return "printer";
        case Preset::Type::TYPE_SLA_PRINT: return "sla_print";
        case Preset::Type::TYPE_SLA_MATERIAL: return "sla_material";
        default: return "unknown";
    }
}

Preset::Type PresetBundleDB::string_to_preset_type(const std::string& type)
{
    if (type == "print") return Preset::Type::TYPE_PRINT;
    if (type == "filament") return Preset::Type::TYPE_FILAMENT;
    if (type == "printer") return Preset::Type::TYPE_PRINTER;
    if (type == "sla_print") return Preset::Type::TYPE_SLA_PRINT;
    if (type == "sla_material") return Preset::Type::TYPE_SLA_MATERIAL;
    return Preset::Type::TYPE_INVALID;
}

void PresetBundleDB::apply_preset_record(Preset& preset, const PresetRecord& record)
{
    preset.name = record.name;
    preset.is_system = record.is_system;
    
    if (!record.setting_id.empty()) {
        preset.setting_id = record.setting_id;
    }
    if (!record.filament_id.empty()) {
        preset.config.set("filament_id", new ConfigOptionString(record.filament_id));
    }
    
    if (!record.inherits.empty()) {
        auto* opt_inherits = preset.config.option<ConfigOptionString>("inherits", true);
        if (opt_inherits) {
            opt_inherits->value = record.inherits;
        }
    }
    
    if (!record.config.is_null()) {
        for (auto it = record.config.begin(); it != record.config.end(); ++it) {
            const std::string& key = it.key();
            if (key == "filament_id" || key == "inherits" || key == "name" || 
                key == "type" || key == "setting_id" || key == "base_id") {
                continue;
            }
            
            const auto& value = it.value();
            if (value.is_number_integer()) {
                preset.config.set(key, new ConfigOptionInt(static_cast<int>(value.get<int64_t>())));
            } else if (value.is_number_float()) {
                preset.config.set(key, new ConfigOptionFloat(value.get<double>()));
            } else if (value.is_string()) {
                preset.config.set(key, new ConfigOptionString(value.get<std::string>()));
            } else if (value.is_boolean()) {
                preset.config.set(key, new ConfigOptionBool(value.get<bool>()));
            }
        }
    }
}

PresetRecord PresetBundleDB::preset_to_record(const Preset& preset, const std::string& type)
{
    PresetRecord record;
    record.id = preset.setting_id;
    record.type = type;
    record.name = preset.name;
    record.is_system = preset.is_system;
    record.setting_id = preset.setting_id;
    record.updated_time = 0;
    
    if (!preset.inherits().empty()) {
        record.inherits = preset.inherits();
    }
    
    json config;
    for (const auto& key : preset.config.keys()) {
        auto* opt = preset.config.option(key);
        if (!opt) continue;
        
        if (auto* s = dynamic_cast<const ConfigOptionString*>(opt)) {
            if (!s->value.empty()) config[key] = s->value;
        } else if (auto* i = dynamic_cast<const ConfigOptionInt*>(opt)) {
            config[key] = i->value;
        } else if (auto* f = dynamic_cast<const ConfigOptionFloat*>(opt)) {
            config[key] = f->value;
        } else if (auto* b = dynamic_cast<const ConfigOptionBool*>(opt)) {
            config[key] = b->value;
        }
    }
    record.config = config;
    
    return record;
}

void PresetBundleDB::load_presets_from_db(
    PresetBundle* bundle,
    const std::string& type,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    m_db->fetch_presets(type, [this, bundle, type, callback](bool success, 
        const std::vector<PresetRecord>& presets, const std::string& error) {
        
        if (!success) {
            callback(false, error);
            return;
        }
        
        PresetCollection* collection = nullptr;
        if (type == "filament") collection = &bundle->filaments;
        else if (type == "print") collection = &bundle->prints;
        else if (type == "printer") collection = &bundle->printers;
        else if (type == "sla_print") collection = &bundle->sla_prints;
        else if (type == "sla_material") collection = &bundle->sla_materials;
        
        if (!collection) {
            callback(false, "Unknown preset type: " + type);
            return;
        }
        
        for (const auto& record : presets) {
            Preset new_preset(Preset::TYPE_INVALID, record.name, record.is_system);
            new_preset.setting_id = record.setting_id;
            
            apply_preset_record(new_preset, record);
            
            if (record.inherits.empty()) {
                collection->load_preset("", record.name, std::move(new_preset.config), false);
            }
        }
        
        BOOST_LOG_TRIVIAL(info) << "Loaded " << presets.size() << " " << type << " presets from database";
        callback(true, "");
    });
}

void PresetBundleDB::save_preset_to_db(
    PresetBundle* bundle,
    Preset& preset,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    std::string type = preset_type_to_string(preset.type);
    PresetRecord record = preset_to_record(preset, type);
    
    if (preset.setting_id.empty()) {
        m_db->create_preset(record, [&preset, callback](bool success, const PresetRecord* result, const std::string& error) {
            if (success && result) {
                preset.setting_id = result->setting_id;
            }
            callback(success, error);
        });
    } else {
        m_db->update_preset(preset.setting_id, record, [callback](bool success, const PresetRecord* result, const std::string& error) {
            callback(success, error);
        });
    }
}

void PresetBundleDB::delete_preset_from_db(
    const std::string& preset_id,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    m_db->delete_preset(preset_id, callback);
}

void PresetBundleDB::sync_from_db(
    PresetBundle* bundle,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    auto cursor_ptr = std::make_shared<std::string>("");
    auto first_batch_ptr = std::make_shared<bool>(true);
    auto fetch_next_batch = std::make_shared<std::function<void()>>();
    
    *fetch_next_batch = [this, bundle, callback, cursor_ptr, first_batch_ptr, fetch_next_batch]() {
        m_db->sync_pull(*cursor_ptr, 100, [this, bundle, callback, cursor_ptr, first_batch_ptr, fetch_next_batch](
            bool success, const SyncPullResult& result, const std::string& error) {
            
            if (!success) {
                callback(false, error);
                return;
            }
            
            for (const auto& record : result.upserts) {
                PresetCollection* collection = nullptr;
                if (record.type == "filament") collection = &bundle->filaments;
                else if (record.type == "print") collection = &bundle->prints;
                else if (record.type == "printer") collection = &bundle->printers;
                else if (record.type == "sla_print") collection = &bundle->sla_prints;
                else if (record.type == "sla_material") collection = &bundle->sla_materials;
                
                if (collection) {
                    Preset new_preset(Preset::TYPE_INVALID, record.name, record.is_system);
                    new_preset.setting_id = record.setting_id;
                    apply_preset_record(new_preset, record);
                    collection->load_preset("", record.name, std::move(new_preset.config), false);
                }
            }
            
            *cursor_ptr = result.next_cursor;
            if (!cursor_ptr->empty()) {
                (*fetch_next_batch)();
            } else {
                BOOST_LOG_TRIVIAL(info) << "Sync from DB completed";
                callback(true, "");
            }
        });
    };
    
    (*fetch_next_batch)();
}

void PresetBundleDB::sync_to_db(
    PresetBundle* bundle,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    std::vector<SyncPushChange> changes;
    
    for (const auto& preset : bundle->filaments) {
        SyncPushChange change;
        change.id = preset.setting_id;
        change.setting_id = preset.setting_id;
        change.name = preset.name;
        change.type = "filament";
        change.content = json::object();
        change.sync_status = "save";
        change.updated_at = 0;
        changes.push_back(change);
    }
    
    if (changes.empty()) {
        callback(true, "");
        return;
    }
    
    m_db->sync_push(changes, [changes, callback](bool success, const json& result, const std::string& error) {
        if (success) {
            BOOST_LOG_TRIVIAL(info) << "Synced " << changes.size() << " presets to database";
        }
        callback(success, error);
    });
}

void PresetBundleDB::login_and_sync(
    const std::string& username,
    const std::string& password,
    PresetBundle* bundle,
    std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    m_db->login(username, password, [this, bundle, callback](bool success, const std::string& error) {
        if (!success) {
            callback(false, "Login failed: " + error);
            return;
        }
        
        sync_from_db(bundle, callback);
    });
}

} // namespace Slic3r
