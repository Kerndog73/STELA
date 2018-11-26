//
//  plain format.hpp
//  STELA
//
//  Created by Indi Kernick on 26/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#ifndef engine_plain_format_hpp
#define engine_plain_format_hpp

#include <string>
#include <iosfwd>
#include "format.hpp"

namespace stela {

std::string plainFormat(const fmt::Tokens &, uint32_t = 2);
void plainFormat(std::ostream &, const fmt::Tokens &, uint32_t = 2);

}

#endif
