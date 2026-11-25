#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) : ast_(ParseFormulaAST(expression)) {}

    Value Evaluate(const SheetInterface& sheet) const override {
        Value result;
        try {
            result = ast_.Execute(sheet);
        } catch (const FormulaError& fe) {
            result = fe;
        }
        return result;
    }

    std::string GetExpression() const override { 
        std::ostringstream tmp;
        ast_.PrintFormula(tmp);
        return tmp.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        return ast_.GetCells();
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}