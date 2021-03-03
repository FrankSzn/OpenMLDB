/*
 * expr_pass.h
 * Copyright 2021 4Paradigm
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * expr_pass.h
 *--------------------------------------------------------------------------
 **/
#ifndef SRC_PASSES_EXPRESSION_EXPR_PASS_H_
#define SRC_PASSES_EXPRESSION_EXPR_PASS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "node/expr_node.h"
#include "node/sql_node.h"
#include "passes/pass_base.h"

namespace fesql {
namespace passes {

class ExprPass
    : public passes::PassBase<node::ExprNode, node::ExprAnalysisContext> {
 public:
    ExprPass() = default;
    virtual ~ExprPass() {}

    node::ExprIdNode* GetWindow() const;
    void SetWindow(node::ExprIdNode*);

    node::ExprIdNode* GetRow() const;
    void SetRow(node::ExprIdNode*);

 private:
    node::ExprIdNode* window_;
    node::ExprIdNode* row_;
};

class ExprPassGroup : public ExprPass {
 public:
    void AddPass(const std::shared_ptr<ExprPass>& pass);
    base::Status Apply(node::ExprAnalysisContext* ctx, node::ExprNode* expr,
                       node::ExprNode** out) override;

 private:
    std::vector<std::shared_ptr<ExprPass>> passes_;
};

/**
 * Utility class to replace child expr in root expression tree inplace.
 */
class ExprReplacer {
 public:
    void AddReplacement(const node::ExprIdNode* arg, node::ExprNode* repl);
    void AddReplacement(const node::ExprNode* expr, node::ExprNode* repl);
    void AddReplacement(size_t column_id, node::ExprNode* repl);
    void AddReplacement(const std::string& relation_name,
                        const std::string& column_name, node::ExprNode* repl);

    fesql::base::Status Replace(node::ExprNode* root,
                                node::ExprNode** output) const;

 private:
    fesql::base::Status DoReplace(node::ExprNode* root,
                                  std::unordered_set<size_t>* visited,
                                  node::ExprNode** output) const;

    std::unordered_map<size_t, node::ExprNode*> arg_id_map_;
    std::unordered_map<size_t, node::ExprNode*> node_id_map_;
    std::unordered_map<size_t, node::ExprNode*> column_id_map_;
    std::unordered_map<std::string, node::ExprNode*> column_name_map_;
};

}  // namespace passes
}  // namespace fesql
#endif  // SRC_PASSES_EXPRESSION_EXPR_PASS_H_
