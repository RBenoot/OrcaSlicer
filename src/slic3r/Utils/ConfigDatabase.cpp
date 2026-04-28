#include "ConfigDatabase.hpp"
#include "Http.hpp"
#include <boost/algorithm/string/replace.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r {

struct ConfigDatabaseRestClient::Priv {
    std::string base_url = "http://localhost:5000";
    std::string auth_token;
    std::string refresh_token;
    
    std::string make_url(const std::string& path) {
        return base_url + path;
    }
    
    void add_auth_header(Http& http) {
        if (!auth_token.empty()) {
            http.header("Authorization", "Bearer " + auth_token);
        }
    }
    
    PresetRecord parse_preset(const json& j) {
        PresetRecord p;
        p.id = j.value("id", "");
        p.vendor_id = j.value("vendorId", "");
        p.vendor_name = j.value("vendorName", "");
        p.type = j.value("type", "");
        p.name = j.value("name", "");
        p.inherits = j.value("inherits", "");
        p.is_system = j.value("isSystem", false);
        p.is_template = j.value("isTemplate", false);
        p.filament_id = j.value("filamentId", "");
        p.setting_id = j.value("settingId", "");
        p.base_id = j.value("baseId", "");
        p.sync_status = j.value("syncStatus", "");
        p.updated_time = j.value("updatedTime", 0);
        p.user_id = j.value("userId", "");
        p.created_at = j.value("createdAt", 0);
        p.updated_at = j.value("updatedAt", 0);
        
        if (j.contains("config") && !j["config"].is_null()) {
            p.config = j["config"];
        } else {
            p.config = json::object();
        }
        return p;
    }
    
    json preset_to_json(const PresetRecord& p) {
        return {
            {"type", p.type},
            {"name", p.name},
            {"inherits", p.inherits.empty() ? nullptr : p.inherits},
            {"config", p.config},
            {"isSystem", p.is_system},
            {"isTemplate", p.is_template},
            {"filamentId", p.filament_id.empty() ? nullptr : p.filament_id},
            {"settingId", p.setting_id.empty() ? nullptr : p.setting_id},
            {"baseId", p.base_id.empty() ? nullptr : p.base_id}
        };
    }
};

std::shared_ptr<ConfigDatabase> ConfigDatabase::instance()
{
    return std::make_shared<ConfigDatabaseRestClient>();
}

ConfigDatabaseRestClient::ConfigDatabaseRestClient()
    : p(std::make_unique<Priv>())
{
}

ConfigDatabaseRestClient::~ConfigDatabaseRestClient() = default;

void ConfigDatabaseRestClient::set_base_url(const std::string& url)
{
    p->base_url = url;
    if (p->base_url.back() == '/') {
        p->base_url.pop_back();
    }
}

void ConfigDatabaseRestClient::set_auth_tokens(const AuthTokens& tokens)
{
    p->auth_token = tokens.token;
    p->refresh_token = tokens.refresh_token;
}

void ConfigDatabaseRestClient::clear_auth_tokens()
{
    p->auth_token.clear();
    p->refresh_token.clear();
}

bool ConfigDatabaseRestClient::is_authenticated() const
{
    return !p->auth_token.empty();
}

