//
//  optimize module.hpp
//  STELA
//
//  Created by Indi Kernick on 12/12/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_optimize_module_hpp
#define stela_optimize_module_hpp

#include "code generation.hpp"

namespace llvm {

class TargetMachine;
class Module;

}

namespace stela {

void optimizeModule(llvm::TargetMachine *, llvm::Module *, OptFlags);

}

#endif
