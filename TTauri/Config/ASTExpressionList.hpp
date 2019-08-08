// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "ASTExpression.hpp"
#include <vector>

namespace TTauri::Config {

/*! Temporary node holding a list of expressions.
 * instances only exists during the execution of bison.
 */
struct ASTExpressionList : ASTNode {
    std::vector<ASTExpression *> expressions;

    ASTExpressionList(Location location, ASTExpression *firstExpression) noexcept : ASTNode(location), expressions({firstExpression}) {}

    ~ASTExpressionList() {
        for (auto expression: expressions) {
            delete expression;
        }
    }

    void add(ASTExpression *x) noexcept {
        expressions.push_back(x);
    }
};

}
