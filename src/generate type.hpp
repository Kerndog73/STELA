//
//  generate type.hpp
//  STELA
//
//  Created by Indi Kernick on 30/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef stela_generate_type_hpp
#define stela_generate_type_hpp

#include "ast.hpp"
#include "gen context.hpp"

namespace stela {

gen::String generateType(gen::Ctx, ast::Type *);
gen::String generateTypeOrVoid(gen::Ctx, ast::Type *);
gen::String generateFuncSig(gen::Ctx, const ast::FuncType &);
gen::String generateFuncName(gen::Ctx, const ast::FuncType &);

ast::Type *concreteType(ast::Type *);

template <typename Concrete>
Concrete *concreteType(ast::Type *type) {
  return dynamic_cast<Concrete *>(concreteType(type));
}

}

#endif
