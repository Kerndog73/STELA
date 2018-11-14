//
//  compare params args.cpp
//  STELA
//
//  Created by Indi Kernick on 30/7/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "compare params args.hpp"

#include "compare types.hpp"
#include <Simpleton/Utils/algorithm.hpp>

using namespace stela;

bool stela::compatParams(
  const NameLookup &lkp,
  const sym::FuncParams &params,
  const sym::FuncParams &args
) {
  return Utils::equal_size(params, args, [&lkp] (sym::ExprType param, sym::ExprType arg) {
    return compareTypes(lkp, param.type, arg.type) && sym::callMutRef(param, arg);
  });
}

bool stela::sameParams(
  const NameLookup &lkp,
  const sym::FuncParams &params,
  const sym::FuncParams &args
) {
  return Utils::equal_size(params, args, [&lkp] (sym::ExprType param, sym::ExprType arg) {
    return compareTypes(lkp, param.type, arg.type);
  });
}
