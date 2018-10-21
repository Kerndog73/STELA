//
//  modules.cpp
//  STELA
//
//  Created by Indi Kernick on 21/10/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "modules.hpp"

#include "log output.hpp"

using namespace stela;

namespace {

class Visitor {
public:
  Visitor(const ASTs &asts, LogBuf &buf)
    : order{},
      stack{},
      visited(asts.size(), false),
      asts{asts},
      log{buf, LogCat::semantic} {
    order.reserve(asts.size());
    stack.reserve(asts.size());
  }

  void checkCycle(const size_t index, const ast::Name &name) {
    for (auto s = stack.cbegin(); s != stack.cend(); ++s) {
      if (*s == index) {
        log.error() << "Cyclic dependencies detected in module \"" << name << "\"" << fatal;
      }
    }
  }

  void visit(const ast::Name &name) {
    for (size_t i = 0; i != asts.size(); ++i) {
      if (asts[i].name == name) {
        checkCycle(i, name);
        return visit(i);
      }
    }
    log.error() << "Module \"" << name << "\" not found" << fatal;
  }

  void visit(const size_t index) {
    if (visited[index]) {
      return;
    }
    visited[index] = true;
    stack.push_back(index);
    for (const ast::Name &dep : asts[index].imports) {
      visit(dep);
    }
    stack.pop_back();
    order.push_back(index);
  }
  
  void visit() {
    for (size_t i = 0; i != asts.size(); ++i) {
      visit(i);
    }
  }
  
  ModuleOrder order;

private:
  std::vector<size_t> stack;
  std::vector<bool> visited;
  const ASTs &asts;
  Log log;
};

}

ModuleOrder stela::findModuleOrder(const ASTs &asts, LogBuf &buf) {
  Visitor visitor{asts, buf};
  visitor.visit();
  return visitor.order;
}