//
//  compare types.hpp
//  STELA
//
//  Created by Indi Kernick on 5/9/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_compare_types_hpp
#define stela_compare_types_hpp

#include "ast.hpp"
#include "scope lookup.hpp"

namespace stela {

bool compareTypes(const NameLookup &, ast::Type *, ast::Type *);

}

#endif
