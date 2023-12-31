/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include <tomic/llvm/asm/impl/StandardAsmGenerator.h>
#include <tomic/llvm/ir/DerivedTypes.h>
#include <tomic/llvm/ir/LlvmContext.h>
#include <tomic/llvm/ir/Module.h>
#include <tomic/llvm/ir/Type.h>
#include <tomic/llvm/ir/value/BasicBlock.h>
#include <tomic/llvm/ir/value/ConstantData.h>
#include <tomic/llvm/ir/value/Function.h>
#include <tomic/llvm/ir/value/GlobalVariable.h>
#include <tomic/llvm/ir/value/inst/Instructions.h>
#include <tomic/llvm/ir/value/inst/ExtendedInstructions.h>
#include <tomic/llvm/ir/value/Value.h>
#include <tomic/parser/ast/SyntaxNode.h>
#include <tomic/parser/table/SymbolTableBlock.h>
#include <tomic/utils/SemanticUtil.h>
#include <tomic/utils/StringUtil.h>

TOMIC_LLVM_BEGIN

StandardAsmGenerator::StandardAsmGenerator()
    : _currentFunction(nullptr), _currentBlock(nullptr)
{
}


ModuleSmartPtr StandardAsmGenerator::Generate(
    SyntaxTreePtr syntaxTree,
    SymbolTablePtr symbolTable,
    const char* name)
{
    _syntaxTree = syntaxTree;
    _symbolTable = symbolTable;

    // Module will check if the name is nullptr or not.
    _module = Module::New(name);

    if (!_ParseCompilationUnit())
    {
        return nullptr;
    }

    return _module;
}


/*
 * ==================== Global Variable Parsing ====================
 */

// node is a Decl
void StandardAsmGenerator::_ParseGlobalDecl(SyntaxNodePtr node)
{
    auto child = node->FirstChild();
    if (child->Type() == SyntaxType::ST_VAR_DECL)
    {
        for (auto it = child->FirstChild(); it; it = it->NextSibling())
        {
            if (it->Type() == SyntaxType::ST_VAR_DEF)
            {
                _ParseGlobalVarDef(it);
            }
        }
    }
    else if (child->Type() == SyntaxType::ST_CONST_DECL)
    {
        for (auto it = child->FirstChild(); it; it = it->NextSibling())
        {
            if (it->Type() == SyntaxType::ST_CONST_DEF)
            {
                _ParseGlobalConstantDef(it);
            }
        }
    }
    else
    {
        TOMIC_PANIC("Illegal type for Decl");
    }
}


GlobalVariablePtr StandardAsmGenerator::_ParseGlobalVarDef(SyntaxNodePtr node)
{
    // Get variable name.
    const std::string& name = node->FirstChild()->Token()->lexeme;
    auto entry = _GetSymbolTableBlock(node)->FindEntry(name);

    /*
     * Warning: Global values must be pointer type.
     * This is done automatically in constructor, so we don't need to
     * do this explicitly.
     */
    auto type = _GetEntryType(entry);
    GlobalVariablePtr value;

    if (node->LastChild()->Type() == SyntaxType::ST_INIT_VAL)
    {
        // with init value
        auto initValue = _ParseGlobalInitValue(node->LastChild());
        value = GlobalVariable::New(type, false, name, initValue);
    }
    else
    {
        value = GlobalVariable::New(type, false, name);
    }

    // Add the value to the symbol table and module.
    _AddValue(entry, value);
    _module->AddGlobalVariable(value);

    return value;
}


GlobalVariablePtr StandardAsmGenerator::_ParseGlobalConstantDef(SyntaxNodePtr node)
{
    // Get constant name.
    const std::string& name = node->FirstChild()->Token()->lexeme;
    auto entry = _GetSymbolTableBlock(node)->FindEntry(name);
    // Warning: Global values must be pointer type.
    auto type = _module->Context()->GetPointerType(_GetEntryType(entry));
    GlobalVariablePtr value;

    if (node->LastChild()->Type() == SyntaxType::ST_CONST_INIT_VAL)
    {
        // with init value
        auto initValue = _ParseGlobalInitValue(node->LastChild());
        value = GlobalVariable::New(type, true, name, initValue);
    }
    else
    {
        TOMIC_PANIC("Constant must have init value");
        return nullptr;
    }

    // Add the value to the symbol table and module.
    _AddValue(entry, value);
    _module->AddGlobalVariable(value);

    return value;
}


