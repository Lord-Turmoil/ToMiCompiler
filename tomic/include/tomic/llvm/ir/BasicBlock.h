/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_LLVM_BASIC_BLOCK_H_
#define _TOMIC_LLVM_BASIC_BLOCK_H_

#include <tomic/llvm/Llvm.h>
#include <tomic/llvm/ir/Value.h>
#include <tomic/llvm/ir/IrForward.h>

TOMIC_LLVM_BEGIN

class BasicBlock final : public Value
{
public:
    ~BasicBlock() override = default;

private:
    BasicBlock();
};

TOMIC_LLVM_END

#endif // _TOMIC_LLVM_BASIC_BLOCK_H_
