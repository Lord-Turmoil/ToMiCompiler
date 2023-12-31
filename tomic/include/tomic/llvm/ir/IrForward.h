/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_IR_FORWARD_H_
#define _TOMIC_IR_FORWARD_H_

#include <memory>
#include <tomic/llvm/Llvm.h>
#include <vector>

TOMIC_LLVM_BEGIN

class LlvmContext;
using LlvmContextPtr = LlvmContext*;

class Module;
using ModulePtr = Module*;
using ModuleSmartPtr = std::shared_ptr<Module>;


////////////////////////////////////////////////////////////////////////////////
// Type Forward Declaration
class Type;
using TypePtr = Type*;


////////////////////////////////////////////////////////////////////////////////
// Value Forward Declaration
class Value;
using ValuePtr = Value*;
using ValueSmartPtr = std::shared_ptr<Value>;

class Argument;
using ArgumentPtr = Argument*;

class Function;
using FunctionPtr = Function*;

class BasicBlock;
using BasicBlockPtr = BasicBlock*;

class User;
using UserPtr = User*;


////////////////////////////////////////////////////////////////////////////////
// Constant Forward Declaration
class Constant;
using ConstantPtr = Constant*;

class ConstantData;
using ConstantDataPtr = ConstantData*;

class GlobalValue;
using GlobalValuePtr = GlobalValue*;

class GlobalVariable;
using GlobalVariablePtr = GlobalVariable*;

class GlobalString;
using GlobalStringPtr = GlobalString*;

class Function;
using FunctionPtr = Function*;


////////////////////////////////////////////////////////////////////////////////
// Instruction Forward Declaration
class Instruction;
using InstructionPtr = Instruction*;

class UnaryInstruction;
using UnaryInstructionPtr = UnaryInstruction*;

class UnaryOperator;
using UnaryOperatorPtr = UnaryOperator*;

class BinaryOperator;
using BinaryOperatorPtr = BinaryOperator*;

class CompareInstruction;
using CompareInstructionPtr = CompareInstruction*;

class AllocaInst;
using AllocaInstPtr = AllocaInst*;

class LoadInst;
using LoadInstPtr = LoadInst*;

class StoreInst;
using StoreInstPtr = StoreInst*;

class CallInst;
using CallInstPtr = CallInst*;

class ReturnInst;
using ReturnInstPtr = ReturnInst*;

class InputInst;
using InputInstPtr = InputInst*;

class OutputInst;
using OutputInstPtr = OutputInst*;

////////////////////////////////////////////////////////////////////////////////
// Use Forward Declaration
class Use;
using UsePtr = Use*;
using UseSmartPtr = std::shared_ptr<Use>;
using UseList = std::vector<UsePtr>;
using UseListPtr = UseList*;

TOMIC_LLVM_END

#endif // _TOMIC_IR_FORWARD_H_