// node is a InitVal or ConstInitVal.
ConstantDataPtr StandardAsmGenerator::_ParseGlobalInitValue(SyntaxNodePtr node)
{
    if (!node->BoolAttribute("det"))
    {
        TOMIC_PANIC("Global initialization value must be deterministic");
    }

    int dim = node->IntAttribute("dim");
    if (dim == 0)
    {
        return ConstantData::New(_module->Context()->GetInt32Ty(), node->IntAttribute("value"));
    }

    std::vector<ConstantDataPtr> values;
    for (auto it = node->FirstChild(); it; it = it->NextSibling())
    {
        if ((it->Type() == SyntaxType::ST_CONST_INIT_VAL) || (it->Type() == SyntaxType::ST_INIT_VAL))
        {
            values.push_back(_ParseGlobalInitValue(it));
        }
    }

    return ConstantData::New(values);
}


/*
 * ==================== Local Variable Parsing ====================
 */
void StandardAsmGenerator::_ParseVariableDecl(SyntaxNodePtr node)
{
    if (node->Type() == SyntaxType::ST_VAR_DECL || node->Type() == SyntaxType::ST_CONST_DECL)
    {
        for (auto it = node->FirstChild(); it; it = it->NextSibling())
        {
            if (it->Type() == SyntaxType::ST_VAR_DEF || it->Type() == SyntaxType::ST_CONST_DEF)
            {
                if (it->IntAttribute("dim") == 0)
                {
                    _ParseVariableDef(it);
                }
                else
                {
                    _ParseArrayDef(it);
                }
            }
        }
    }
    else
    {
        TOMIC_PANIC("Illegal type for Decl");
    }
}


AllocaInstPtr StandardAsmGenerator::_ParseVariableDef(SyntaxNodePtr node)
{
    // Get variable name.
    const std::string& name = node->FirstChild()->Token()->lexeme;
    auto entry = _GetSymbolTableBlock(node)->FindEntry(name);
    auto type = _GetEntryType(entry);
    AllocaInstPtr address = AllocaInst::New(type);

    _InsertInstruction(address);
    if ((node->LastChild()->Type() == SyntaxType::ST_INIT_VAL)
        || (node->LastChild()->Type() == SyntaxType::ST_CONST_INIT_VAL))
    {
        // with init value
        auto value = _ParseExpression(node->LastChild()->FirstChild());
        _InsertInstruction(StoreInst::New(value, address));
    }

    // Add the value to the symbol table and module.
    _AddValue(entry, address);

    return address;
}


AllocaInstPtr StandardAsmGenerator::_ParseArrayDef(SyntaxNodePtr node)
{
    TOMIC_PANIC("Not implemented yet");
    return nullptr;
}


ReturnInstPtr StandardAsmGenerator::_ParseReturnStatement(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node->Type() == SyntaxType::ST_RETURN_STMT);
    auto context = _module->Context();
    ReturnInstPtr inst;

    // Generate return value.
    auto exp = SemanticUtil::GetChildNode(node, SyntaxType::ST_EXP);
    if (exp == nullptr)
    {
        // return;
        inst = ReturnInst::New(context);
    }
    else
    {
        auto value = _ParseExpression(exp);

        // TODO: Error handling.
        TOMIC_ASSERT(value);

        inst = ReturnInst::New(context, value);
    }

    // Add the instruction to the current basic block.
    _InsertInstruction(inst);

    return inst;
}


void StandardAsmGenerator::_ParseAssignStatement(SyntaxNodePtr node)
{
    TOMIC_ASSERT(node->Type() == SyntaxType::ST_ASSIGNMENT_STMT);

    auto address = _GetLValValue(node->FirstChild());
    // Notice that the last child is semicolon.
    auto value = _ParseExpression(node->LastChild()->PrevSibling());

    _InsertInstruction(StoreInst::New(value, address));
}


