#pragma once

#include "common.h"
#include "formula.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet, Position position);
    ~Cell();

    void Set(std::string text) override;
    void Clear();

    std::vector<Position> GetReferencedCells() const override;
    std::vector<Position> GetDependedCells() const;
    bool HaveCycle(const std::vector<Position>& new_references) const;

    void InvalidateCache();
    bool HaveCache() const;

    Value GetValue() const override;
    std::string GetText() const override;

private:

    class Impl {
    public:
        virtual ~Impl() = default;

        virtual Value GetValue() const = 0;
        virtual std::string GetText() const = 0;
    };

    enum class Color {
        NOT_VISITED = 0,
        IN_PROCESS = 1,
        PROCESSED = 2
    };

    bool FindCycle(std::unordered_map<Position, Color, PositionHasher>& colors) const;
    void Invalidate(std::unordered_set<Position, PositionHasher>& visited);
    void UpdateReferences(const std::vector<Position>& new_references);

    mutable std::optional<Value> cache_;
    std::unordered_set<Position, PositionHasher> referenced_cells_;
    std::unordered_set<Position, PositionHasher> dependent_cells_;

    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;
    std::unique_ptr<Impl> impl_;

    Sheet& sheet_;
    Position position_;
};
