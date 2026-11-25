#include "cell.h"
#include "common.h"
#include "sheet.h"

#include <algorithm>

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

namespace {
bool IsFormula(std::string_view text) {
    return text.size() > 1 && text.front() == FORMULA_SIGN;
}
}  // namespace

class Cell::EmptyImpl : public Cell::Impl {
public:
    EmptyImpl() = default;

    virtual Cell::Value GetValue() const override {
        return "";
    }

    virtual std::string GetText() const override {
        return "";
    }

    std::string_view GetRawText() const override {
        return {};
    }
};

Cell::Cell(Sheet& sheet, Position position)
    : impl_(std::make_unique<EmptyImpl>())
    , sheet_(sheet)
    , position_(position) {
}

class Cell::TextImpl : public Cell::Impl {
public:
    TextImpl(std::string text) : text_(std::move(text)) {}

    virtual Cell::Value GetValue() const override {

        if (text_[0] == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }

    virtual std::string GetText() const override {
        return text_;
    }

    std::string_view GetRawText() const override {
        return text_;
    }

private:
    std::string text_;
};

class Cell::FormulaImpl : public Cell::Impl {
public:
    FormulaImpl(std::string text, const SheetInterface& sheet)
        : raw_text_(std::move(text))
        , formula_(ParseFormula(raw_text_.substr(1)))
        , sheet_(sheet) {
    }

    Value GetValue() const override {
        return std::visit([](const auto& value) -> Value {
            return value;
            }, formula_->Evaluate(sheet_));
    }

    std::string GetText() const override {
        return FORMULA_SIGN + formula_->GetExpression();
    }

    std::string_view GetRawText() const override {
        return raw_text_;
    }

    std::vector<Position> GetReferencedCells() const {
        return formula_->GetReferencedCells();
    }

private:
    std::string raw_text_;
    std::unique_ptr<FormulaInterface> formula_;
    const SheetInterface& sheet_;
};

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    if (text == impl_->GetRawText()) {
        return;
    }

    std::unique_ptr<Impl> new_impl;
    std::vector<Position> references;
    if (text.empty()) {
        new_impl = std::make_unique<EmptyImpl>();
    } else if (IsFormula(text)) {
        auto formula_impl = std::make_unique<FormulaImpl>(std::move(text), sheet_);
        references = formula_impl->GetReferencedCells();
        if (HaveCycle(references)) {
            throw CircularDependencyException(
                "Circular dependency found while setting cell " + position_.ToString());
        }
        new_impl = std::move(formula_impl);
    } else {
        new_impl = std::make_unique<TextImpl>(std::move(text));
    }

    InvalidateCache();
    UpdateReferences(references);
    impl_ = std::move(new_impl);
    sheet_.InvalidatePrintableSize();
}

void Cell::Clear() {
    Set("");
}

Cell::Value Cell::GetValue() const {
    const auto* formula = dynamic_cast<const FormulaImpl*>(impl_.get());
    if (!formula) {
        return impl_->GetValue();
    }

    if (!cache_) {
        cache_ = formula->GetValue();
    }
    return *cache_;
}

std::string Cell::GetText() const {
    return impl_->GetText();
}


bool Cell::FindCycle(std::unordered_map<Position, Cell::Color, PositionHasher>& colors) const {
    auto it = colors.find(position_);
    if (it != colors.end()) {
        if (it->second == Cell::Color::IN_PROCESS) {
            return true;
        }
        if (it->second == Cell::Color::PROCESSED) {
            return false;
        }
    }
    colors[position_] = Cell::Color::IN_PROCESS;
    const auto reference = GetReferencedCells();
    bool cycle = false;
    for (const auto& position : reference) {
        Cell* cell = sheet_.FindCell(position);
        cycle |= cell && cell->FindCycle(colors);
    }
    colors[position_] = Cell::Color::PROCESSED;
    return cycle;
}

bool Cell::HaveCycle(const std::vector<Position>& new_references) const {
    std::unordered_map<Position, Cell::Color, PositionHasher> colors;
    colors[position_] = Cell::Color::IN_PROCESS;
    bool cycle = false;
    for (const auto& position : new_references) {
        Cell* cell = sheet_.FindCell(position);
        cycle |= position == position_ || (cell && cell->FindCycle(colors));
    }
    colors[position_] = Cell::Color::PROCESSED;
    return cycle;
}

void Cell::Invalidate(std::unordered_set<Position, PositionHasher>& visited) {
    if (!visited.insert(position_).second) {
        return;
    }
    const auto dependent_positions = GetDependedCells();
    cache_.reset();
    for (const auto& position : dependent_positions) {
        Cell* cell = sheet_.FindCell(position);
        if (cell) {
            cell->Invalidate(visited);
        }
    }
}

void Cell::UpdateReferences(const std::vector<Position>& new_references) { //метод обеспечивает строгую гарантию
    decltype(referenced_cells_) next_references(
        new_references.begin(), new_references.end());

    decltype(sheet_.cells_) new_cells;
    std::unordered_map<Cell*, decltype(dependent_cells_)> next_dependents;

    for (const Position& pos : referenced_cells_) {
        if (Cell* cell = sheet_.FindCell(pos)) {
            auto [it, inserted] =
                next_dependents.try_emplace(cell, cell->dependent_cells_);
            it->second.erase(position_);
        }
    }

    for (const Position& pos : next_references) {
        if (Cell* cell = sheet_.FindCell(pos)) {
            auto [it, inserted] =
                next_dependents.try_emplace(cell, cell->dependent_cells_);
            it->second.insert(position_);
        } else {
            auto empty_cell = std::make_unique<Cell>(sheet_, pos);
            empty_cell->dependent_cells_.insert(position_);
            new_cells.emplace(pos, std::move(empty_cell));
        }
    }

    if (!new_cells.empty()) {
        sheet_.cells_.reserve(sheet_.cells_.size() + new_cells.size());
        sheet_.cells_.merge(new_cells);
    }

    for (auto& [cell, dependents] : next_dependents) {
        cell->dependent_cells_.swap(dependents);
    }
    referenced_cells_.swap(next_references);
}

std::vector<Position> Cell::GetReferencedCells() const {
    std::vector<Position> references(referenced_cells_.begin(), referenced_cells_.end());
    std::sort(references.begin(), references.end());
    return references;
}

std::vector<Position> Cell::GetDependedCells() const {
    return {dependent_cells_.begin(), dependent_cells_.end()};
}

void Cell::InvalidateCache() {
    std::unordered_set<Position, PositionHasher> visited;
    Invalidate(visited);
}
