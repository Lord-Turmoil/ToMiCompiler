/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_XML_AST_PRINTER_H_
#define _TOMIC_XML_AST_PRINTER_H_

#include <tomic/lexer/token/ITokenMapper.h>
#include <tomic/parser/ast/AstForward.h>
#include <tomic/parser/ast/AstVisitor.h>
#include <tomic/parser/ast/mapper/ISyntaxMapper.h>
#include <tomic/parser/ast/printer/IAstPrinter.h>
#include <tomic/Shared.h>

TOMIC_BEGIN

class XmlAstPrinter : public IAstPrinter, private AstVisitor
{
public:
    XmlAstPrinter(ISyntaxMapperPtr syntaxMapperPtr, ITokenMapperPtr tokenMapper);
    ~XmlAstPrinter() override = default;

    void Print(SyntaxTreePtr tree, twio::IWriterPtr writer) override;

private:
    bool VisitEnter(SyntaxNodePtr node) override;
    bool VisitExit(SyntaxNodePtr node) override;
    bool Visit(SyntaxNodePtr node) override;

    void _VisitNonTerminal(SyntaxNodePtr node);
    void _VisitTerminal(SyntaxNodePtr node);
    void _VisitEpsilon(SyntaxNodePtr node);

    void _PrintIndent(int depth);

private:
    twio::IWriterPtr _writer;
    ISyntaxMapperPtr _syntaxMapper;
    ITokenMapperPtr _tokenMapper;
    int _depth;
    int _indent;
};


TOMIC_END

#endif // _TOMIC_XML_AST_PRINTER_H_
