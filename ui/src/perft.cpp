#include "perft.hpp"

#include <wx/wx.h>

PerftTextBox::PerftTextBox(wxWindow *parent, perft_fn perft, uint16_t depth)
    : wxTextCtrl(
          parent, wxID_ANY, std::to_string(depth), wxPoint(705, 250), wxSize(70, 25),
          wxTE_PROCESS_ENTER
      ),
      m_perft(perft) {
    SetToolTip("Hit Enter/Return after entering the perft depth.");
    Bind(wxEVT_TEXT_ENTER, &PerftTextBox::OnEnter, this);
}
void PerftTextBox::OnEnter(wxCommandEvent &) { m_perft(GetValue().ToStdString()); }

wxDEFINE_EVENT(EVT_PERFT_OUTPUT, wxThreadEvent);

PerftDialog::PerftDialog(wxWindow *parent, const chess::Position &position)
    : wxDialog(parent, wxID_ANY, "Perft", wxDefaultPosition, wxSize(450, 650)),
      m_position(position) {
    SetFont(wxFont(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    auto *sizer = new wxBoxSizer(wxVERTICAL);

    m_output = new wxTextCtrl(
        this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY
    );

    sizer->Add(m_output, 1, wxEXPAND | wxALL, 10);

    SetSizer(sizer);

    Bind(EVT_PERFT_OUTPUT, &PerftDialog::on_perft_output, this);
    Bind(wxEVT_CLOSE_WINDOW, &PerftDialog::OnClose, this);
}

PerftDialog::~PerftDialog() {
    m_thread.request_stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void PerftDialog::run(uint8_t depth) {
    m_output->Clear();

    chess::Position position = m_position;

    m_thread = std::jthread(
        [this, depth, position = std::move(position)](std::stop_token stop_token) mutable {
            uint64_t nodes = position
                                 .perft(
                                     depth,
                                     [this](std::string_view line) {
                                         auto *event = new wxThreadEvent(EVT_PERFT_OUTPUT);
                                         event->SetString(wxString::FromUTF8(line));
                                         wxQueueEvent(this, event);
                                     },
                                     stop_token
                                 )
                                 .nodes;

            auto *event = new wxThreadEvent();
            event->SetString(wxString::Format("%llu", static_cast<unsigned long long>(nodes)));
            wxQueueEvent(this, event);
        }
    );
}

void PerftDialog::OnClose(wxCloseEvent &event) {
    m_thread.request_stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    Destroy();
}
void PerftDialog::on_perft_output(wxThreadEvent &event) {
    m_output->AppendText(event.GetString() + "\n");
}
