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
        auto get_str = [&j](const std::string& key) { return (j.contains(key) && !j[key].is_null()) ? j[key].get<std::string>() : ""; };
        auto get_bool = [&j](const std::string& key) { return (j.contains(key) && !j[key].is_null()) ? j[key].get<bool>() : false; };
        auto get_int = [&j](const std::string& key) { return (j.contains(key) && !j[key].is_null()) ? j[key].get<int64_t>() : 0; };
        
        p.id = get_str("id");
        p.vendor_id = get_str("vendorId");
        p.vendor_name = get_str("vendorName");
        p.type = get_str("type");
        p.name = get_str("name");
        p.inherits = get_str("inherits");
        p.is_system = get_bool("isSystem");
        p.is_template = get_bool("isTemplate");
        p.filament_id = get_str("filamentId");
        p.setting_id = get_str("settingId");
        p.base_id = get_str("baseId");
        p.sync_status = get_str("syncStatus");
        p.updated_time = get_int("updatedTime");
        p.user_id = get_str("userId");
        p.created_at = get_int("createdAt");
        p.updated_at = get_int("updatedAt");
        
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
            {"inherits", p.inherits.empty() ? json(nullptr) : json(p.inherits)},
            {"config", p.config},
            {"isSystem", p.is_system},
            {"isTemplate", p.is_template},
            {"filamentId", p.filament_id.empty() ? json(nullptr) : json(p.filament_id)},
            {"settingId", p.setting_id.empty() ? json(nullptr) : json(p.setting_id)},
            {"baseId", p.base_id.empty() ? json(nullptr) : json(p.base_id)}
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
                    tokens.token = (j.contains("token") && !j["token"].is_null()) ? j["token"].get<std::string>() : "";
                    tokens.refresh_token = (j.contains("refreshToken") && !j["refreshToken"].is_null()) ? j["refreshToken"].get<std::string>() : "";
                    tokens.expires_at = (j.contains("expiresAt") && !j["expiresAt"].is_null()) ? j["expiresAt"].get<int64_t>() : 0;
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
    
    auto http = Http::get(url);
    p->add_auth_header(http);
    http.on_complete([this, callback](std::string body, unsigned status) {
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
    
    auto http = Http::get(url);
    p->add_auth_header(http);
    http.on_complete([this, callback, id](std::string body, unsigned status) {
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
    auto http = Http::post(p->make_url("/api/presets"));
    p->add_auth_header(http);
    http.header("Content-Type", "application/json")
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
    auto http = Http::put(p->make_url("/api/presets/" + id));
    p->add_auth_header(http);
    http.header("Content-Type", "application/json")
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
    auto http = Http::del(p->make_url("/api/presets/" + id));
    p->add_auth_header(http);
    http.on_complete([callback](std::string body, unsigned status) {
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
    
    auto http = Http::get(url);
    p->add_auth_header(http);
    http.on_complete([this, callback](std::string body, unsigned status) {
            if (status == 200) {
                try {
                    auto j = json::parse(body);
                    SyncPullResult result;
                    if (j.contains("nextCursor") && !j["nextCursor"].is_null()) {
                        result.next_cursor = j["nextCursor"].get<std::string>();
                    } else {
                        result.next_cursor = "";
                    }
                    
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
    json body = json::object();
    json changes_array = json::array();
    for (const auto& change : changes) {
        changes_array.push_back({
            {"id", change.id.empty() ? json(nullptr) : json(change.id)},
            {"settingId", change.setting_id.empty() ? json(nullptr) : json(change.setting_id)},
            {"name", change.name},
            {"type", change.type},
            {"content", change.content},
            {"syncStatus", change.sync_status},
            {"updatedAt", change.updated_at}
        });
    }
    body["changes"] = changes_array;
    
    auto http = Http::post(p->make_url("/api/sync/push"));
    p->add_auth_header(http);
    http.header("Content-Type", "application/json")
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
    auto http = Http::get(p->make_url("/api/sync/state"));
    p->add_auth_header(http);
    http.on_complete([callback](std::string body, unsigned status) {
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
