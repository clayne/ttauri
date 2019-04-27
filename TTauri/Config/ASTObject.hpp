
#pragma once

#include "ASTExpression.hpp"
#include "ASTExpressions.hpp"
#include <vector>

namespace TTauri::Config {

struct ASTObject : ASTExpression {
    std::vector<ASTExpression *> expressions;

    ASTObject(ASTLocation location) : ASTExpression(location), expressions() { }

    ASTObject(ASTLocation location, ASTExpression *expression) : ASTExpression(location), expressions({expression}) {
    }

    ASTObject(ASTLocation location, ASTExpressions *expressions) : ASTExpression(location), expressions(expressions->expressions) {
        // We copied the pointers of the expression, so they must not be destructed when expressions is deleted.
        expressions->expressions.clear();
        delete expressions;
    }

    ~ASTObject() {
        for (auto expression: expressions) {
            delete expression;
        }
    }

    std::string str() override {
        std::string s = "{";

        bool first = true;
        for (auto const expression: expressions) {
            if (!first) {
                s += ",";
            }
            s += expression->str();
            first = false;
        }

        s += "}";
        return s;
    }

    virtual std::shared_ptr<ValueBase> execute(ExecutionContext *context) override {
        auto r = std::make_shared<ValueObject>();
        context->objectStack.push_back(r);

        for (auto const expression: expressions) {
            expression->executeStatement(context);
        }

        context->objectStack.pop_back();
        return r;
    } 

};

}