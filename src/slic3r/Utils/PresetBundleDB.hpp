#ifndef slic3r_PresetBundleDB_hpp_
#define slic3r_PresetBundleDB_hpp_

#include "libslic3r/PresetBundle.hpp"
#include "ConfigDatabase.hpp"
#include <memory>

namespace Slic3r {

class PresetBundleDB
{
public:
    static std::shared_ptr<PresetBundleDB> instance();
    
    void set_config_database(std::shared_ptr<ConfigDatabase> db) { m_db = db; }
    std::shared_ptr<ConfigDatabase> config_database() const { return m_db; }
    
    void load_presets_from_db(PresetBundle* bundle, const std::string& type,
                              std::function<void(bool success, const std::string& error)> callback);
    
    void save_preset_to_db(PresetBundle* bundle, Preset& preset,
                          std::function<void(bool success, const std::string& error)> callback);
    
    void delete_preset_from_db(const std::string& preset_id,
                               std::function<void(bool success, const std::string& error)> callback);
    
    void sync_from_db(PresetBundle* bundle,
                     std::function<void(bool success, const std::string& error)> callback);
    
    void sync_to_db(PresetBundle* bundle,
                   std::function<void(bool success, const std::string& error)> callback);
    
    void login_and_sync(const std::string& username, const std::string& password,
                       PresetBundle* bundle,
                       std::function<void(bool success, const std::string& error)> callback);

private:
    PresetBundleDB() = default;
    
    std::shared_ptr<ConfigDatabase> m_db;
    
    void apply_preset_record(Preset& preset, const PresetRecord& record);
    PresetRecord preset_to_record(const Preset& preset, const std::string& type);
    
    std::string preset_type_to_string(Preset::Type type);
    Preset::Type string_to_preset_type(const std::string& type);
};

} // namespace Slic3r

#endif // slic3r_PresetBundleDB_hpp_
