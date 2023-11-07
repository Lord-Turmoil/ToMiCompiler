/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include <tomic/llvm/ir/value/Value.h>
#include <tomic/llvm/ir/Type.h>
#include <tomic/llvm/ir/LlvmContext.h>

TOMIC_LLVM_BEGIN

LlvmContextPtr Value::Context() const
{
    return _type->Context();
}

TOMIC_LLVM_END