/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TMOIC_SYNTAX_TREE_H_
#define _TMOIC_SYNTAX_TREE_H_

#include <tomic/lexer/token/Token.h>
#include <tomic/parser/ast/AstForward.h>
#include <tomic/parser/ast/SyntaxType.h>

#include <memory>
#include <unordered_set>

TOMIC_BEGIN

/*
 * Syntactic Tree will manage the memory allocation of all the syntactic nodes.
 * So the syntactic nodes should not be created by the user directly, that is, the
 * constructors and destructors of the syntactic nodes are private. Therefore,
 * smart pointer are not used here.
 * Although under ast directory, it is actually not an AST, since it contains all
 * non-terminal nodes. But I guess it is OK to treat it as an AST. :?
 */
class SyntaxTree
{
public:
    SyntaxTree() = default;
    ~SyntaxTree();

    // Prohibit copying and cloning.
    SyntaxTree(const SyntaxTree&) = delete;
    SyntaxTree& operator=(const SyntaxTree&) = delete;
    SyntaxTree(SyntaxTree&&) = delete;
    SyntaxTree& operator=(SyntaxTree&&) = delete;

    // Create a new syntactic tree.
    static std::shared_ptr<SyntaxTree> New();

public:
    // Create a new syntactic node, which is self-managed by the syntactic tree.
    SyntaxNodePtr NewTerminalNode(TokenPtr token);
    SyntaxNodePtr NewNonTerminalNode(SyntaxType type);
    SyntaxNodePtr NewEpsilonNode();

    void DeleteNode(SyntaxNodePtr node);

    SyntaxNodePtr Root() const { return _root; }
    SyntaxNodePtr SetRoot(SyntaxNodePtr root);

    // For visitor pattern. A utility function to traverse the tree.
    bool Accept(AstVisitorPtr visitor);

private:
    void _ClearUp();

    SyntaxNodePtr _root;
    std::unordered_set<SyntaxNodePtr> _nodes;
};


using SyntaxTreePtr = std::shared_ptr<SyntaxTree>;

TOMIC_END

#endif // _TMOIC_SYNTAX_TREE_H_