void StandardAsmGenerator::_ParseInputStatement(SyntaxNodePtr node)
{
    auto value = InputInst::New(_module->Context());
    _InsertInstruction(value);

    auto address = _GetLValValue(node->FirstChild());
    _InsertInstruction(StoreInst::New(value, address));
}

/*
 * A helper function to split format string.
 * For example, it will split "Execute Order %d.\n" into:
 *   - "Execute Order "
 *   - "%d"
 *   - ".\n"
 *   - nullptr
 * WARNING: This function is not re-entrant.
 */

// Guess 1024 is big enough?
static char _formatBuffer[1024];
static const char* _SplitFormat(const char* format);

void StandardAsmGenerator::_ParseOutputStatement(SyntaxNodePtr node)
{
    auto context = _module->Context();
    const char* format = node->ChildAt(2)->Token()->lexeme.c_str();
    int paramNo = 0;

    const char* str = _SplitFormat(format);
    while (str)
    {
        if (StringUtil::Equals(str, "%d"))
        {
            auto exp = SemanticUtil::GetDirectChildNode(node, SyntaxType::ST_EXP, ++paramNo);
            auto value = _ParseExpression(exp);
            _InsertInstruction(OutputInst::New(value));
        }
        else
        {
            auto value = GlobalString::New(context, str);
            _module->AddGlobalString(value);
            _InsertInstruction(OutputInst::New(value));
        }

        str = _SplitFormat(nullptr);
    }
}

static const char* _SplitFormat(const char* format)
{
    static const char* _format = nullptr;
    if (format)
    {
        // Reset buffer, and clear leading '\"'.
        _format = format;
        while (*_format == '\"')
        {
            _format++;
        }
    }

    if (!_format || *_format == '\0')
    {
        return nullptr;
    }

    // Check if it is a format control.
    if (*_format == '%')
    {
        // We are sure '%' is not the last character.
        _formatBuffer[0] = *_format++;
        _formatBuffer[1] = *_format++;
        _formatBuffer[2] = '\0';
        return _formatBuffer;
    }

    int i = 0;
    while (*_format && (*_format != '%'))
    {
        if (*_format == '\"')
        {
            _format++;
            continue;
        }

        _formatBuffer[i++] = *_format++;
        TOMIC_ASSERT((i < 1024) && "Format buffer overflow!");
    }
    _formatBuffer[i] = '\0';

    return (i == 0) ? nullptr : _formatBuffer;
}

// node is an Exp or ConstExp.
ValuePtr StandardAsmGenerator::_ParseExpression(SyntaxNodePtr node)
{
    auto context = _module->Context();

    if (node->BoolAttribute("det"))
    {
        int value = node->IntAttribute("value");
        auto type = IntegerType::Get(context, 32);
        return ConstantData::New(type, value);
    }

    return _ParseAddExp(node->FirstChild());
}


ValuePtr StandardAsmGenerator::_ParseAddExp(SyntaxNodePtr node)
{
    auto context = _module->Context();

    if (node->BoolAttribute("det"))
    {
        int value = node->IntAttribute("value");
        auto type = IntegerType::Get(context, 32);
        return ConstantData::New(type, value);
    }

    if (node->HasManyChildren())
    {
        // AddExp + MulExp
        auto lhs = _ParseAddExp(node->FirstChild());
        auto op = node->ChildAt(1)->Token()->lexeme.c_str();
        auto rhs = _ParseMulExp(node->LastChild());
        switch (op[0])
        {
        case '+':
            return _InsertInstruction(BinaryOperator::New(BinaryOpType::Add, lhs, rhs));
        case '-':
            return _InsertInstruction(BinaryOperator::New(BinaryOpType::Sub, lhs, rhs));
        default:
            TOMIC_PANIC("Illegal operator");
        }
        return nullptr;
    }

    // MulExp
    return _ParseMulExp(node->FirstChild());
}


