//
//  compare params args.hpp
//  STELA
//
//  Created by Indi Kernick on 30/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_compare_params_args_hpp
#define stela_compare_params_args_hpp

#include "symbols.hpp"

namespace stela {

/// Argument types are convertible to parameter types
/// Used for calling a non-overloaded function
bool convParams(const sym::FuncParams &, const sym::FuncParams &);
/// Argument types are compatible with parameter types (Checks ValueCat).
/// Used for calling overloaded function
bool compatParams(const sym::FuncParams &, const sym::FuncParams &);
/// Argument types are the same as parameter types (ValueCat may be different).
/// Used for inserting functions
bool sameParams(const sym::FuncParams &, const sym::FuncParams &);

}

#endif
