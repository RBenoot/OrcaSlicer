#include "SyncStatusPanel.hpp"
#include "SyncManager.hpp"
#include "GUI_App.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include <wx/sizer.h>

namespace Slic3r {
namespace GUI {

SyncStatusPanel::SyncStatusPanel(wxWindow* parent, wxWindowID id)
    : wxControl(parent, id)
    , m_state(STATE_OFFLINE)
    , m_pending_count(0)
    , m_refresh_timer(this)
{
    SetBackgroundColour(wxColour(48, 48, 48));

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_status_text = new wxStaticText(this, wxID_ANY, _("Offline"));
    m_status_text->SetForegroundColour(wxColour(180, 180, 180));
    
    m_sync_button = new Button(this, _("Sync Now"));
    m_sync_button->SetBackgroundColour(wxColour(0, 145, 255));
    m_sync_button->SetTextColor(wxColour(255, 255, 255));
    
    sizer->Add(m_status_text, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    sizer->Add(m_sync_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    
    SetSizer(sizer);
    SetMinSize(wxSize(200, 36));
    
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        Layout();
        e.Skip();
    });
    
    m_sync_button->Bind(wxEVT_BUTTON, &SyncStatusPanel::on_sync_click, this);
    m_refresh_timer.Start(5000);
    Bind(wxEVT_TIMER, &SyncStatusPanel::on_timer, this);
    
    update_ui();
}

SyncStatusPanel::~SyncStatusPanel()
{
    m_refresh_timer.Stop();
}

void SyncStatusPanel::set_state(SyncState state)
{
    m_state = state;
    update_ui();
}

void SyncStatusPanel::set_pending_count(int count)
{
    m_pending_count = count;
    update_ui();
}

void SyncStatusPanel::set_last_sync_time(const wxString& time)
{
    wxString text;
    switch (m_state) {
        case STATE_OFFLINE:
            text = wxString::Format(_("Offline | Last sync: %s"), time);
            break;
        case STATE_IDLE:
            text = wxString::Format(_("Synced | Last sync: %s"), time);
            break;
        case STATE_SYNCING:
            text = _("Syncing...");
            break;
        case STATE_ERROR:
            text = _("Sync Error");
            break;
    }
    m_status_text->SetLabel(text);
}

void SyncStatusPanel::set_error_message(const wxString& msg)
{
    if (m_state == STATE_ERROR) {
        m_status_text->SetLabel(wxString::Format(_("Error: %s"), msg));
    }
}

void SyncStatusPanel::update_ui()
{
    wxColour color = get_state_color();
    m_status_text->SetForegroundColour(color);
    
    switch (m_state) {
        case STATE_OFFLINE:
            m_status_text->SetLabel(_("Offline"));
            m_sync_button->Enable(true);
            m_sync_button->SetLabel(_("Connect"));
            break;
        case STATE_IDLE:
            if (m_pending_count > 0) {
                m_status_text->SetLabel(wxString::Format(_("Pending: %d changes"), m_pending_count));
            } else {
                m_status_text->SetLabel(_("Synced"));
            }
            m_sync_button->Enable(true);
            m_sync_button->SetLabel(_("Sync Now"));
            break;
        case STATE_SYNCING:
            m_status_text->SetLabel(_("Syncing..."));
            m_sync_button->Enable(false);
            m_sync_button->SetLabel(_("Syncing..."));
            break;
        case STATE_ERROR:
            m_status_text->SetLabel(_("Sync Error"));
            m_sync_button->Enable(true);
            m_sync_button->SetLabel(_("Retry"));
            break;
    }
}

wxColour SyncStatusPanel::get_state_color() const
{
    switch (m_state) {
        case STATE_OFFLINE:  return wxColour(150, 150, 150);
        case STATE_IDLE:     return wxColour(100, 200, 100);
        case STATE_SYNCING:  return wxColour(0, 145, 255);
        case STATE_ERROR:    return wxColour(255, 80, 80);
    }
    return wxColour(200, 200, 200);
}

void SyncStatusPanel::on_sync_click(wxCommandEvent& event)
{
    set_state(STATE_SYNCING);
    
    auto sync_mgr = SyncManager::instance();
    if (sync_mgr) {
        if (!sync_mgr->is_online()) {
            // Zorg dat de database geconfigureerd is
            auto db = ConfigDatabase::instance();
            sync_mgr->set_config_database(db);
            
            // Log in met de hardcoded admin credentials uit de backend
            sync_mgr->login_and_sync("admin", "admin", [](bool success, const std::string& err) {});
        } else {
            sync_mgr->sync_async();
        }
    }
}

void SyncStatusPanel::on_timer(wxTimerEvent& event)
{
    auto sync_mgr = SyncManager::instance();
    if (sync_mgr) {
        switch (sync_mgr->get_status()) {
            case SyncStatus::Idle:
                set_state(STATE_IDLE);
                set_pending_count(sync_mgr->get_pending_changes());
                break;
            case SyncStatus::Syncing:
                set_state(STATE_SYNCING);
                break;
            case SyncStatus::Error:
                set_state(STATE_ERROR);
                set_error_message(sync_mgr->get_last_error());
                break;
            case SyncStatus::Offline:
                set_state(STATE_OFFLINE);
                break;
        }
    }
}

} // namespace GUI
} // namespace Slic3r
