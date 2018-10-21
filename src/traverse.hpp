//
//  traverse.hpp
//  STELA
//
//  Created by Indi Kernick on 10/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_traverse_hpp
#define stela_traverse_hpp

#include "ast.hpp"
#include "symbols.hpp"
#include "log output.hpp"
#include "scope manager.hpp"

namespace stela {

void traverse(ScopeMan, sym::Module &, Symbols &, Log &);

}

#endif
