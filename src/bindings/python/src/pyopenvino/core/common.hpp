// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <ie_parameter.hpp>
#include <ie_plugin_config.hpp>
#include <openvino/core/type/element_type.hpp>
#include <string>

#include "Python.h"
#include "ie_common.h"
#include "openvino/runtime/executable_network.hpp"
#include "openvino/runtime/infer_request.hpp"
#include "openvino/runtime/tensor.hpp"
#include "pyopenvino/core/containers.hpp"
#include "pyopenvino/graph/any.hpp"

namespace py = pybind11;

namespace Common {
const std::map<ov::element::Type, py::dtype>& ov_type_to_dtype();

const std::map<py::str, ov::element::Type>& dtype_to_ov_type();

ov::runtime::Tensor tensor_from_numpy(py::array& array, bool shared_memory);

py::array as_contiguous(py::array& array, ov::element::Type type);

const ov::runtime::Tensor& cast_to_tensor(const py::handle& tensor);

const Containers::TensorNameMap cast_to_tensor_name_map(const py::dict& inputs);

const Containers::TensorIndexMap cast_to_tensor_index_map(const py::dict& inputs);

void set_request_tensors(ov::runtime::InferRequest& request, const py::dict& inputs);

PyAny from_ov_any(const ov::Any& any);

uint32_t get_optimal_number_of_requests(const ov::runtime::ExecutableNetwork& actual);

py::dict outputs_to_dict(const std::vector<ov::Output<const ov::Node>>& outputs, ov::runtime::InferRequest& request);

// Use only with classes that are not creatable by users on Python's side, because
// Objects created in Python that are wrapped with such wrapper will cause memory leaks.
template <typename T>
class ref_wrapper {
    std::reference_wrapper<T> impl;

public:
    explicit ref_wrapper(T* p) : impl(*p) {}
    T* get() const {
        return &impl.get();
    }
};
};  // namespace Common
