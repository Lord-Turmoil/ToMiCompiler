/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include <tomic/logger/error/ErrorType.h>
#include <tomic/parser/ast/trans/RightRecursiveAstTransformer.h>
#include <tomic/parser/impl/ResilientSyntacticParser.h>

#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <string>

TOMIC_BEGIN

static char _logBuffer[1024];


ResilientSyntacticParser::ResilientSyntacticParser(
    ILexicalParserPtr lexicalParser,
    ISyntaxMapperPtr syntaxMapper,
    ITokenMapperPtr tokenMapper,
    IErrorLoggerPtr errorLogger,
    ILoggerPtr logger)
    : _lexicalParser(lexicalParser),
    _syntaxMapper(syntaxMapper),
    _tokenMapper(tokenMapper),
    _errorLogger(errorLogger),
    _logger(logger)
{
}


ResilientSyntacticParser* ResilientSyntacticParser::SetReader(twio::IAdvancedReaderPtr reader)
{
    _lexicalParser->SetReader(reader);
    return this;
}


TokenPtr ResilientSyntacticParser::_Current()
{
    auto current = _lexicalParser->Current();
    // If it is the very beginning, a compromise lookahead is performd.
    // Nothing can help it if the stream is empty.
    if (!current)
    {
        current = _Lookahead();
    }
    return current;
}


TokenPtr ResilientSyntacticParser::_Next()
{
    return _lexicalParser->Next();
}


TokenPtr ResilientSyntacticParser::_Lookahead(int n)
{
    TOMIC_ASSERT(n > 0);

    int i;
    TokenPtr token = nullptr;

    // Read n tokens.
    for (i = 0; i < n; i++)
    {
        token = _Next();
        // EOF reached.
        if (_Match(TokenType::TK_TERMINATOR, token))
        {
            break;
        }
    }

    // Rewind to the original position.
    for (int j = 0; j < i; j++)
    {
        _lexicalParser->Rewind();
    }

    return token;
}


bool ResilientSyntacticParser::_Match(TokenType type, TokenPtr token)
{
    return Token::Type(token) == type;
}


bool ResilientSyntacticParser::_MatchAny(const std::vector<TokenType>& types, TokenPtr token)
{
    for (auto type : types)
    {
        if (_Match(type, token))
        {
            return true;
        }
    }
    return false;
}


void ResilientSyntacticParser::_PostParseError(int checkpoint, SyntaxNodePtr node)
{
    if (checkpoint >= 0)
    {
        _lexicalParser->Rollback(checkpoint);
    }
    if (node)
    {
        _tree->DeleteNode(node);
    }
}


void ResilientSyntacticParser::_SetTryParse(bool tryParse)
{
    if (tryParse)
    {
        _tryParse++;
    }
    else if (_tryParse > 0)
    {
        // Avoid illegal decrement.
        _tryParse--;
    }
}


void ResilientSyntacticParser::_Log(LogLevel level, TokenPtr position, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    _Log(level, position, format, args);
    va_end(args);
}


void ResilientSyntacticParser::_Log(LogLevel level, TokenPtr position, const char* format, va_list argv)
{
    // When in try-parse mode, do not log.
    if (_IsTryParse())
    {
        return;
    }

    TOMIC_VSPRINTF(_logBuffer, format, argv);

    auto token = position;
    int lineNo = token ? token->lineNo : 1;
    int charNo = token ? token->charNo : 1;

    _logger->LogFormat(level, "(%d:%d) %s", lineNo, charNo, _logBuffer);
}


void ResilientSyntacticParser::_Log(LogLevel level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    _Log(level, _Current(), format, args);
    va_end(args);
}


void ResilientSyntacticParser::_LogFailedToParse(SyntaxType type, LogLevel level)
{
    const char* descr = _syntaxMapper->Description(type);
    if (!descr)
    {
        descr = "MISSING";
    }
    // Since it is the result of other parsing, it is not an error.
    _Log(level, "Failed to parse <%s>", descr);
}


void ResilientSyntacticParser::_LogExpect(TokenType expected, LogLevel level)
{
    auto actual = _Lookahead();

    auto expectedDescr = _tokenMapper->Lexeme(expected);
    if (!expectedDescr)
    {
        expectedDescr = _tokenMapper->Description(expected);
        if (!expectedDescr)
        {
            expectedDescr = "MISSING";
        }
    }

    if (actual->type == TokenType::TK_TERMINATOR)
    {
        _Log(level, actual, "Expect %s, but got EOF", expectedDescr);
    }
    else
    {
        _Log(level, actual, "Expect %s, but got %s", expectedDescr, actual->lexeme.c_str());
    }
}


void ResilientSyntacticParser::_LogExpect(const std::vector<TokenType>& expected, LogLevel level)
{
    std::stringstream stream;

    for (auto type : expected)
    {
        auto descr = _tokenMapper->Lexeme(type);
        if (!descr)
        {
            descr = "MISSING";
        }
        stream << " " << descr;
    }

    _Log(level, "Expect one of %s, but got %s", stream.str().c_str(), _Current()->lexeme.c_str());
}


void ResilientSyntacticParser::_LogExpectAfter(TokenType expected, LogLevel level)
{
    auto current = _Current();
    auto expectedDescr = _tokenMapper->Lexeme(expected);
    if (!expectedDescr)
    {
        expectedDescr = "MISSING";
    }

    _Log(level, current, "Expect %s after %s", expectedDescr, current->lexeme.c_str());
}


