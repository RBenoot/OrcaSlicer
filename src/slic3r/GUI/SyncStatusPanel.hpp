#ifndef _SyncStatusPanel_hpp_
#define _SyncStatusPanel_hpp_

#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/gdicmn.h>

namespace Slic3r {
namespace GUI {

class SyncStatusPanel : public wxPanel
{
public:
    enum SyncState {
        STATE_OFFLINE,
        STATE_IDLE,
        STATE_SYNCING,
        STATE_ERROR
    };

    SyncStatusPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    ~SyncStatusPanel();

    void set_state(SyncState state);
    void set_pending_count(int count);
    void set_last_sync_time(const wxString& time);
    void set_error_message(const wxString& msg);

    SyncState get_state() const { return m_state; }

    void on_sync_click(wxCommandEvent& event);
    void on_timer(wxTimerEvent& event);

protected:
    wxStaticText* m_status_text;
    wxButton* m_sync_button;
    wxTimer m_refresh_timer;
    SyncState m_state;
    int m_pending_count;

    void update_ui();
    wxColour get_state_color() const;
};

} // namespace GUI
} // namespace Slic3r

#endif // _SyncStatusPanel_hpp_
