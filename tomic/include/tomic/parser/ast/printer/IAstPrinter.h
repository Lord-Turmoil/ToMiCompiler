/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

/*
 * There are two AST printers, one will print the whole AST tree, including nodes
 * that are defined by my self. And the other will only print the nodes that is
 * required by the barnacle judge system.
 * Of course, it can be registered to the IoC container.
 */

#ifndef _TOMIC_AST_PRINTER_H_
#define _TOMIC_AST_PRINTER_H_

#include "../../../Common.h"
#include "../ASTVisitor.h"

TOMIC_BEGIN

// This is just a transitional interface class for DI. No other usage.
class IAstPrinter : public ASTVisitor
{
public:
    ~IAstPrinter() override = default;

    virtual IAstPrinter* SetWriter(twio::IWriterPtr writer) = 0;

    virtual void Print(SyntaxTreePtr tree) = 0;
};

TOMIC_END

#endif