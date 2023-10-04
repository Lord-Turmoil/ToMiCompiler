/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include "../../../../include/tomic/parser/ast/SyntaxTree.h"
#include "../../../../include/tomic/parser/ast/SyntaxNode.h"
#include "../../../../include/tomic/parser/ast/printer/XmlAstPrinter.h"
#include "../../../../include/tomic/lexer/token/ITokenMapper.h"

TOMIC_BEGIN

/*
 * ASTPrinter
 */
XmlAstPrinter::XmlAstPrinter() : _depth(0), _indent(2) {}

void XmlAstPrinter::Print(SyntaxTreePtr tree)
{
    TOMIC_ASSERT(tree);

    // To make first element with depth 0, we set depth to -1.
    _depth = -1;

    tree->Accept(this);
}

bool XmlAstPrinter::VisitEnter(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);

    auto container = mioc::SingletonContainer::GetContainer();
    auto mapper = container->Resolve<ISyntacticTypeMapper>();
    auto descr = mapper->Description(node->Type());

    _depth++;

    for (int i = 0; i < _depth * _indent; i++)
    {
        _writer->Write(" ");
    }
    if (descr)
    {
        _writer->WriteFormat("<%s>\n", descr);
    }
    else
    {
        _writer->WriteFormat("<MISSING-%d>\n", _depth);
    }

    return true;
}

bool XmlAstPrinter::VisitExit(tomic::SyntaxNodePtr node)
{
    auto container = mioc::SingletonContainer::GetContainer();
    auto mapper = container->Resolve<ISyntacticTypeMapper>();
    auto descr = mapper->Description(node->Type());

    for (int i = 0; i < _depth * _indent; i++)
    {
        _writer->Write(" ");
    }
    if (descr)
    {
        _writer->WriteFormat("</%s>\n", descr);
    }
    else
    {
        _writer->WriteFormat("</MISSING-%d>\n", _depth);
    }

    _depth--;

    return true;
}

bool XmlAstPrinter::Visit(tomic::SyntaxNodePtr node)
{
    TOMIC_ASSERT(node);

    _depth++;

    if (node->IsNonTerminal())
    {
        _VisitNonTerminal(node);
    }
    else if (node->IsTerminal())
    {
        _VisitTerminal(node);
    }
    else if (node->IsEpsilon())
    {
        _VisitEpsilon(node);
    }
    else
    {
        TOMIC_ASSERT(false && "What the?");
    }

    _depth--;

    return true;
}

void XmlAstPrinter::_VisitNonTerminal(tomic::SyntaxNodePtr node)
{
    auto container = mioc::SingletonContainer::GetContainer();
    auto mapper = container->Resolve<ISyntacticTypeMapper>();
    auto descr = mapper->Description(node->Type());

    for (int i = 0; i < _depth * _indent; i++)
    {
        _writer->Write(" ");
    }
    if (descr)
    {
        _writer->WriteFormat("<%s />\n", descr);
    }
    else
    {
        _writer->WriteFormat("<MISSING-%d />\n", _depth);
    }

    // Visit will not recurse into children.
}

void XmlAstPrinter::_VisitTerminal(tomic::SyntaxNodePtr node)
{
    auto container = mioc::SingletonContainer::GetContainer();
    auto tokenMapper = container->Resolve<ITokenMapper>();
    auto syntacticMapper = container->Resolve<ISyntacticTypeMapper>();
    auto syntacticDescr = syntacticMapper->Description(node->Type());

    for (int i = 0; i < _depth * _indent; i++)
    {
        _writer->Write(" ");
    }
    if (syntacticDescr)
    {
        _writer->WriteFormat("<%s", syntacticDescr);
    }
    else
    {
        _writer->WriteFormat("<MISSING-%d", _depth);
    }

    const char* tokenDescr = tokenMapper->Description(node->Token()->type);
    if (tokenDescr)
    {
        _writer->WriteFormat(" token=\'%s\'", tokenDescr);
    }
    else
    {
        _writer->WriteFormat(" token=\'\'");
    }

    _writer->WriteFormat(" lexeme=\'%s\' />\n", node->Token()->lexeme.c_str());
}

void XmlAstPrinter::_VisitEpsilon(tomic::SyntaxNodePtr node)
{
    auto container = mioc::SingletonContainer::GetContainer();
    auto mapper = container->Resolve<ISyntacticTypeMapper>();
    auto descr = mapper->Description(node->Type());

    for (int i = 0; i < _depth * _indent; i++)
    {
        _writer->Write(" ");
    }
    if (descr)
    {
        _writer->WriteFormat("<%s>\n", descr);
    }
    else
    {
        _writer->WriteFormat("<EPSILON: %d>\n", _depth);
    }
}
TOMIC_END