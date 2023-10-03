/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

/*
 * Dedicated to TinyXML, which is a great library.
 */

#ifndef _TOMIC_SYNTAX_NODE_H_
#define _TOMIC_SYNTAX_NODE_H_

#include "../../../../include/tomic/Common.h"
#include "../../lexer/token/Token.h"
#include "SyntaxType.h"
#include <vector>

TOMIC_BEGIN

class SyntaxNode;
using SyntaxNodePtr = SyntaxNode*;

class SyntaxTree;
using SyntaxTreePtr = std::shared_ptr<SyntaxTree>;

class ASTVisitor;
using ASTVisitorPtr = ASTVisitor*;

class SyntaxNode
{
    friend class SyntaxTree;
public:
    // Insert a child node to the end of the children list.
    // Return the added child.
    SyntaxNodePtr InsertEndChild(SyntaxNodePtr child);
    SyntaxNodePtr InsertFirstChild(SyntaxNodePtr child);
    SyntaxNodePtr InsertAfterChild(SyntaxNodePtr child, SyntaxNodePtr after);

    // Get the root of the tree, can be a sub-tree.
    SyntaxNodePtr Root() const;

    // For visitor pattern.
    virtual bool Accept(ASTVisitorPtr visitor) = 0;

public:
    bool HasChildren() const { return _firstChild; }

    SyntaxNodePtr Parent() const { return _parent; }

    SyntaxNodePtr FirstChild() const { return _firstChild; }

    SyntaxNodePtr LastChild() const { return _lastChild; }

    SyntaxNodePtr NextSibling() const { return _next; }

    SyntaxNodePtr PrevSibling() const { return _prev; }

private:
    void _InsertChildPreamble(SyntaxNodePtr child);
    void _DeleteChildPreamble(SyntaxNodePtr child);
    SyntaxNodePtr _Unlink(SyntaxNodePtr child);

protected:
    SyntaxNode(SyntaxType type);
    SyntaxNode(SyntaxType type, TokenPtr token);
    virtual ~SyntaxNode() = default;

    // AST links.
    SyntaxTree* _tree;

    SyntaxNodePtr _parent;
    SyntaxNodePtr _prev;
    SyntaxNodePtr _next;

    SyntaxNodePtr _firstChild;
    SyntaxNodePtr _lastChild;

    // AST properties.
    SyntaxType _type;
    TokenPtr _token;
};

class NonTerminalSyntaxNode : public SyntaxNode
{
    friend class SyntaxTree;
protected:
    NonTerminalSyntaxNode(SyntaxType type);

    ~NonTerminalSyntaxNode() override = default;

    bool Accept(ASTVisitorPtr visitor) override;
};


class TerminalSyntaxNode : public SyntaxNode
{
    friend class SyntaxTree;
protected:
    TerminalSyntaxNode(TokenPtr token);

    ~TerminalSyntaxNode() override = default;

    bool Accept(ASTVisitorPtr visitor) override;
};

TOMIC_END

#endif // _TOMIC_SYNTAX_NODE_H_
