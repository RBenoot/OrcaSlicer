#include "SyncManager.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "ConfigDatabase.hpp"
#include <boost/log/trivial.hpp>
#include <boost/core/null_deleter.hpp>
#include <chrono>
#include <thread>

namespace Slic3r {

std::shared_ptr<SyncManager> SyncManager::instance()
{
    static std::shared_ptr<SyncManager> inst(new SyncManager(), [](SyncManager* p) { delete p; });
    return inst;
}

SyncManager::SyncManager()
{
    m_status = SyncStatus::Offline;
}

SyncManager::~SyncManager()
{
    stop_background_sync();
}

void SyncManager::queue_change(const std::string& preset_id, const std::string& preset_name,
                               const std::string& preset_type, const std::string& action)
{
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_queue.emplace(preset_id, preset_name, preset_type, action);
    m_pending_count++;
    BOOST_LOG_TRIVIAL(info) << "Queued change: " << action << " for " << preset_name;
}

void SyncManager::login_and_sync(const std::string& username, const std::string& password,
                                 std::function<void(bool success, const std::string& error)> callback)
{
    if (!m_db) {
        callback(false, "ConfigDatabase not set");
        return;
    }
    
    m_status = SyncStatus::Syncing;
    
    m_db->login(username, password, [this, callback](bool success, const std::string& error) {
        if (!success) {
            m_status = SyncStatus::Error;
            m_online = false;
            callback(false, "Login failed: " + error);
            return;
        }
        
        m_online = true;
        m_last_error.clear();
        m_status = SyncStatus::Idle;
        sync(callback);
    });
}

void SyncManager::logout()
{
    stop_background_sync();
    if (m_db) {
        m_db->clear_auth_tokens();
    }
    m_online = false;
    m_status = SyncStatus::Offline;
}

void SyncManager::sync(std::function<void(bool success, const std::string& error)> callback)
{
    m_sync_callback = callback;
    sync_async();
}

void SyncManager::sync_async()
{
    std::lock_guard<std::mutex> lock(m_sync_mutex);
    
    if (m_status == SyncStatus::Syncing) {
        return;
    }
    
    if (!m_online) {
        m_status = SyncStatus::Offline;
        if (m_sync_callback) {
            m_sync_callback(false, "Not online");
        }
        return;
    }
    
    m_status = SyncStatus::Syncing;
    
    std::thread([this]() {
        push_to_server();
        pull_from_server();
    }).detach();
}

void SyncManager::push_to_server()
{
    std::vector<SyncPushChange> changes;
    
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_queue.empty()) {
            auto item = m_queue.front();
            
            SyncPushChange change;
            change.id = item.preset_id;
            change.setting_id = item.preset_id;
            change.name = item.preset_name;
            change.type = item.preset_type;
            change.sync_status = item.action;
            change.updated_at = item.timestamp;
            change.content = json::object();
            
            if (m_bundle && item.action != "delete") {
                std::string file_path;
                if (item.preset_type == "filament") {
                    if (auto* p = m_bundle->filaments.find_preset(item.preset_name)) file_path = p->file;
                } else if (item.preset_type == "machine") {
                    if (auto* p = m_bundle->printers.find_preset(item.preset_name)) file_path = p->file;
                } else if (item.preset_type == "process") {
                    if (auto* p = m_bundle->prints.find_preset(item.preset_name)) file_path = p->file;
                } else if (item.preset_type == "physical_printer") {
                    if (auto* p = m_bundle->physical_printers.find_printer(item.preset_name)) file_path = p->file;
                }
                
                if (!file_path.empty()) {
                    try {
                        std::ifstream ifs(file_path);
                        change.content = json::parse(ifs);
                    } catch(...) {
                        // Keep empty object on failure
                    }
                }
            }
            
            changes.push_back(change);
            m_queue.pop();
            m_pending_count--;
        }
    }
    
    if (changes.empty()) {
        return;
    }
    
    if (!m_db) return;
    
    m_db->sync_push(changes, [changes](bool success, const json& result, const std::string& error) {
        if (!success) {
            BOOST_LOG_TRIVIAL(error) << "Push failed: " << error;
        } else {
            BOOST_LOG_TRIVIAL(info) << "Pushed " << changes.size() << " changes to server";
        }
    });
}

void SyncManager::pull_from_server()
{
    if (!m_db) {
        m_status = SyncStatus::Error;
        if (m_sync_callback) {
            m_sync_callback(false, "ConfigDatabase not set");
        }
        return;
    }
    
    auto cursor_ptr = std::make_shared<std::string>("");
    auto fetch_next = std::make_shared<std::function<void()>>();
    
    *fetch_next = [this, fetch_next, cursor_ptr]() {
        m_db->sync_pull(*cursor_ptr, 100, [this, fetch_next, cursor_ptr](bool success, const SyncPullResult& result, const std::string& error) {
            if (!success) {
                m_status = SyncStatus::Error;
                m_last_error = error;
                if (m_sync_callback) {
                    m_sync_callback(false, error);
                }
                return;
            }
            
            if (m_bundle) {
                for (const auto& record : result.upserts) {
                    BOOST_LOG_TRIVIAL(info) << "Pulled preset: " << record.name;
                }
            }
            
            *cursor_ptr = result.next_cursor;
            if (!cursor_ptr->empty()) {
                (*fetch_next)();
            } else {
                m_status = SyncStatus::Idle;
                if (m_sync_callback) {
                    m_sync_callback(true, "");
                }
                BOOST_LOG_TRIVIAL(info) << "Sync completed";
            }
        });
    };
    
    (*fetch_next)();
}

void SyncManager::resolve_conflict(const std::string& preset_id, bool use_server)
{
    std::lock_guard<std::mutex> lock(m_conflicts_mutex);
    
    for (auto it = m_conflicts.begin(); it != m_conflicts.end(); ) {
        if (it->preset_id == preset_id) {
            if (use_server) {
                BOOST_LOG_TRIVIAL(info) << "Conflict resolved (server wins): " << preset_id;
            } else {
                BOOST_LOG_TRIVIAL(info) << "Conflict resolved (client wins): " << preset_id;
            }
            it = m_conflicts.erase(it);
        } else {
            ++it;
        }
    }
}

void SyncManager::start_background_sync()
{
    if (m_running) return;
    
    m_running = true;
    m_sync_thread = std::thread([this]() {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::seconds(m_sync_interval));
            if (m_running && m_online) {
                sync_async();
            }
        }
    });
}

void SyncManager::stop_background_sync()
{
    m_running = false;
    if (m_sync_thread.joinable()) {
        m_sync_thread.join();
    }
}

} // namespace Slic3r
