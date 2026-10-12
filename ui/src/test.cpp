#include "test.hpp"

#include "fen_pos.hpp"
#include "position.hpp"

#include "wx/dataview.h"
#include "wx/dialog.h"
#include "wx/sizer.h"
#include <wx/button.h>

#include <iostream>
#include <vector>

std::vector<TestResult> run_perft_tests() {
    std::vector<TestResult> results;
    results.reserve(chess::fen::test_positions.size());

    for (const auto &test : chess::fen::test_positions) {
        TestResult test_result{test, 0, false};

        chess::StateStack st_stack;
        chess::Position pos(st_stack);
        auto err = pos.parse_fen(test.fen);
        if (err != chess::Position::FenParseErr::ok) {
            std::cerr << "[BUG] test fen is invalid: \n";
            std::cerr << "FEN: " << test.fen << '\n';
            std::cerr << chess::Position::format_fen_err(err) << '\n';
            exit(1);
        }

        test_result.nodes = pos.perft(test.depth, [](auto) {}).nodes;
        test_result.is_successful = test_result.nodes == test.nodes;

        results.push_back(test_result);
    }

    return results;
}

TestDialog::TestDialog(wxWindow *parent)
    : wxDialog(
          parent, wxID_ANY, "Perft Tests", wxDefaultPosition, wxSize(1000, 500),
          wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
      ) {
    auto *mainSizer = new wxBoxSizer(wxVERTICAL);

    list = new wxDataViewListCtrl(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES
    );

    list->AppendTextColumn("FEN", wxDATAVIEW_CELL_INERT, 550);
    list->AppendTextColumn("Depth", wxDATAVIEW_CELL_INERT, 60, wxALIGN_CENTER);
    list->AppendTextColumn("Expected", wxDATAVIEW_CELL_INERT, 120, wxALIGN_RIGHT);
    list->AppendTextColumn("Actual", wxDATAVIEW_CELL_INERT, 120, wxALIGN_RIGHT);
    list->AppendTextColumn("Status", wxDATAVIEW_CELL_INERT, 20, wxALIGN_CENTER);

    mainSizer->Add(list, 1, wxEXPAND | wxALL, 10);

    auto *closeButton = new wxButton(this, wxID_CLOSE, "Close");

    mainSizer->Add(closeButton, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CLOSE); }, wxID_CLOSE);

    SetSizer(mainSizer);
    CentreOnParent();
}

void TestDialog::add_test_result(const TestResult &result) {
    wxVector<wxVariant> row;

    row.push_back(wxString::FromUTF8(result.test.fen));
    row.push_back(wxString::Format("%d", result.test.depth));
    row.push_back(wxString::Format("%llu", result.test.nodes));
    row.push_back(wxString::Format("%llu", result.nodes));
    row.push_back(result.is_successful ? "PASS" : "FAIL");

    list->AppendItem(row);
}
