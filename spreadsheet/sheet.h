#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;
    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;
    Size GetPrintableSize() const override;
    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    Cell* FindCell(Position pos);
    void AddMissingCells(std::vector<std::pair<Position, std::unique_ptr<Cell>>> cells);
    void InvalidatePrintableSize();

private:
    static void CheckPosition(Position pos);
    void PrintCells(std::ostream& output,
                    const std::function<void(const CellInterface&)>& print_cell) const;
    Size GetActualSize() const;

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher> cells_;
    mutable Size printable_size_{};
    mutable bool printable_size_dirty_ = false;
};