void ConfigDatabaseRestClient::login(
    const std::string& username,
    const std::string& password,
    std::function<void(bool success, const std::string& error)> callback)
{
    json body = {
        {"username", username},
        {"password", password}
    };
    
    Http::post(p->make_url("/api/auth/login"))
        .header("Content-Type", "application/json")
        .set_post_body(body.dump())
        .on_complete([this, callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    AuthTokens tokens;
                    tokens.token = j.value("token", "");
                    tokens.refresh_token = j.value("refreshToken", "");
                    tokens.expires_at = j.value("expiresAt", 0);
                    set_auth_tokens(tokens);
                    callback(true, "");
                } catch (const std::exception& e) {
                    callback(false, std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, "Login failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::fetch_presets(
    const std::string& type,
    std::function<void(bool success, const std::vector<PresetRecord>& presets, const std::string& error)> callback)
{
    std::string url = p->make_url("/api/presets?type=" + type);
    
    Http::get(url)
        .on_complete([this, callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    std::vector<PresetRecord> results;
                    for (const auto& item : j) {
                        results.push_back(p->parse_preset(item));
                    }
                    callback(true, results, "");
                } catch (const std::exception& e) {
                    callback(false, {}, std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, {}, "Request failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, {}, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::fetch_preset(
    const std::string& id,
    std::function<void(bool success, const PresetRecord* preset, const std::string& error)> callback)
{
    std::string url = p->make_url("/api/presets/" + id);
    
    Http::get(url)
        .on_complete([this, callback, id](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    PresetRecord* result = new PresetRecord(p->parse_preset(j));
                    callback(true, result, "");
                } catch (const std::exception& e) {
                    callback(false, nullptr, std::string("Parse error: ") + e.what());
                }
            } else if (status == 404) {
                callback(false, nullptr, "Preset not found");
            } else {
                callback(false, nullptr, "Request failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, nullptr, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::create_preset(
    const PresetRecord& preset,
    std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback)
{
    Http::post(p->make_url("/api/presets"))
        .header("Content-Type", "application/json")
        .set_post_body(p->preset_to_json(preset).dump())
        .on_complete([this, callback](std::string body, unsigned status) {
            if (status == 201) {
                try {
                    auto j = json::parse(body);
                    PresetRecord* result = new PresetRecord(p->parse_preset(j));
                    callback(true, result, "");
                } catch (const std::exception& e) {
                    callback(false, nullptr, std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, nullptr, "Create failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, nullptr, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::update_preset(
    const std::string& id,
    const PresetRecord& preset,
    std::function<void(bool success, const PresetRecord* result, const std::string& error)> callback)
{
    Http::put(p->make_url("/api/presets/" + id))
        .header("Content-Type", "application/json")
        .set_post_body(p->preset_to_json(preset).dump())
        .on_complete([this, callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    PresetRecord* result = new PresetRecord(p->parse_preset(j));
                    callback(true, result, "");
                } catch (const std::exception& e) {
                    callback(false, nullptr, std::string("Parse error: ") + e.what());
                }
            } else if (status == 404) {
                callback(false, nullptr, "Preset not found");
            } else {
                callback(false, nullptr, "Update failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, nullptr, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::delete_preset(
    const std::string& id,
    std::function<void(bool success, const std::string& error)> callback)
{
    Http::del(p->make_url("/api/presets/" + id))
        .on_complete([callback](std::string body, unsigned status) {
            if (status == 204 || status == 200) {
                callback(true, "");
            } else if (status == 404) {
                callback(false, "Preset not found");
            } else {
                callback(false, "Delete failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::sync_pull(
    const std::string& cursor,
    int limit,
    std::function<void(bool success, const SyncPullResult& result, const std::string& error)> callback)
{
    std::string url = p->make_url("/api/sync/pull?limit=" + std::to_string(limit));
    if (!cursor.empty()) {
        url += "&cursor=" + cursor;
    }
    
    Http::get(url)
        .on_complete([this, callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    SyncPullResult result;
                    result.next_cursor = j.value("nextCursor", "");
                    
                    for (const auto& item : j["upserts"]) {
                        result.upserts.push_back(p->parse_preset(item));
                    }
                    
                    for (const auto& item : j["deletes"]) {
                        result.deletes.push_back(item.get<std::string>());
                    }
                    
                    callback(true, result, "");
                } catch (const std::exception& e) {
                    callback(false, {}, std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, {}, "Sync pull failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, {}, "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::sync_push(
    const std::vector<SyncPushChange>& changes,
    std::function<void(bool success, const json& result, const std::string& error)> callback)
{
    json body = json::array();
    for (const auto& change : changes) {
        body.push_back({
            {"id", change.id.empty() ? nullptr : change.id},
            {"settingId", change.setting_id.empty() ? nullptr : change.setting_id},
            {"name", change.name},
            {"type", change.type},
            {"content", change.content},
            {"syncStatus", change.sync_status},
            {"updatedAt", change.updated_at}
        });
    }
    
    Http::post(p->make_url("/api/sync/push"))
        .header("Content-Type", "application/json")
        .set_post_body(body.dump())
        .on_complete([callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    callback(true, j, "");
                } catch (const std::exception& e) {
                    callback(false, json::object(), std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, json::object(), "Sync push failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, json::object(), "Request error: " + error);
        })
        .perform();
}

void ConfigDatabaseRestClient::sync_state(
    std::function<void(bool success, const SyncState& state, const std::string& error)> callback)
{
    Http::get(p->make_url("/api/sync/state"))
        .on_complete([callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    SyncState state;
                    state.user_id = j.value("userId", "");
                    state.last_sync_timestamp = j.value("lastSyncTimestamp", 0);
                    state.sync_cursor = j.value("syncCursor", "");
                    callback(true, state, "");
                } catch (const std::exception& e) {
                    callback(false, {}, std::string("Parse error: ") + e.what());
                }
            } else {
                callback(false, {}, "Sync state failed with status " + std::to_string(status));
            }
        })
        .on_error([callback](std::string body, std::string error, unsigned status) {
            callback(false, {}, "Request error: " + error);
        })
        .perform();
}

} // namespace Slic3r
