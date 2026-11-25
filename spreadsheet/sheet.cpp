#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>

Sheet::~Sheet() = default;

void Sheet::CheckPosition(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    CheckPosition(pos);
    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        auto cell = std::make_unique<Cell>(*this, pos);
        Cell* new_cell = cell.get();
        cells_.emplace(pos, std::move(cell));
        try {
            new_cell->Set(std::move(text));
        } catch (...) {
            cells_.erase(pos);
            throw;
        }
    } else {
        it->second->Set(std::move(text));
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    CheckPosition(pos);
    auto it = cells_.find(pos);

    if (it == cells_.end()) {
        return nullptr;
    }
    return it->second.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    CheckPosition(pos);
    return FindCell(pos);
}

Cell* Sheet::FindCell(Position pos) {
    auto it = cells_.find(pos);

    if (it == cells_.end()) {
        return nullptr;
    }

    return it->second.get();
}

void Sheet::AddMissingCells(std::vector<std::pair<Position, std::unique_ptr<Cell>>> cells) {
    auto current = cells.begin();
    try {
        for (; current != cells.end(); ++current) {
            auto& [position, cell] = *current;
            if (!cells_.try_emplace(position, std::move(cell)).second) {
                throw std::logic_error("Cell already exists");
            }
        }
    } catch (...) {
        for (auto inserted = cells.begin(); inserted != current; ++inserted) {
            cells_.erase(inserted->first);
        }
        throw;
    }
}

void Sheet::ClearCell(Position pos) {
    CheckPosition(pos);
    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return;
    }
    Cell& cell = *it->second;
    const bool has_dependents = !cell.GetDependedCells().empty();
    cell.Clear();
    if (!has_dependents) {
        cells_.erase(pos);
    }
}

Size Sheet::GetPrintableSize() const {
    if (printable_size_dirty_) {
        printable_size_ = GetActualSize();
        printable_size_dirty_ = false;
    }
    return printable_size_;
}

void Sheet::PrintValues(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        std::visit([&output](const auto& value) { output << value; }, cell.GetValue());
    });
}

void Sheet::PrintTexts(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        output << cell.GetText();
    });
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}

void Sheet::InvalidatePrintableSize() {
    printable_size_dirty_ = true;
}

void Sheet::PrintCells(std::ostream& output,
                       const std::function<void(const CellInterface&)>& print_cell) const {
    const Size size = GetPrintableSize();
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            if (const auto* cell = GetCell({row, col})) {
                print_cell(*cell);
            }
        }
        output << '\n';
    }
}

Size Sheet::GetActualSize() const {
    Size result{0, 0};

    for (const auto& [pos, cell] : cells_) {
        if (!cell->GetText().empty()) {
            result.rows = std::max(result.rows, pos.row + 1);
            result.cols = std::max(result.cols, pos.col + 1);
        }
    }

    return result;
}