ValuePtr StandardAsmGenerator::_ParseMulExp(SyntaxNodePtr node)
{
    auto context = _module->Context();

    if (node->BoolAttribute("det"))
    {
        int value = node->IntAttribute("value");
        auto type = IntegerType::Get(context, 32);
        return ConstantData::New(type, value);
    }

    if (node->HasManyChildren())
    {
        // MulExp * UnaryExp
        auto lhs = _ParseMulExp(node->FirstChild());
        auto op = node->ChildAt(1)->Token()->lexeme.c_str();
        auto rhs = _ParseUnaryExp(node->LastChild());

        switch (op[0])
        {
        case '*':
            return _InsertInstruction(BinaryOperator::New(BinaryOpType::Mul, lhs, rhs));
        case '/':
            return _InsertInstruction(BinaryOperator::New(BinaryOpType::Div, lhs, rhs));
        case '%':
            return _InsertInstruction(BinaryOperator::New(BinaryOpType::Mod, lhs, rhs));
        default:
            TOMIC_PANIC("Illegal operator");
        }
        return nullptr;
    }

    // UnaryExp
    return _ParseUnaryExp(node->FirstChild());
}


ValuePtr StandardAsmGenerator::_ParseUnaryExp(SyntaxNodePtr node)
{
    if (node->FirstChild()->Type() == SyntaxType::ST_PRIMARY_EXP)
    {
        return _ParsePrimaryExp(node->FirstChild());
    }
    if (node->FirstChild()->Type() == SyntaxType::ST_FUNC_CALL)
    {
        return _ParseFunctionCall(node->FirstChild());
    }

    // UnaryOP UnaryExp
    switch (node->FirstChild()->Attribute("op")[0])
    {
    case '+':
        return _ParseUnaryExp(node->LastChild());
    case '-':
        return _InsertInstruction(UnaryOperator::New(UnaryOpType::Neg, _ParseUnaryExp(node->LastChild())));
    case '!':
        TOMIC_PANIC("Not implemented yet");
        return nullptr;
    default:
        TOMIC_PANIC("Illegal operator");
        return nullptr;
    }
}


ValuePtr StandardAsmGenerator::_ParsePrimaryExp(SyntaxNodePtr node)
{
    if (node->HasManyChildren())
    {
        // ( Exp )
        return _ParseExpression(node->ChildAt(1));
    }
    if (node->FirstChild()->Type() == SyntaxType::ST_LVAL)
    {
        return _ParseLVal(node->FirstChild());
    }
    if (node->FirstChild()->Type() == SyntaxType::ST_NUMBER)
    {
        return _ParseNumber(node->FirstChild());
    }

    TOMIC_PANIC("Illegal child type for PrimaryExp");

    return nullptr;
}


ValuePtr StandardAsmGenerator::_ParseFunctionCall(SyntaxNodePtr node)
{
    FunctionPtr function = _GetFunction(node);

    // Parameters.
    auto params = SemanticUtil::GetChildNode(node, SyntaxType::ST_FUNC_APARAMS);
    if (params && params->HasChildren())
    {
        std::vector<ValuePtr> parameters;
        for (auto it = params->FirstChild(); it; it = it->NextSibling())
        {
            if (it->Type() == SyntaxType::ST_FUNC_APARAM)
            {
                parameters.push_back(_ParseExpression(it->FirstChild()));
            }
        }
        return _InsertInstruction(CallInst::New(function, std::move(parameters)));
    }

    return _InsertInstruction(CallInst::New(function));
}


ValuePtr StandardAsmGenerator::_ParseLVal(SyntaxNodePtr node)
{
    // TODO: Add support for array!
    if (node->IntAttribute("dim") != 0)
    {
        TOMIC_PANIC("Not implemented yet");
        return nullptr;
    }

    return _InsertInstruction(LoadInst::New(_GetLValValue(node)));
}


ValuePtr StandardAsmGenerator::_ParseNumber(SyntaxNodePtr node)
{
    if (!node->BoolAttribute("det"))
    {
        TOMIC_PANIC("Number must be deterministic");
        return nullptr;
    }

    return ConstantData::New(_module->Context()->GetInt32Ty(), node->IntAttribute("value"));
}


TOMIC_LLVM_END
