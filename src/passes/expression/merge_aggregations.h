/*
 * merge_aggregations.h
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
 * merge_aggregations.h
 *--------------------------------------------------------------------------
 **/
#ifndef SRC_PASSES_EXPRESSION_MERGE_AGGREGATIONS_H_
#define SRC_PASSES_EXPRESSION_MERGE_AGGREGATIONS_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "passes/expression/expr_pass.h"

namespace fesql {
namespace passes {

using base::Status;
using node::ExprAnalysisContext;
using node::ExprNode;

class MergeAggregations : public passes::ExprPass {
 public:
    Status Apply(ExprAnalysisContext* ctx, ExprNode* expr,
                 ExprNode** out) override;
};

}  // namespace passes
}  // namespace fesql
#endif  // SRC_PASSES_EXPRESSION_MERGE_AGGREGATIONS_H_
