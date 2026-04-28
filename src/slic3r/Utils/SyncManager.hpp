#ifndef slic3r_SyncManager_hpp_
#define slic3r_SyncManager_hpp_

#include "PresetBundleDB.hpp"
#include "ConfigDatabase.hpp"
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>

namespace Slic3r {

enum class SyncStatus {
    Idle,
    Syncing,
    Error,
    Offline
};

enum class ConflictResolution {
    ServerWins,
    ClientWins,
    Manual
};

struct SyncQueueItem {
    std::string preset_id;
    std::string preset_name;
    std::string preset_type;
    std::string action; // "create", "update", "delete"
    int64_t timestamp;
    int retry_count;
    
    SyncQueueItem(const std::string& id, const std::string& name, 
                  const std::string& type, const std::string& act)
        : preset_id(id), preset_name(name), preset_type(type), 
          action(act), timestamp(0), retry_count(0) {}
};

struct SyncConflict {
    std::string preset_id;
    std::string preset_name;
    int64_t local_timestamp;
    int64_t server_timestamp;
    std::string local_hash;
    std::string server_hash;
};

class SyncManager
{
public:
    static std::shared_ptr<SyncManager> instance();
    
    void set_config_database(std::shared_ptr<ConfigDatabase> db) { 
        m_db = db; 
        if (db) {
            m_status = SyncStatus::Idle;
        }
    }
    
    void set_preset_bundle(PresetBundle* bundle) { m_bundle = bundle; }
    
    void set_conflict_resolution(ConflictResolution resolution) { m_conflict_resolution = resolution; }
    
    SyncStatus get_status() const { return m_status; }
    std::string get_last_error() const { return m_last_error; }
    int get_queue_size() const { return m_queue.size(); }
    int get_pending_changes() const { return m_pending_count; }
    
    void queue_change(const std::string& preset_id, const std::string& preset_name,
                     const std::string& preset_type, const std::string& action);
    
    void sync(std::function<void(bool success, const std::string& error)> callback);
    
    void sync_async();
    
    void set_sync_interval(int seconds) { m_sync_interval = seconds; }
    
    bool is_online() const { return m_online; }
    
    void login_and_sync(const std::string& username, const std::string& password,
                       std::function<void(bool success, const std::string& error)> callback);
    
    void logout();
    
    std::vector<SyncConflict> get_conflicts() const { return m_conflicts; }
    
    void resolve_conflict(const std::string& preset_id, bool use_server);
    
    void start_background_sync();
    void stop_background_sync();

private:
    SyncManager();
    ~SyncManager();
    
    void process_queue();
    void push_to_server();
    void pull_from_server();
    
    std::shared_ptr<ConfigDatabase> m_db;
    PresetBundle* m_bundle = nullptr;
    
    std::queue<SyncQueueItem> m_queue;
    std::mutex m_queue_mutex;
    std::atomic<int> m_pending_count{0};
    
    std::vector<SyncConflict> m_conflicts;
    std::mutex m_conflicts_mutex;
    
    ConflictResolution m_conflict_resolution = ConflictResolution::ServerWins;
    SyncStatus m_status = SyncStatus::Offline;
    std::string m_last_error;
    bool m_online = false;
    bool m_running = false;
    int m_sync_interval = 300;
    std::thread m_sync_thread;
    std::mutex m_sync_mutex;
    
    std::function<void(bool, const std::string&)> m_sync_callback;
};

} // namespace Slic3r

#endif // slic3r_SyncManager_hpp_
