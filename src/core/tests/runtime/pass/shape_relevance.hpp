// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "backend_visibility.hpp"
#include "ngraph/pass/pass.hpp"

namespace ngraph {
namespace pass {
class BACKEND_API ShapeRelevance : public FunctionPass {
public:
    ShapeRelevance() : FunctionPass() {}
    bool run_on_model(const std::shared_ptr<ngraph::Function>& m) override;
};
}  // namespace pass
}  // namespace ngraph
