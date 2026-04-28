#ifndef slic3r_ConfigDatabase_hpp_
#define slic3r_ConfigDatabase_hpp_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace Slic3r {

using json = nlohmann::json;

struct PresetRecord {
    std::string id;
    std::string vendor_id;
    std::string vendor_name;
    std::string type;
    std::string name;
    std::string inherits;
    json config;
    bool is_system;
    bool is_template;
    std::string filament_id;
    std::string setting_id;
    std::string base_id;
    std::string sync_status;
    int64_t updated_time;
    std::string user_id;
    int64_t created_at;
    int64_t updated_at;
};

struct SyncPullResult {
    std::string next_cursor;
    std::vector<PresetRecord> upserts;
    std::vector<std::string> deletes;
};

struct SyncPushChange {
    std::string id;
    std::string setting_id;
    std::string name;
    std::string type;
    json content;
    std::string sync_status;
    int64_t updated_at;
};

struct SyncState {
    std::string user_id;
    int64_t last_sync_timestamp;
    std::string sync_cursor;
};

struct AuthTokens {
    std::string token;
    std::string refresh_token;
    int64_t expires_at;
};

class ConfigDatabase {
public:
    static std::shared_ptr<ConfigDatabase> instance();

    virtual ~ConfigDatabase() = default;

    virtual void set_base_url(const std::string& url) = 0;
    virtual void set_auth_tokens(const AuthTokens& tokens) = 0;
    virtual void clear_auth_tokens() = 0;
    virtual bool is_authenticated() const = 0;

    virtual void login(const std::string& username, const std::string& password,
                       std::function<void(bool success, const std::string& error)> callback) = 0;
    
    virtual void fetch_presets(const std::string& type,
                                std::function<void(bool success, const std::vector<PresetRecord>& presets, const std::string& error)> callback) = 0;
    
    virtual void fetch_preset(const std::string& id,
                              std::function<void(bool success, const PresetRecord* preset, const std::string& error)> callback) = 0;
    
    virtual void create_preset(const PresetRecord& preset,
                               std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback) = 0;
    
    virtual void update_preset(const std::string& id, const PresetRecord& preset,
                              std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback) = 0;
    
    virtual void delete_preset(const std::string& id,
                              std::function<void(bool success, const std::string& error)> callback) = 0;

    virtual void sync_pull(const std::string& cursor, int limit,
                          std::function<void(bool success, const SyncPullResult& result, const std::string& error)> callback) = 0;
    
    virtual void sync_push(const std::vector<SyncPushChange>& changes,
                         std::function<void(bool success, const json& result, const std::string& error)> callback) = 0;
    
    virtual void sync_state(std::function<void(bool success, const SyncState& state, const std::string& error)> callback) = 0;
};

class ConfigDatabaseRestClient : public ConfigDatabase {
public:
    ConfigDatabaseRestClient();
    ~ConfigDatabaseRestClient() override;

    void set_base_url(const std::string& url) override;
    void set_auth_tokens(const AuthTokens& tokens) override;
    void clear_auth_tokens() override;
    bool is_authenticated() const override;

    void login(const std::string& username, const std::string& password,
               std::function<void(bool success, const std::string& error)> callback) override;
    
    void fetch_presets(const std::string& type,
                       std::function<void(bool success, const std::vector<PresetRecord>& presets, const std::string& error)> callback) override;
    
    void fetch_preset(const std::string& id,
                      std::function<void(bool success, const PresetRecord* preset, const std::string& error)> callback) override;
    
    void create_preset(const PresetRecord& preset,
                       std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback) override;
    
    void update_preset(const std::string& id, const PresetRecord& preset,
                       std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback) override;
    
    void delete_preset(const std::string& id,
                       std::function<void(bool success, const std::string& error)> callback) override;

    void sync_pull(const std::string& cursor, int limit,
                   std::function<void(bool success, const SyncPullResult& result, const std::string& error)> callback) override;
    
    void sync_push(const std::vector<SyncPushChange>& changes,
                   std::function<void(bool success, const json& result, const std::string& error)> callback) override;
    
    void sync_state(std::function<void(bool success, const SyncState& state, const std::string& error)> callback) override;

private:
    struct Priv;
    std::unique_ptr<Priv> p;
};

} // namespace Slic3r

#endif // slic3r_ConfigDatabase_hpp_