void ResilientSyntacticParser::_RecoverFromMissingToken(SyntaxNodePtr node, TokenType expected)
{
    ErrorType type;
    switch (expected)
    {
    case TokenType::TK_SEMICOLON:
        type = ErrorType::ERR_MISSING_SEMICOLON;
        break;
    case TokenType::TK_RIGHT_PARENTHESIS:
        type = ErrorType::ERR_MISSING_RIGHT_PARENTHESIS;
        break;
    case TokenType::TK_RIGHT_BRACKET:
        type = ErrorType::ERR_MISSING_RIGHT_BRACKET;
        break;
    case TokenType::TK_RIGHT_BRACE:
        type = ErrorType::ERR_MISSING_RIGHT_BRACE;
        break;
    default:
        type = ErrorType::ERR_UNKNOWN;
        break;
    }

    auto current = _Current();
    if (current)
    {
        _errorLogger->LogFormat(
            current->lineNo, current->charNo, type,
            "Missing %s after %s",
            _tokenMapper->Lexeme(expected), current->lexeme.c_str());
    }
    else
    {
        _errorLogger->LogFormat(
            1, 1, type,
            "Missing %s at the beginning of file",
            _tokenMapper->Lexeme(expected));
    }

    // Insert a pseudo token.
    node->InsertEndChild(_tree->NewTerminalNode(Token::New(expected)));
}


void ResilientSyntacticParser::_MarkCorrupted(SyntaxNodePtr node)
{
    node->SetBoolAttribute("corrupted", true);
}


/*
 * ========== Parse ==========
 */

