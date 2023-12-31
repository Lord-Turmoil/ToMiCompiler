/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_AST_TRANSFORMER_H_
#define _TOMIC_AST_TRANSFORMER_H_

#include <tomic/parser/ast/AstVisitor.h>
#include <tomic/Shared.h>

TOMIC_BEGIN

class IAstTransformer : public AstVisitor
{
public:
    ~IAstTransformer() override = default;

    virtual SyntaxTreePtr Transform(SyntaxTreePtr tree) = 0;
};


TOMIC_END

#endif // _TOMIC_AST_TRANSFORMER_H_
