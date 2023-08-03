// Copyright (c) 2023 CINN Authors. All Rights Reserved.
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

#include "paddle/cinn/hlir/pass/general_fusion_merge_pass/fuse_pass.h"

namespace cinn {
namespace hlir {
namespace pass {

class InputFusePassCtx;

class InputFusePass : public FusePass {
 public:
  virtual ~InputFusePass() = default;

  virtual void operator()(InputFusePassCtx* ctx) const = 0;

  const std::string FuseMode() const final { return "InputFuse"; }

  virtual int Benefit() const = 0;

 protected:
  InputFusePass() = default;
};

}  // namespace pass
}  // namespace hlir
}  // namespace cinn
