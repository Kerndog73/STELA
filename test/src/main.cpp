//
//  main.cpp
//  Test
//
//  Created by Indi Kernick on 17/6/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "macros.hpp"

#include "lexer.hpp"
#include "syntax.hpp"
#include "format.hpp"
#include "semantics.hpp"
#include "generation.hpp"

#include <iostream>
#include <memory>

/*
#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

void foo() {
  std::cout << "foo()\n";
}
*/

int main() {
  /*llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  
  llvm::LLVMContext context;
  auto module = std::make_unique<llvm::Module>("top", context);
  llvm::IRBuilder<> builder{context};
  
  // a function that accepts no parameters and returns void
  auto footype = llvm::FunctionType::get(builder.getVoidTy(), false);
  // create a foo function
  // extern linkage so that it can be called from the module
  auto foofunc = llvm::Function::Create(footype, llvm::Function::ExternalLinkage, "foo", module.get());
  // make the foo symbol available via dynamic symbol lookup
  llvm::sys::DynamicLibrary::AddSymbol("foo", reinterpret_cast<void *>(foo));
  
  // a function that accepts no parameters and returns int32
  auto functype = llvm::FunctionType::get(builder.getInt32Ty(), false);
  // create a main function
  // extern linkage so that it can be called from this program
  auto mainfunc = llvm::Function::Create(functype, llvm::Function::ExternalLinkage, "main", module.get());
  // make the main symbol available via dynamic symbol lookup
  //llvm::sys::DynamicLibrary::AddSymbol("main", mainfunc);
  
  // create a block at the beginning of the main function
  auto entryblock = llvm::BasicBlock::Create(context, "entry", mainfunc);
  // instructions shall be appended to the end of the entry block
  builder.SetInsertPoint(entryblock);
  // create a call instruction that calls foo
  builder.CreateCall(foofunc);
  // create a ret instruction that returns 0
  builder.CreateRet(llvm::ConstantInt::get(builder.getInt32Ty(), 0));
  
  // check that the module is valid
  std::string error;
  // wrap the error string in a stream
  llvm::raw_string_ostream error_os(error);
  if (llvm::verifyModule(*module, &error_os)) {
    std::cerr << "Module Error: " << error << '\n';
    module->print(llvm::errs(), nullptr);
    return 1;
  }
  
  // build an execution engine
  auto engine = llvm::EngineBuilder(std::move(module))
                .setErrorStr(&error)
                .setOptLevel(llvm::CodeGenOpt::Aggressive)
                .setEngineKind(llvm::EngineKind::JIT)
                .create();
  
  // check that the engine was created
  if (!engine) {
    std::cerr << "Execution engine error: " << error << '\n';
    return 1;
  }
  
  // generate code
  engine->finalizeObject();
  // lookup function address
  using Function = void (*)();
  Function f = reinterpret_cast<Function>(engine->getFunctionAddress("main"));
  if (f) {
    f();
  } else {
    std::cerr << "Error getting function\n";
  }
  */

  int failures = 0;
  failures += testFormat();
  failures += testLexer();
  failures += testSyntax();
  failures += testSemantics();
  failures += testGeneration();
  if (failures == 0) {
    std::cout << "ALL PASSED!\n";
  } else if (failures == 1) {
    std::cout << "1 TEST FAILED!\n";
  } else {
    std::cout << failures << " TESTS FAILED!\n";
  }
  if (failures > 255) {
    return 255;
  } else {
    return failures;
  }
}