SyntaxTreePtr ResilientSyntacticParser::Parse()
{
    _tree = SyntaxTree::New();
    _tryParse = 0;

    auto compUnit = _ParseCompUnit();
    if (!compUnit)
    {
        // TODO: Error handling.
        _logger->LogFormat(LogLevel::FATAL, "Failed to parse the source code.");
        return nullptr;
    }

    _tree->SetRoot(compUnit);
    RightRecursiveAstTransformer().Transform(_tree);

    return _tree;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseCompUnit()
{
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_COMP_UNIT);
    auto checkpoint = _lexicalParser->SetCheckPoint();

    // Parse Decl
    while (_MatchDecl())
    {
        SyntaxNodePtr decl = _ParseDecl();
        if (!decl)
        {
            _LogFailedToParse(SyntaxType::ST_DECL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(decl);
    }

    // Parse FuncDef
    while (_MatchFuncDef())
    {
        SyntaxNodePtr funcDef = _ParseFuncDef();
        if (!funcDef)
        {
            _LogFailedToParse(SyntaxType::ST_FUNC_DEF);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(funcDef);
    }

    // Parse MainFuncDef
    SyntaxNodePtr mainFuncDef = _ParseMainFuncDef();
    if (!mainFuncDef)
    {
        _Log(LogLevel::ERROR, "Failed to parse <MainFuncDef>");
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(mainFuncDef);

    return root;
}


bool ResilientSyntacticParser::_MatchDecl()
{
    // const ...
    if (_Match(TokenType::TK_CONST, _Lookahead()))
    {
        return true;
    }

    // int ident, ...
    // As long as the third one is not '(', it must be a declaration.
    if (_Match(TokenType::TK_INT, _Lookahead()) && _Match(TokenType::TK_IDENTIFIER, _Lookahead(2)))
    {
        return !_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead(3));
    }

    return false;
}


static std::vector<TokenType> _funcDefFirstSet = {
    TokenType::TK_INT,
    TokenType::TK_VOID
};


bool ResilientSyntacticParser::_MatchFuncDef()
{
    if (!_MatchAny(_funcDefFirstSet, _Lookahead()))
    {
        return false;
    }

    return _Match(TokenType::TK_IDENTIFIER, _Lookahead(2)) &&
        _Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead(3));
}


/*
 * ==================== Decl ====================
 */

SyntaxNodePtr ResilientSyntacticParser::_ParseDecl()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_DECL);

    TokenPtr lookahead = _Lookahead();

    if (_Match(TokenType::TK_CONST, lookahead))
    {
        auto constDecl = _ParseConstDecl();
        if (!constDecl)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_DECL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constDecl);
    }
    else
    {
        auto varDecl = _ParseVarDecl();
        if (!varDecl)
        {
            _LogFailedToParse(SyntaxType::ST_VAR_DECL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(varDecl);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseBType()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_BTYPE);

    SyntaxNodePtr child;
    if (_Match(TokenType::TK_INT, _Lookahead()))
    {
        child = _tree->NewTerminalNode(_Next());
    }
    else
    {
        _LogExpect(TokenType::TK_INT);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(child);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseConstDecl()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_CONST_DECL);

    // const
    if (!_Match(TokenType::TK_CONST, _Lookahead()))
    {
        _LogExpect(TokenType::TK_CONST);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // BType
    SyntaxNodePtr type = _ParseBType();
    if (!type)
    {
        _LogFailedToParse(SyntaxType::ST_BTYPE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(type);

    // ConstDef
    SyntaxNodePtr constDef = _ParseConstDef();
    if (!constDef)
    {
        _LogFailedToParse(SyntaxType::ST_CONST_DEF);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(constDef);

    while (_Match(TokenType::TK_COMMA, _Lookahead()))
    {
        // Skip ','
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        constDef = _ParseConstDef();
        if (!constDef)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_DEF);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constDef);
    }

    // Check ;
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseConstDef()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_CONST_DEF);

    // Identifier
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Dimension
    // TODO: Limit dimension.
    while (_Match(TokenType::TK_LEFT_BRACKET, _Lookahead()))
    {
        // Skip '['
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        SyntaxNodePtr constExp = _ParseConstExp();
        if (!constExp)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constExp);

        // Check ']'
        if (!_Match(TokenType::TK_RIGHT_BRACKET, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACKET);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACKET);
        }
        else
        {
            // Skip ']'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
    }

    // '='
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_ASSIGN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // const init val
    auto constInitVal = _ParseConstInitVal();
    if (!constInitVal)
    {
        _LogFailedToParse(SyntaxType::ST_CONST_INIT_VAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(constInitVal);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseConstInitVal()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_CONST_INIT_VAL);

    if (_Match(TokenType::TK_LEFT_BRACE, _Lookahead()))
    {
        // Skip '{'
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        // Check if it is an empty list.
        if (_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
        {
            _logger->LogFormat(LogLevel::WARNING, "Empty initialization list in <ConstInitVal>");
            // Skip '}'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
            return root;
        }

        auto constInitVal = _ParseConstInitVal();
        if (!constInitVal)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_INIT_VAL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constInitVal);

        while (_Match(TokenType::TK_COMMA, _Lookahead()))
        {
            // Skip ','
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));

            constInitVal = _ParseConstInitVal();
            if (!constInitVal)
            {
                _LogFailedToParse(SyntaxType::ST_CONST_INIT_VAL);
                _PostParseError(checkpoint, root);
                return nullptr;
            }
            root->InsertEndChild(constInitVal);
        }

        // Check '}'
        if (!_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACE);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACE);
        }
        else
        {
            // Skip '}'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
    }
    else
    {
        auto constExt = _ParseConstExp();
        if (!constExt)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constExt);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseVarDecl()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_VAR_DECL);

    // BType
    SyntaxNodePtr type = _ParseBType();
    if (!type)
    {
        _LogFailedToParse(SyntaxType::ST_BTYPE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(type);

    // VarDef
    SyntaxNodePtr varDef = _ParseVarDef();
    if (!varDef)
    {
        _LogFailedToParse(SyntaxType::ST_VAR_DEF);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(varDef);

    while (_Match(TokenType::TK_COMMA, _Lookahead()))
    {
        // Skip ','
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        varDef = _ParseVarDef();
        if (!varDef)
        {
            _LogFailedToParse(SyntaxType::ST_VAR_DEF);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(varDef);
    }

    // Check ;
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseVarDef()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_VAR_DEF);

    // Identifier
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    auto identifier = _tree->NewTerminalNode(_Next());
    root->InsertEndChild(identifier);

    // Dimension
    // TODO: Limit dimension.
    while (_Match(TokenType::TK_LEFT_BRACKET, _Lookahead()))
    {
        // Skip '['
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        SyntaxNodePtr constExp = _ParseConstExp();
        if (!constExp)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constExp);

        // Check ']'
        if (!_Match(TokenType::TK_RIGHT_BRACKET, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACKET);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACKET);
        }
        else
        {
            // Skip ']'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
    }

    // Check existence of '=', if not, return success, because it's a declaration.
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _Log(LogLevel::WARNING, "No initial value for %s", identifier->Token()->lexeme.c_str());
        return root;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // init val
    auto initVal = _ParseInitVal();
    if (!initVal)
    {
        _LogFailedToParse(SyntaxType::ST_INIT_VAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(initVal);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseInitVal()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_INIT_VAL);

    if (_Match(TokenType::TK_LEFT_BRACE, _Lookahead()))
    {
        // Skip '{'
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        // Check if it is an empty list.
        if (_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
        {
            _logger->LogFormat(LogLevel::WARNING, "Empty initialization list in <InitVal>");
            // Skip '}'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
            return root;
        }

        auto initVal = _ParseInitVal();
        if (!initVal)
        {
            _LogFailedToParse(SyntaxType::ST_INIT_VAL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(initVal);

        while (_Match(TokenType::TK_COMMA, _Lookahead()))
        {
            // Skip ','
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));

            initVal = _ParseInitVal();
            if (!initVal)
            {
                _LogFailedToParse(SyntaxType::ST_INIT_VAL);
                _PostParseError(checkpoint, root);
                return nullptr;
            }
            root->InsertEndChild(initVal);
        }

        // Check '}'
        if (!_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACE);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACE);
        }
        else
        {
            // Skip '}'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
    }
    else
    {
        auto exp = _ParseExp();
        if (!exp)
        {
            _LogFailedToParse(SyntaxType::ST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(exp);
    }

    return root;
}


/*
 * ==================== FuncDef ====================
 */

SyntaxNodePtr ResilientSyntacticParser::_ParseFuncDef()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_DEF);

    // FuncDecl
    SyntaxNodePtr funcDecl = _ParseFuncDecl();
    if (!funcDecl)
    {
        _LogFailedToParse(SyntaxType::ST_FUNC_DECL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(funcDecl);

    // Block
    SyntaxNodePtr block = _ParseBlock();
    if (!block)
    {
        _LogFailedToParse(SyntaxType::ST_BLOCK);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(block);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncDecl()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_DECL);

    // FuncType
    SyntaxNodePtr funcType = _ParseFuncType();
    if (!funcType)
    {
        _LogFailedToParse(SyntaxType::ST_FUNC_TYPE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(funcType);

    // Identifier
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Check existence of FuncFParams.
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        SyntaxNodePtr funcFParams = _ParseFuncFParams();
        if (funcFParams)
        {
            root->InsertEndChild(funcFParams);
        }
        else
        {
            // We accept this case, but log a warning.
            _LogFailedToParse(SyntaxType::ST_FUNC_FPARAMS);
        }
    }

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncType()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_TYPE);

    if (_MatchAny(_funcDefFirstSet, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }
    else
    {
        _LogExpect(_funcDefFirstSet);
        _PostParseError(checkpoint, root);
        return nullptr;
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncFParams()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_FPARAMS);

    auto funcFParam = _ParseFuncFParam();
    if (!funcFParam)
    {
        _LogFailedToParse(SyntaxType::ST_FUNC_FPARAM);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(funcFParam);

    while (_Match(TokenType::TK_COMMA, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        funcFParam = _ParseFuncFParam();
        if (!funcFParam)
        {
            _LogFailedToParse(SyntaxType::ST_FUNC_FPARAM);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(funcFParam);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncFParam()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_FPARAM);

    // BType
    SyntaxNodePtr type = _ParseBType();
    if (!type)
    {
        _LogFailedToParse(SyntaxType::ST_BTYPE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(type);

    // Identifier
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // first dimension
    if (_Match(TokenType::TK_LEFT_BRACKET, _Lookahead()))
    {
        // Skip '['
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        if (_Match(TokenType::TK_RIGHT_BRACKET, _Lookahead()))
        {
            // Skip ']'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
        else
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACKET);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACKET);
        }

        // second dimension
        if (_Match(TokenType::TK_LEFT_BRACKET, _Lookahead()))
        {
            // Skip '['
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
            auto constExpr = _ParseConstExp();
            if (!constExpr)
            {
                _LogFailedToParse(SyntaxType::ST_CONST_EXP);
                _PostParseError(checkpoint, root);
                return nullptr;
            }
            root->InsertEndChild(constExpr);

            // Check ']'
            if (!_Match(TokenType::TK_RIGHT_BRACKET, _Lookahead()))
            {
                _LogExpectAfter(TokenType::TK_RIGHT_BRACKET);
                _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACKET);
            }
            else
            {
                // Skip ']'
                root->InsertEndChild(_tree->NewTerminalNode(_Next()));
            }
        }
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncAParams()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_APARAMS);

    auto param = _ParseFuncAParam();
    if (!param)
    {
        _LogFailedToParse(SyntaxType::ST_FUNC_APARAM);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(param);

    while (_Match(TokenType::TK_COMMA, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        param = _ParseFuncAParam();
        if (!param)
        {
            _LogFailedToParse(SyntaxType::ST_FUNC_APARAM);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(param);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncAParam()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_APARAM);

    auto exp = _ParseExp();
    if (!exp)
    {
        _LogFailedToParse(SyntaxType::ST_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(exp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseBlock()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_BLOCK);

    // '{'
    if (!_Match(TokenType::TK_LEFT_BRACE, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_BRACE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // If the block is empty, return directly.
    if (_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
    {
        // '}'
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        return root;
    }

    // BlockItem
    while (!_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
    {
        auto blockItem = _ParseBlockItem();
        if (!blockItem)
        {
            _LogFailedToParse(SyntaxType::ST_BLOCK_ITEM);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(blockItem);
    }

    // '}'
    if (!_Match(TokenType::TK_RIGHT_BRACE, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_BRACE);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACE);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseBlockItem()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_BLOCK_ITEM);

    TokenPtr lookahead = _Lookahead();
    if (_Match(TokenType::TK_CONST, lookahead))
    {
        auto constDecl = _ParseConstDecl();
        if (!constDecl)
        {
            _LogFailedToParse(SyntaxType::ST_CONST_DECL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(constDecl);
    }
    else if (_Match(TokenType::TK_INT, lookahead))
    {
        auto varDecl = _ParseVarDecl();
        if (!varDecl)
        {
            _LogFailedToParse(SyntaxType::ST_VAR_DECL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(varDecl);
    }
    else
    {
        auto stmt = _ParseStmt();
        if (!stmt)
        {
            _LogFailedToParse(SyntaxType::ST_STMT);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(stmt);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseMainFuncDef()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_MAIN_FUNC_DEF);

    // int
    if (!_Match(TokenType::TK_INT, _Lookahead()))
    {
        _LogExpect(TokenType::TK_INT);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // main
    if (!_Match(TokenType::TK_MAIN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_MAIN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // (
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // )
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // Block
    SyntaxNodePtr block = _ParseBlock();
    if (!block)
    {
        _LogFailedToParse(SyntaxType::ST_BLOCK);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(block);

    return root;
}


/*
 * ==================== Stmt ====================
 */

/*
 * Statement parsing is a little bit tricky. Since there may be ambiguity between
 * expression statement, assignment statement and input statement. So we have to
 * try parse them. We do this in an auxiliary function _ParseStmtAux() below.
 */
SyntaxNodePtr ResilientSyntacticParser::_ParseStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_STMT);
    TokenPtr lookahead = _Lookahead();

    // ExpStmt, AssignmentStmt and InStmt can all start with an identifier.
    // So we have to try parse them. No other statements can start with an identifier,
    // thus an error shall be reported if we failed to parse.
    if (_Match(TokenType::TK_IDENTIFIER, lookahead))
    {
        _SetTryParse(true);
        auto stmt = _ParseStmtAux();
        _SetTryParse(false);

        if (!stmt)
        {
            _LogFailedToParse(SyntaxType::ST_STMT);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(stmt);
        return root;
    }

    // Now, we can parse other statements.
    SyntaxNodePtr child = nullptr;
    if (_Match(TokenType::TK_IF, lookahead))
    {
        child = _ParseIfStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_IF_STMT);
    }
    else if (_Match(TokenType::TK_FOR, lookahead))
    {
        child = _ParseForStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_FOR_STMT);
    }
    else if (_Match(TokenType::TK_BREAK, lookahead))
    {
        child = _ParseBreakStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_BREAK_STMT);
    }
    else if (_Match(TokenType::TK_CONTINUE, lookahead))
    {
        child = _ParseContinueStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_CONTINUE_STMT);
    }
    else if (_Match(TokenType::TK_RETURN, lookahead))
    {
        child = _ParseReturnStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_RETURN_STMT);
    }
    else if (_Match(TokenType::TK_PRINTF, lookahead))
    {
        child = _ParseOutStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_OUT_STMT);
    }
    else if (_Match(TokenType::TK_LEFT_BRACE, lookahead))
    {
        child = _ParseBlock();
        if (!child) _LogFailedToParse(SyntaxType::ST_BLOCK);
    }
    else
    {
        // If not any of above, it must be an expression statement.
        child = _ParseExpStmt();
        if (!child) _LogFailedToParse(SyntaxType::ST_EXP_STMT);
    }

    // If failed, rollback.
    if (!child)
    {
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(child);

    return root;
}


/*
 * Try parse ambiguous ExpStmt, AssignmentStmt and InStmt.
 */
SyntaxNodePtr ResilientSyntacticParser::_ParseStmtAux()
{
    // InStmt is the simplest, so try parse it first.
    auto inStmt = _ParseInStmt();
    if (inStmt)
    {
        return inStmt;
    }

    // AssignmentStmt is the second simplest, so try parse it second.
    auto assignmentStmt = _ParseAssignmentStmt();
    if (assignmentStmt)
    {
        return assignmentStmt;
    }

    // ExpStmt is the most complex, so try parse it last.
    auto expStmt = _ParseExpStmt();
    if (expStmt)
    {
        return expStmt;
    }

    _Log(LogLevel::DEBUG, "StmtAux didn't match any Stmt");

    return nullptr;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseAssignmentStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_ASSIGNMENT_STMT);

    // LVal
    SyntaxNodePtr lval = _ParseLVal();
    if (!lval)
    {
        _LogFailedToParse(SyntaxType::ST_LVAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(lval);

    // '='
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_ASSIGN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Exp
    SyntaxNodePtr exp = _ParseExp();
    if (!exp)
    {
        _LogFailedToParse(SyntaxType::ST_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(exp);

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseLVal()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_LVAL);

    // Identifier
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Dimension
    while (_Match(TokenType::TK_LEFT_BRACKET, _Lookahead()))
    {
        // Skip '['
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        SyntaxNodePtr exp = _ParseExp();
        if (!exp)
        {
            _LogFailedToParse(SyntaxType::ST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(exp);

        // Check ']'
        if (!_Match(TokenType::TK_RIGHT_BRACKET, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_BRACKET);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_BRACKET);
        }
        else
        {
            // Skip ']'
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseCond()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_COND);

    SyntaxNodePtr orExp = _ParseOrExp();
    if (!orExp)
    {
        _LogFailedToParse(SyntaxType::ST_OR_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(orExp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseIfStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_IF_STMT);

    // if
    if (!_Match(TokenType::TK_IF, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IF);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Cond
    SyntaxNodePtr cond = _ParseCond();
    if (!cond)
    {
        _LogFailedToParse(SyntaxType::ST_COND);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(cond);

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // Stmt
    SyntaxNodePtr stmt = _ParseStmt();
    if (!stmt)
    {
        _LogFailedToParse(SyntaxType::ST_STMT);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(stmt);

    // else
    if (_Match(TokenType::TK_ELSE, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        stmt = _ParseStmt();
        if (!stmt)
        {
            _LogFailedToParse(SyntaxType::ST_STMT);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(stmt);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseForStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FOR_STMT);

    // for
    if (!_Match(TokenType::TK_FOR, _Lookahead()))
    {
        _LogExpect(TokenType::TK_FOR);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Check existence of for init stmt
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        auto forInitStmt = _ParseForInitStmt();
        if (!forInitStmt)
        {
            _LogFailedToParse(SyntaxType::ST_FOR_INIT_STMT);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(forInitStmt);
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // Check existence of Cond
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        auto cond = _ParseCond();
        if (!cond)
        {
            _LogFailedToParse(SyntaxType::ST_COND);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(cond);
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // Check existence of for step stmt
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        auto forStepStmt = _ParseForStepStmt();
        if (!forStepStmt)
        {
            _LogFailedToParse(SyntaxType::ST_FOR_STEP_STMT);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(forStepStmt);
    }

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // Stmt
    SyntaxNodePtr stmt = _ParseStmt();
    if (!stmt)
    {
        _LogFailedToParse(SyntaxType::ST_STMT);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(stmt);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseForInitStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FOR_INIT_STMT);

    // LVal
    SyntaxNodePtr lval = _ParseLVal();
    if (!lval)
    {
        _LogFailedToParse(SyntaxType::ST_LVAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(lval);

    // '='
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_ASSIGN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Exp
    SyntaxNodePtr exp = _ParseExp();
    if (!exp)
    {
        _LogFailedToParse(SyntaxType::ST_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(exp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseForStepStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FOR_STEP_STMT);

    // LVal
    SyntaxNodePtr lval = _ParseLVal();
    if (!lval)
    {
        _LogFailedToParse(SyntaxType::ST_LVAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(lval);

    // '='
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_ASSIGN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Exp
    SyntaxNodePtr exp = _ParseExp();
    if (!exp)
    {
        _LogFailedToParse(SyntaxType::ST_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(exp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseExpStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_EXP_STMT);

    // Check existence of Exp
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        SyntaxNodePtr exp = _ParseExp();
        if (exp)
        {
            root->InsertEndChild(exp);
        }
        // We accept this case, and continue parse ';'.
        // if (!exp)
        // {
        //     _LogFailedToParse(SyntaxType::ST_EXP);
        //     _PostParseError(checkpoint, root);
        //     return nullptr;
        // }
        // root->InsertEndChild(exp);

        /*
         * 2023/11/20 TS: BUG
         * Here, an infinite loop may occur if we failed to parse an expression.
         * So we can read in a token to break the loop. And notice that this
         * token read is not ';', so the error will still be reported.
         */
        if (!exp)
        {
            _lexicalParser->Rollback(checkpoint);
            _Next();    // read in a junk token.
        }
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseBreakStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_BREAK_STMT);

    // break
    if (!_Match(TokenType::TK_BREAK, _Lookahead()))
    {
        _LogExpect(TokenType::TK_BREAK);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseContinueStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_CONTINUE_STMT);

    // continue
    if (!_Match(TokenType::TK_CONTINUE, _Lookahead()))
    {
        _LogExpect(TokenType::TK_CONTINUE);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseReturnStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_RETURN_STMT);

    // return
    if (!_Match(TokenType::TK_RETURN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_RETURN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Check existence of Exp
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        SyntaxNodePtr exp = _ParseExp();
        if (exp)
        {
            root->InsertEndChild(exp);
        }
        else
        {
            // We accept this case, and continue parse ';'.
            _LogFailedToParse(SyntaxType::ST_EXP);
        }
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseInStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_IN_STMT);

    // LVal
    SyntaxNodePtr lval = _ParseLVal();
    if (!lval)
    {
        _LogFailedToParse(SyntaxType::ST_LVAL);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(lval);

    // '='
    if (!_Match(TokenType::TK_ASSIGN, _Lookahead()))
    {
        _LogExpect(TokenType::TK_ASSIGN);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // getint
    if (!_Match(TokenType::TK_GETINT, _Lookahead()))
    {
        _LogExpect(TokenType::TK_GETINT);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseOutStmt()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_OUT_STMT);

    // printf
    if (!_Match(TokenType::TK_PRINTF, _Lookahead()))
    {
        _LogExpect(TokenType::TK_PRINTF);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // FormatString
    if (_Match(TokenType::TK_FORMAT, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    }
    else
    {
        _LogExpect(TokenType::TK_FORMAT);
    }

    // ',' Exp
    while (_Match(TokenType::TK_COMMA, _Lookahead()))
    {
        // Skip ','
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        SyntaxNodePtr exp = _ParseExp();
        if (!exp)
        {
            _LogFailedToParse(SyntaxType::ST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(exp);
    }

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    // ';'
    if (!_Match(TokenType::TK_SEMICOLON, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_SEMICOLON);
        _RecoverFromMissingToken(root, TokenType::TK_SEMICOLON);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_EXP);

    auto addExp = _ParseAddExp();
    if (!addExp)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(addExp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseConstExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_CONST_EXP);

    auto addExp = _ParseAddExp();
    if (!addExp)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(addExp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseAddExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_ADD_EXP);

    auto mulExp = _ParseMulExp();
    if (!mulExp)
    {
        _LogFailedToParse(SyntaxType::ST_MUL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(mulExp);

    auto addExpAux = _ParseAddExpAux();
    if (!addExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that addExpAux can be epsilon.
    if (!addExpAux->IsEpsilon())
    {
        root->InsertEndChild(addExpAux);
    }

    return root;
}


static std::vector<TokenType> _addExpAuxFirstSet = {
    TokenType::TK_PLUS,
    TokenType::TK_MINUS
};


SyntaxNodePtr ResilientSyntacticParser::_ParseAddExpAux()
{
    if (!_MatchAny(_addExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_ADD_EXP);

    // '+' or '-'
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // MulExp
    auto mulExp = _ParseMulExp();
    if (!mulExp)
    {
        _LogFailedToParse(SyntaxType::ST_MUL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(mulExp);

    // AddExpAux
    auto addExpAux = _ParseAddExpAux();
    if (!addExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that addExpAux can be epsilon.
    if (!addExpAux->IsEpsilon())
    {
        root->InsertEndChild(addExpAux);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseMulExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_MUL_EXP);

    auto unaryExp = _ParseUnaryExp();
    if (!unaryExp)
    {
        _LogFailedToParse(SyntaxType::ST_UNARY_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(unaryExp);

    auto mulExpAux = _ParseMulExpAux();
    if (!mulExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_MUL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that mulExpAux can be epsilon.
    if (!mulExpAux->IsEpsilon())
    {
        root->InsertEndChild(mulExpAux);
    }

    return root;
}


static std::vector<TokenType> _mulExpAuxFirstSet = {
    TokenType::TK_MULTIPLY,
    TokenType::TK_DIVIDE,
    TokenType::TK_MOD
};


SyntaxNodePtr ResilientSyntacticParser::_ParseMulExpAux()
{
    if (!_MatchAny(_mulExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_MUL_EXP);

    // '*' or '/' or '%'
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // UnaryExp
    auto unaryExp = _ParseUnaryExp();
    if (!unaryExp)
    {
        _LogFailedToParse(SyntaxType::ST_UNARY_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(unaryExp);

    // MulExpAux
    auto mulExpAux = _ParseMulExpAux();
    if (!mulExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_MUL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that mulExpAux can be epsilon.
    if (!mulExpAux->IsEpsilon())
    {
        root->InsertEndChild(mulExpAux);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseUnaryExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_UNARY_EXP);

    // UnaryExp -> UnaryOp UnaryExp
    auto unaryOp = _ParseUnaryOp();
    if (unaryOp)
    {
        root->InsertEndChild(unaryOp);
        auto unaryExp = _ParseUnaryExp();
        if (!unaryExp)
        {
            _LogFailedToParse(SyntaxType::ST_UNARY_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(unaryExp);
        return root;
    }

    // UnaryExp -> Ident '(' FuncAParams ')'
    if (_Match(TokenType::TK_IDENTIFIER, _Lookahead()) && _Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead(2)))
    {
        auto functionCall = _ParseFuncCall();
        if (!functionCall)
        {
            _LogFailedToParse(SyntaxType::ST_FUNC_CALL);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(functionCall);
        return root;
    }

    // UnaryExp -> PrimaryExp
    auto primaryExp = _ParsePrimaryExp();
    if (!primaryExp)
    {
        _LogFailedToParse(SyntaxType::ST_PRIMARY_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(primaryExp);

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseUnaryOp()
{
    // auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_UNARY_OP);

    // UnaryOp -> '+'
    if (_Match(TokenType::TK_PLUS, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        return root;
    }

    // UnaryOp -> '-'
    if (_Match(TokenType::TK_MINUS, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        return root;
    }

    // UnaryOp -> '!'
    if (_Match(TokenType::TK_NOT, _Lookahead()))
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        return root;
    }

    // It's OK for UnaryOp not to match any.
    // _Log(LogLevel::INFO, "UnaryOp does not find a match");

    return nullptr;
}


SyntaxNodePtr ResilientSyntacticParser::_ParsePrimaryExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_PRIMARY_EXP);

    // PrimaryExp -> Number
    if (_Match(TokenType::TK_INTEGER, _Lookahead()))
    {
        auto number = _ParseNumber();
        if (!number)
        {
            _LogFailedToParse(SyntaxType::ST_NUMBER);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(number);
        return root;
    }

    // PrimaryExp -> '(' Exp ')'
    if (_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        // '('
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));

        // Exp
        auto exp = _ParseExp();
        if (!exp)
        {
            _LogFailedToParse(SyntaxType::ST_EXP);
            _PostParseError(checkpoint, root);
            return nullptr;
        }
        root->InsertEndChild(exp);

        // ')'
        if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
        {
            _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
            _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
        }
        else
        {
            root->InsertEndChild(_tree->NewTerminalNode(_Next()));
        }

        return root;
    }

    // PrimaryExp -> LVal
    auto lval = _ParseLVal();
    if (lval)
    {
        root->InsertEndChild(lval);
        return root;
    }

    // TODO: Error handling.
    _Log(LogLevel::ERROR, "PrimaryExp does not find a match");

    return nullptr;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseFuncCall()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_FUNC_CALL);

    // Ident
    if (!_Match(TokenType::TK_IDENTIFIER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_IDENTIFIER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // '('
    if (!_Match(TokenType::TK_LEFT_PARENTHESIS, _Lookahead()))
    {
        _LogExpect(TokenType::TK_LEFT_PARENTHESIS);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // Check existence of FuncAParams
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        auto funcAParams = _ParseFuncAParams();
        // We just log the error and continue parsing.
        if (funcAParams)
        {
            root->InsertEndChild(funcAParams);
        }
        else
        {
            _MarkCorrupted(root);
            _LogFailedToParse(SyntaxType::ST_FUNC_APARAMS);
        }
    }

    // ')'
    if (!_Match(TokenType::TK_RIGHT_PARENTHESIS, _Lookahead()))
    {
        _LogExpectAfter(TokenType::TK_RIGHT_PARENTHESIS);
        _RecoverFromMissingToken(root, TokenType::TK_RIGHT_PARENTHESIS);
    }
    else
    {
        root->InsertEndChild(_tree->NewTerminalNode(_Next()));
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseNumber()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_NUMBER);

    // Number -> Integer
    if (!_Match(TokenType::TK_INTEGER, _Lookahead()))
    {
        _LogExpect(TokenType::TK_INTEGER);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseOrExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_OR_EXP);

    auto andExp = _ParseAndExp();
    if (!andExp)
    {
        _LogFailedToParse(SyntaxType::ST_AND_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(andExp);

    auto orExpAux = _ParseOrExpAux();
    if (!orExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_OR_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that orExpAux can be epsilon.
    if (!orExpAux->IsEpsilon())
    {
        root->InsertEndChild(orExpAux);
    }

    return root;
}


static std::vector<TokenType> _orExpAuxFirstSet = {
    TokenType::TK_OR
};


SyntaxNodePtr ResilientSyntacticParser::_ParseOrExpAux()
{
    if (!_MatchAny(_orExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_OR_EXP);

    // '||'
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // AndExp
    auto andExp = _ParseAndExp();
    if (!andExp)
    {
        _LogFailedToParse(SyntaxType::ST_AND_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(andExp);

    // OrExpAux
    auto orExpAux = _ParseOrExpAux();
    if (!orExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_OR_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that orExpAux can be epsilon.
    if (!orExpAux->IsEpsilon())
    {
        root->InsertEndChild(orExpAux);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseAndExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_AND_EXP);

    auto eqExp = _ParseEqExp();
    if (!eqExp)
    {
        _LogFailedToParse(SyntaxType::ST_EQ_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(eqExp);

    auto andExpAux = _ParseAndExpAux();
    if (!andExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_AND_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that andExpAux can be epsilon.
    if (!andExpAux->IsEpsilon())
    {
        root->InsertEndChild(andExpAux);
    }

    return root;
}


static std::vector<TokenType> _andExpAuxFirstSet = {
    TokenType::TK_AND
};


SyntaxNodePtr ResilientSyntacticParser::_ParseAndExpAux()
{
    if (!_MatchAny(_andExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_AND_EXP);

    // '&&'
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // EqExp
    auto eqExp = _ParseEqExp();
    if (!eqExp)
    {
        _LogFailedToParse(SyntaxType::ST_EQ_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(eqExp);

    // AndExpAux
    auto andExpAux = _ParseAndExpAux();
    if (!andExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_AND_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that andExpAux can be epsilon.
    if (!andExpAux->IsEpsilon())
    {
        root->InsertEndChild(andExpAux);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseEqExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_EQ_EXP);

    auto relExp = _ParseRelExp();
    if (!relExp)
    {
        _LogFailedToParse(SyntaxType::ST_REL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(relExp);

    auto eqExpAux = _ParseEqExpAux();
    if (!eqExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_EQ_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that eqExpAux can be epsilon.
    if (!eqExpAux->IsEpsilon())
    {
        root->InsertEndChild(eqExpAux);
    }

    return root;
}


static std::vector<TokenType> _eqExpAuxFirstSet = {
    TokenType::TK_EQUAL,
    TokenType::TK_NOT_EQUAL
};


SyntaxNodePtr ResilientSyntacticParser::_ParseEqExpAux()
{
    if (!_MatchAny(_eqExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_EQ_EXP);

    // '==' or '!='
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // RelExp
    auto relExp = _ParseRelExp();
    if (!relExp)
    {
        _LogFailedToParse(SyntaxType::ST_REL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(relExp);

    // EqExpAux
    auto eqExpAux = _ParseEqExpAux();
    if (!eqExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_EQ_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that eqExpAux can be epsilon.
    if (!eqExpAux->IsEpsilon())
    {
        root->InsertEndChild(eqExpAux);
    }

    return root;
}


SyntaxNodePtr ResilientSyntacticParser::_ParseRelExp()
{
    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_REL_EXP);

    auto addExp = _ParseAddExp();
    if (!addExp)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(addExp);

    auto relExpAux = _ParseRelExpAux();
    if (!relExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_REL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that relExpAux can be epsilon.
    if (!relExpAux->IsEpsilon())
    {
        root->InsertEndChild(relExpAux);
    }

    return root;
}


static std::vector<TokenType> _relExpAuxFirstSet = {
    TokenType::TK_LESS,
    TokenType::TK_LESS_EQUAL,
    TokenType::TK_GREATER,
    TokenType::TK_GREATER_EQUAL
};


SyntaxNodePtr ResilientSyntacticParser::_ParseRelExpAux()
{
    if (!_MatchAny(_relExpAuxFirstSet, _Lookahead()))
    {
        return _tree->NewEpsilonNode();
    }

    auto checkpoint = _lexicalParser->SetCheckPoint();
    auto root = _tree->NewNonTerminalNode(SyntaxType::ST_REL_EXP);

    // '<' or '<=' or '>' or '>='
    // Already checked in _MatchAny().
    root->InsertEndChild(_tree->NewTerminalNode(_Next()));

    // AddExp
    auto addExp = _ParseAddExp();
    if (!addExp)
    {
        _LogFailedToParse(SyntaxType::ST_ADD_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    root->InsertEndChild(addExp);

    // RelExpAux
    auto relExpAux = _ParseRelExpAux();
    if (!relExpAux)
    {
        _LogFailedToParse(SyntaxType::ST_REL_EXP);
        _PostParseError(checkpoint, root);
        return nullptr;
    }
    // Notice that relExpAux can be epsilon.
    if (!relExpAux->IsEpsilon())
    {
        root->InsertEndChild(relExpAux);
    }

    return root;
}


TOMIC_END
