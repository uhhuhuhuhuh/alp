#pragma once

#include "fen_pos.hpp"

#include "wx/dataview.h"
#include "wx/dialog.h"
#include <wx/button.h>

#include <cstdint>
#include <vector>

struct TestResult {
    const chess::fen::TestPerft &test;
    uint64_t nodes;
    bool is_successful;
};

std::vector<TestResult> run_perft_tests();

class TestDialog : public wxDialog {
  private:
    wxDataViewListCtrl *list = nullptr;

  public:
    TestDialog(wxWindow *parent);

    void add_test_result(const TestResult &result);
};
