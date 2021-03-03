/*
 * udf_library.cc
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
 * udf_library.cc
 *
 * Author: chenjing
 * Date: 2019/11/26
 *--------------------------------------------------------------------------
 **/
#include "udf/udf_library.h"

#include <unordered_set>
#include <vector>
#include "boost/algorithm/string/case_conv.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/string_file.hpp"

#include "codegen/type_ir_builder.h"
#include "parser/parser.h"
#include "plan/planner.h"
#include "udf/udf_registry.h"
#include "vm/jit_wrapper.h"

using ::fesql::common::kCodegenError;

namespace fesql {
namespace udf {

std::string UDFLibrary::GetCanonicalName(const std::string& name) const {
    std::string canonical_name = name;
    if (!case_sensitive_) {
        boost::to_lower(canonical_name);
    }
    return canonical_name;
}

std::shared_ptr<UDFRegistry> UDFLibrary::Find(
    const std::string& name,
    const std::vector<const node::TypeNode*>& arg_types) const {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    if (iter == table_.end()) {
        return nullptr;
    }
    auto& signature_table = iter->second->signature_table;
    std::shared_ptr<UDFRegistry> registry = nullptr;
    std::string signature;
    int variadic_pos = -1;
    auto status =
        signature_table.Find(arg_types, &registry, &signature, &variadic_pos);
    return registry;
}

bool UDFLibrary::HasFunction(const std::string& name) const {
    std::string canonical_name = GetCanonicalName(name);
    return table_.find(canonical_name) != table_.end();
}

void UDFLibrary::InsertRegistry(
    const std::string& name,
    const std::vector<const node::TypeNode*>& arg_types, bool is_variadic,
    bool always_return_list,
    const std::unordered_set<size_t>& always_list_argidx,
    std::shared_ptr<UDFRegistry> registry) {
    std::string canonical_name = GetCanonicalName(name);
    std::shared_ptr<UDFLibraryEntry> entry = nullptr;
    auto iter = table_.find(canonical_name);
    if (iter == table_.end()) {
        entry = std::make_shared<UDFLibraryEntry>();
        table_.insert(iter, {canonical_name, entry});
    } else {
        entry = iter->second;
    }
    // set return list property
    if (always_return_list) {
        if (entry->signature_table.GetTable().size() == 0) {
            entry->always_return_list = true;
        }
    } else {
        entry->always_return_list = false;
    }

    // set list typed arg property
    auto& is_list_dict = entry->arg_is_always_list;
    for (size_t i : always_list_argidx) {
        is_list_dict[i] = true;
    }
    for (size_t i = 0; i < arg_types.size(); ++i) {
        if (arg_types[i] != nullptr && arg_types[i]->base() == node::kList) {
            auto iter = is_list_dict.find(i);
            if (iter == is_list_dict.end()) {
                is_list_dict.insert(iter, {i, true});
            }
        } else {
            if (always_list_argidx.find(i) != always_list_argidx.end()) {
                if (arg_types[i] == nullptr) {
                    continue;  // AnyArg
                }
                LOG(WARNING) << "Override argument position " << i
                             << " to be not always list";
            }
            is_list_dict[i] = false;
        }
    }

    // insert argument signature
    auto status =
        entry->signature_table.Register(arg_types, is_variadic, registry);
    if (!status.isOK()) {
        LOG(WARNING) << "Insert " << name << " registry failed: " << status;
    }
}

bool UDFLibrary::IsUDAF(const std::string& name, size_t args) const {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    if (iter == table_.end()) {
        return false;
    }
    auto& arg_num_set = iter->second->udaf_arg_nums;
    return arg_num_set.find(args) != arg_num_set.end();
}

void UDFLibrary::SetIsUDAF(const std::string& name, size_t args) {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    if (iter == table_.end()) {
        LOG(WARNING) << canonical_name
                     << " is not registered, can not set as udaf";
        return;
    }
    iter->second->udaf_arg_nums.insert(args);
}

bool UDFLibrary::RequireListAt(const std::string& name, size_t index) const {
    std::string canonical_name = GetCanonicalName(name);
    auto entry_iter = table_.find(canonical_name);
    if (entry_iter == table_.end()) {
        return false;
    }
    auto& is_list_dict = entry_iter->second->arg_is_always_list;
    auto iter = is_list_dict.find(index);
    return iter != is_list_dict.end() && iter->second;
}

bool UDFLibrary::IsListReturn(const std::string& name) const {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    if (iter == table_.end()) {
        return false;
    }
    return iter->second->always_return_list;
}

ExprUDFRegistryHelper UDFLibrary::RegisterExprUDF(const std::string& name) {
    return ExprUDFRegistryHelper(GetCanonicalName(name), this);
}

LLVMUDFRegistryHelper UDFLibrary::RegisterCodeGenUDF(const std::string& name) {
    return LLVMUDFRegistryHelper(GetCanonicalName(name), this);
}

ExternalFuncRegistryHelper UDFLibrary::RegisterExternal(
    const std::string& name) {
    return ExternalFuncRegistryHelper(GetCanonicalName(name), this);
}

UDAFRegistryHelper UDFLibrary::RegisterUDAF(const std::string& name) {
    return UDAFRegistryHelper(GetCanonicalName(name), this);
}

Status UDFLibrary::RegisterAlias(const std::string& alias,
                                 const std::string& name) {
    std::string canonical_name = GetCanonicalName(name);
    std::string canonical_alias = GetCanonicalName(alias);
    auto iter = table_.find(canonical_alias);
    CHECK_TRUE(iter == table_.end(), kCodegenError, "Function name '",
               canonical_alias, "' is duplicated");
    iter = table_.find(canonical_name);
    CHECK_TRUE(iter != table_.end(), kCodegenError,
               "Alias target Function name '", canonical_name, "' not found");
    table_[canonical_alias] = iter->second;
    return Status::OK();
}

Status UDFLibrary::RegisterFromFile(const std::string& path_str) {
    boost::filesystem::path path(path_str);
    std::string script;
    boost::filesystem::load_string_file(path, script);
    DLOG(INFO) << "Script file : " << script << "\n" << script;

    ::fesql::node::NodePointVector parser_trees;
    ::fesql::parser::FeSQLParser parser;
    ::fesql::plan::SimplePlanner planer(node_manager());
    ::fesql::node::PlanNodeList plan_trees;

    Status status;
    CHECK_TRUE(0 == parser.parse(script, parser_trees, node_manager(), status),
               kCodegenError, "Fail to parse script:", status.str());
    CHECK_TRUE(0 == planer.CreatePlanTree(parser_trees, plan_trees, status),
               kCodegenError, "Fail to create sql plan: ", status.str());

    std::unordered_map<std::string, std::shared_ptr<SimpleUDFRegistry>> dict;
    auto it = plan_trees.begin();
    for (; it != plan_trees.end(); ++it) {
        const ::fesql::node::PlanNode* node = *it;
        CHECK_TRUE(node != nullptr, kCodegenError, "Compile null plan");
        switch (node->GetType()) {
            case ::fesql::node::kPlanTypeFuncDef: {
                auto func_def_plan =
                    dynamic_cast<const ::fesql::node::FuncDefPlanNode*>(node);
                CHECK_TRUE(func_def_plan->fn_def_ != nullptr, kCodegenError,
                           "fn_def node is null");

                auto header = func_def_plan->fn_def_->header_;
                auto def_node = dynamic_cast<node::UDFDefNode*>(
                    node_manager()->MakeUDFDefNode(func_def_plan->fn_def_));
                auto registry = std::make_shared<SimpleUDFRegistry>(
                    header->name_, def_node);

                std::vector<const node::TypeNode*> arg_types;
                for (size_t i = 0; i < def_node->GetArgSize(); ++i) {
                    arg_types.push_back(def_node->GetArgType(i));
                }
                InsertRegistry(header->name_, arg_types, false,
                               def_node->GetReturnType()->base() == node::kList,
                               {}, registry);
                break;
            }
            default:
                return Status(
                    common::kCodegenError,
                    "fail to codegen fe script: unrecognized plan type " +
                        node::NameOfPlanNodeType(node->GetType()));
        }
    }
    return Status::OK();
}

Status UDFLibrary::Transform(const std::string& name,
                             const std::vector<node::ExprNode*>& args,
                             node::NodeManager* node_manager,
                             ExprNode** result) const {
    UDFResolveContext ctx(args, node_manager, this);
    return this->Transform(name, &ctx, result);
}

Status UDFLibrary::Transform(const std::string& name, UDFResolveContext* ctx,
                             ExprNode** result) const {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    CHECK_TRUE(iter != table_.end(), kCodegenError,
               "Fail to find registered function: ", canonical_name);
    auto& signature_table = iter->second->signature_table;

    std::shared_ptr<UDFRegistry> registry = nullptr;
    std::string signature;
    int variadic_pos = -1;
    CHECK_STATUS(
        signature_table.Find(ctx, &registry, &signature, &variadic_pos),
        "Fail to find matching argument signature for ", canonical_name, ": <",
        ctx->GetArgSignature(), ">");

    DLOG(INFO) << "Resolve '" << canonical_name << "'<"
               << ctx->GetArgSignature() << ">to " << canonical_name << "("
               << signature << ")";
    CHECK_TRUE(registry != nullptr, kCodegenError);
    return registry->Transform(ctx, result);
}

Status UDFLibrary::ResolveFunction(const std::string& name,
                                   UDFResolveContext* ctx,
                                   node::FnDefNode** result) const {
    std::string canonical_name = GetCanonicalName(name);
    auto iter = table_.find(canonical_name);
    CHECK_TRUE(iter != table_.end(), kCodegenError,
               "Fail to find registered function: ", canonical_name);
    auto& signature_table = iter->second->signature_table;

    std::shared_ptr<UDFRegistry> registry = nullptr;
    std::string signature;
    int variadic_pos = -1;
    CHECK_STATUS(
        signature_table.Find(ctx, &registry, &signature, &variadic_pos),
        "Fail to find matching argument signature for ", canonical_name, ": <",
        ctx->GetArgSignature(), ">");

    DLOG(INFO) << "Resolve '" << canonical_name << "'<"
               << ctx->GetArgSignature() << ">to " << canonical_name << "("
               << signature << ")";
    CHECK_TRUE(registry != nullptr, kCodegenError);
    return registry->ResolveFunction(ctx, result);
}

Status UDFLibrary::ResolveFunction(const std::string& name,
                                   const std::vector<node::ExprNode*>& args,
                                   node::NodeManager* node_manager,
                                   node::FnDefNode** result) const {
    UDFResolveContext ctx(args, node_manager, this);
    return this->ResolveFunction(name, &ctx, result);
}

void UDFLibrary::AddExternalFunction(const std::string& name, void* addr) {
    external_symbols_.insert(std::make_pair(name, addr));
}

void UDFLibrary::InitJITSymbols(vm::FeSQLJITWrapper* jit_ptr) {
    for (auto& pair : external_symbols_) {
        jit_ptr->AddExternalFunction(pair.first, pair.second);
    }
}

const std::string GetArgSignature(const std::vector<node::ExprNode*>& args) {
    std::stringstream ss;
    for (size_t i = 0; i < args.size(); ++i) {
        auto arg = args[i];
        if (arg == nullptr) {
            ss << "?";
        } else {
            if (arg->nullable()) {
                ss << "nullable ";
            }
            if (arg->GetOutputType() != nullptr) {
                ss << arg->GetOutputType()->GetName();
            } else {
                ss << "?";
            }
        }
        if (i < args.size() - 1) {
            ss << ", ";
        }
    }
    return ss.str();
}

}  // namespace udf
}  // namespace fesql
