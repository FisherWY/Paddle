// Copyright (c) 2021 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <absl/container/flat_hash_map.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "paddle/cinn/auto_schedule/tuning.h"
#include "paddle/cinn/backends/compiler.h"
#include "paddle/cinn/backends/cuda_util.h"
#include "paddle/cinn/common/macros.h"
#include "paddle/cinn/hlir/framework/graph.h"
#include "paddle/cinn/hlir/framework/instruction.h"
#include "paddle/cinn/hlir/framework/op_strategy.h"
#include "paddle/cinn/hlir/framework/parallel_compiler.h"
#include "paddle/cinn/hlir/framework/program.h"
#include "paddle/cinn/hlir/framework/scope.h"
#include "paddle/cinn/ir/lowered_func.h"
#include "paddle/cinn/lang/packed_func.h"
#include "paddle/cinn/utils/timer.h"

namespace cinn {
namespace hlir {
namespace framework {

/**
 * GraphCompiler compiles a graph and generate the runtime Program.
 */
class GraphCompiler final {
 public:
  struct CompilationResult {
    std::unique_ptr<Program> runtime_program;
    std::vector<std::vector<ir::LoweredFunc>> lowered_funcs;
    std::vector<std::string> source_codes;
    std::vector<std::string> source_ptxs;
    std::vector<std::unique_ptr<Instruction>> instructions;
  };

  struct CompilationContext {
    CompilationContext() {}
    CompilationContext(const std::shared_ptr<Graph>& graph,
                       const std::shared_ptr<Scope>& scope,
                       Target target)
        : graph(graph), scope(scope), target(std::move(target)) {}
    // Code to be compiled
    std::string attached_code = "";
    // Compile options
    bool with_instantiate_variables = false;
    bool with_buffer_handle_instruction_inserted = false;
    bool remove_unused_variables = true;
    // Compile stage
    ParallelCompiler::Stage stage = ParallelCompiler::Stage::DEFAULT;
    // Compile target
    Target target;
    // Computation graph
    std::shared_ptr<Graph> graph;
    // Variable scope
    std::shared_ptr<Scope> scope;
    // fetch var ids in cinn and the corresponding var nodes will not be fused
    // so as to get the result
    std::unordered_set<std::string> fetch_var_ids;
    // map dst reuse var to the src var sharing buffer
    absl::flat_hash_map<std::string, std::string> reuse_vars_map;
    // nodes group, it may come from the result of op fusion or graph tuning.
    // nodes in a group will be built into an Instruction
    std::vector<std::shared_ptr<Graph::Group>> groups;
    // corresponding LoweredFuncs of above grouped nodes,
    // if it is empty then graph_compiler will generate for them
    std::vector<std::vector<ir::LoweredFunc>> lowered_funcs;

    // apply results of auto-tune to compile
    void Apply(const auto_schedule::TuningResult& tuning_result);
  };

  GraphCompiler(CompilationContext context) : compilation_context_(context) {}

  // Compile with a packing option and result, to be extended easily.
  CompilationResult Build(CompilationContext* context);

  std::unique_ptr<Program> Build(const std::string& code = "");

  CompilationResult Lowering();

  CompilationResult CodegenAndJit();

  CompilationResult BuildInstruction();

  const std::shared_ptr<Scope>& GetScope() const {
    return compilation_context_.scope;
  }

  CompilationContext& GetCompilationContext() { return compilation_context_; }

  void SetCompilationContext(const CompilationContext& context) {
    compilation_context_ = context;
  }

 private:
  // instantiate all variables on compile time
  void InstantiateVariables(CompilationContext* context);

  // some variables are eliminated by optimized passes(such as OpFusion),
  // we can filter out them according to arguments of the built instructions,
  // and erase them from the scope to avoid unnecessary buffer allocation
  void RemoveInvalidVariables(
      CompilationContext* context,
      const std::vector<std::unique_ptr<Instruction>>& instructions);

  // find the first and last instruction where a variable used, and mark the
  // variable should allocate buffer before the first instruction runing and
  // can release the buffer after the last instruction finished.
  void AnalyzeVariableLifeTime(
      const std::vector<std::unique_ptr<Instruction>>& instructions,
      std::unordered_map<int, std::vector<std::string>>* step2malloc,
      std::unordered_map<int, std::vector<std::string>>* step2free);

  // insert a buffer malloc instruction applying on variables before they are
  // firstly used in the next instruction, and insert a buffer free instruction
  // applying on variables after no instruction will use them anymore
  void InsertBufferHandlers(
      CompilationContext* context,
      std::vector<std::unique_ptr<Instruction>>* instructions);

 private:
  // parallel compiler
  std::shared_ptr<ParallelCompiler> parallel_compiler_;

  CompilationContext compilation_context_;

  CINN_DISALLOW_COPY_AND_ASSIGN(GraphCompiler);
};

std::shared_ptr<Scope> BuildScope(Target target,
                                  const std::shared_ptr<Graph>& graph,
                                  std::shared_ptr<Scope> scope = nullptr);

// Given params, lower the op to LoweredFunc using new IR Schedule
std::vector<ir::LoweredFunc> GetFuncFromImpl(
    const std::shared_ptr<OpImpl>& impl,
    const common::CINNValuePack& cinn_inputs,
    std::vector<ir::Tensor>& tensor_inputs,  // NOLINT
    const std::vector<std::string>& input_output_nodes,
    const std::string& node_id,
    const Target& target);

}  // namespace framework
}  // namespace hlir
}  // namespace cinn
