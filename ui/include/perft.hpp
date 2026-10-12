#pragma once

#include "position.hpp"

#include <cstdint>
#include <thread>

#include <wx/wx.h>

class PerftTextBox : public wxTextCtrl {
  public:
    using perft_fn = std::function<void(std::string_view)>;

    PerftTextBox(wxWindow *parent, perft_fn perft, uint16_t depth);

  private:
    perft_fn m_perft;

    void OnEnter(wxCommandEvent &);
};

wxDECLARE_EVENT(EVT_PERFT_OUTPUT, wxThreadEvent);

class PerftDialog : public wxDialog {
  private:
    chess::Position m_position;
    std::jthread m_thread;
    wxTextCtrl *m_output = nullptr;

  public:
    PerftDialog(wxWindow *parent, const chess::Position &position);

    ~PerftDialog();

    void run(uint8_t depth);

  private:
    void perft_output(std::string_view line);
    void OnClose(wxCloseEvent &event);
    void on_perft_output(wxThreadEvent &event);
};
