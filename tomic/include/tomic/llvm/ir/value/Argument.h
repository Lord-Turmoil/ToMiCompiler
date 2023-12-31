/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_LLVM_ARGUMENT_H_
#define _TOMIC_LLVM_ARGUMENT_H_

#include <tomic/llvm/ir/IrForward.h>
#include <tomic/llvm/ir/value/Value.h>
#include <tomic/llvm/Llvm.h>

TOMIC_LLVM_BEGIN

class Argument final : public Value
{
public:
    ~Argument() override = default;

    static ArgumentPtr New(TypePtr type, const std::string& name, int argNo);

    static bool classof(const ValueType type) { return type == ValueType::ArgumentTy; }

    void PrintAsm(IAsmWriterPtr writer) override;
    void PrintUse(IAsmWriterPtr writer) override;

    bool IsArgument() const override { return true; }

    void SetParent(FunctionPtr parent) { _parent = parent; }
    FunctionPtr Parent() const { return _parent; }
    int ArgNo() const { return _argNo; }

private:
    Argument(TypePtr type, const std::string& name, int argNo);

    FunctionPtr _parent;
    int _argNo;
};


TOMIC_LLVM_END

#endif // _TOMIC_LLVM_ARGUMENT_H_
