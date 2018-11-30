//
//  generation.cpp
//  STELA
//
//  Created by Indi Kernick on 29/11/18.
//  Copyright © 2018 Indi Kernick. All rights reserved.
//

#include "generation.hpp"

#include <fstream>
#include <iostream>
#include "macros.hpp"
#include <STELA/code generation.hpp>
#include <STELA/syntax analysis.hpp>
#include <STELA/semantic analysis.hpp>

using namespace stela;

namespace {

Symbols createSym(const std::string_view source, LogBuf &log) {
  AST ast = createAST(source, log);
  Symbols syms = initModules(log);
  compileModule(syms, ast, log);
  return syms;
}

std::string generateCpp(const std::string_view source, LogBuf &log) {
  return generateCpp(createSym(source, log), log);
}

bool validCode(const std::string &cpp) {
  const std::string cppFileName = "temporary_source.cpp";
  const std::string errFileName = "temporary_error.txt";
  const std::string exeFileName = "temporary_executable";
  std::ofstream cppFile(cppFileName);
  if (!cppFile.is_open()) {
    std::cout << "Could not open temporary source file\n";
    return false;
  }
  cppFile << cpp;
  cppFile.close();
  std::string command = "$CXX -Wno-return-type -std=c++17 ";
  command.append(cppFileName);
  command.append(" -o ");
  command.append(exeFileName);
  command.append(" 2> ");
  command.append(errFileName);
  system(command.c_str());
  std::ifstream errFile(errFileName);
  if (!errFile.is_open()) {
    std::cout << "Could not open temporary error file\n";
    return false;
  }
  errFile.seekg(0, std::ios::end);
  std::string errMsg(static_cast<size_t>(errFile.tellg()), '\0');
  errFile.seekg(0, std::ios::beg);
  errFile.read(errMsg.data(), static_cast<std::streamsize>(errMsg.size()));
  if (errMsg.size() > 1) {
    std::cout << errMsg << '\n';
    return false;
  }
  return true;
}

bool generate(const std::string_view source, LogBuf &log) {
  return validCode(generateCpp(source, log));
}

}

#define ASSERT_COMPILES(SOURCE)                                                 \
  ASSERT_TRUE(generate(SOURCE, log))
#define ASSERT_STELA_FAILS(SOURCE)                                              \
  ASSERT_THROWS(generate(SOURCE, log), FatalError)
#define ASSERT_CPP_FAILS(SOURCE)                                                \
  ASSERT_FALSE(generate(SOURCE, log))

TEST_GROUP(Generation, {
  stela::StreamLog log;
  log.pri(LogPri::status);
  
  TEST(Empty source, {
    ASSERT_COMPILES("");
  });
  
  TEST(Functions, {
    ASSERT_COMPILES(R"(
      func first() {}
      
      func second() -> sint {
        return 1;
      }
      
      func third(a: real) {}
      
      func fourth(a: bool, b: uint) {}
      
      func fifth(a: real, b: inout byte, c: sint) {}
      
      func (self: inout [struct {v: [sint];}]) sixth(a: inout sint) {}
    )");
  });
});

#undef ASSERT_CPP_FAILS
#undef ASSERT_STELA_FAILS
#undef ASSERT_COMPILES
