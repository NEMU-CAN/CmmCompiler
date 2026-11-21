#pragma once

#include "AST.h"

#include <ostream>
#include <string>
#include <vector>

namespace cmm {

struct IRResult {
    bool success = false;
    std::string irText;                // 生成的人类可读 LLVM IR (.ll)
    std::vector<std::string> errors;   // 语义错误信息（如重复定义、未声明变量等）
};

// 成员 3 & 成员 4 协作实现（src/ir/）
class IRGenerator {
public:
    IRGenerator() = default;
    ~IRGenerator() = default;

    // 遍历 AST 并调用 third_party/compiler_ir 生成 .ll 中间代码
    IRResult generate(const ASTNode* root);

    // 便捷接口：直接输出到文件流/标准输出
    bool emitToStream(const ASTNode* root, std::ostream& os, std::vector<std::string>* errorsOut = nullptr);
};

}  // namespace cmm
