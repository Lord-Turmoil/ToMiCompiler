/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include "../../../../include/tomic/parser/ast/SyntaxTree.h"
#include "../../../../include/tomic/parser/ast/SyntaxNode.h"
#include "../../../../include/tomic/parser/ast/printer/StandardAstPrinter.h"

TOMIC_BEGIN

/*
 * ASTPrinter
 */

StandardAstPrinter::StandardAstPrinter(tomic::ISyntaxMapperPtr syntaxMapperPtr, tomic::ITokenMapperPtr tokenMapper)
        : _syntaxMapper(syntaxMapperPtr), _tokenMapper(tokenMapper)
{
}

void StandardAstPrinter::Print(SyntaxTreePtr tree, twio::IWriterPtr writer)
{
    TOMIC_ASSERT(tree);
    TOMIC_ASSERT(writer);

    _writer = writer;

    tree->Accept(this);
}

bool StandardAstPrinter::VisitEnter(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);
    return true;
}

bool StandardAstPrinter::VisitExit(SyntaxNodePtr node)
{
    if (node->IsNonTerminal())
    {
        _VisitNonTerminal(node);
    }

    return true;
}

bool StandardAstPrinter::Visit(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);

    if (node->IsTerminal())
    {
        _VisitTerminal(node);
    }
    else if (node->IsEpsilon())
    {
        _VisitEpsilon(node);
    }
    else
    {
        _VisitNonTerminal(node);
    }

    return true;
}

void StandardAstPrinter::_VisitNonTerminal(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);
    TOMIC_ASSERT(node->IsNonTerminal());

    auto descr = _syntaxMapper->Description(node->Type());
    if (descr)
    {
        _writer->WriteFormat("<%s>\n", descr);
    }
}

void StandardAstPrinter::_VisitTerminal(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);
    TOMIC_ASSERT(node->IsTerminal());

    auto token = node->Token();
    auto descr = _tokenMapper->Description(token->type);

    _writer->WriteFormat("%s", descr ? descr : "MISSING");
    _writer->Write(" ");
    _writer->Write(token->lexeme.c_str());
    _writer->Write("\n");
}

void StandardAstPrinter::_VisitEpsilon(SyntaxNodePtr node)
{
    return;
}

TOMIC_END
