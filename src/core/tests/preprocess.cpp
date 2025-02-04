// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "ngraph/ops.hpp"
#include "openvino/core/preprocess/pre_post_process.hpp"
#include "util/test_tools.hpp"

using namespace ov;
using namespace ov::preprocess;

static std::shared_ptr<Model> create_simple_function(element::Type type, const PartialShape& shape) {
    auto data1 = std::make_shared<op::v0::Parameter>(type, shape);
    data1->set_friendly_name("input1");
    data1->get_output_tensor(0).set_names({"tensor_input1"});
    auto op = std::make_shared<op::v0::Relu>(data1);
    op->set_friendly_name("Relu");
    op->get_output_tensor(0).set_names({"tensor_Relu"});
    auto res = std::make_shared<op::v0::Result>(op);
    res->set_friendly_name("Result1");
    res->get_output_tensor(0).set_names({"tensor_output1"});
    return std::make_shared<Model>(ResultVector{res}, ParameterVector{data1});
}

template <int N>
static std::shared_ptr<Model> create_n_inputs(element::Type type, const PartialShape& shape) {
    ResultVector res;
    ParameterVector params;
    for (size_t i = 0; i < N; i++) {
        auto index_str = std::to_string(i);
        auto data1 = std::make_shared<op::v0::Parameter>(type, shape);
        data1->set_friendly_name("input" + index_str);
        data1->get_output_tensor(0).set_names({"tensor_input" + index_str});
        auto op1 = std::make_shared<op::v0::Relu>(data1);
        op1->set_friendly_name("Relu" + index_str);
        auto res1 = std::make_shared<op::v0::Result>(op1);
        res1->set_friendly_name("Result" + index_str);
        res1->get_output_tensor(0).set_names({"tensor_output" + index_str});
        params.push_back(data1);
        res.push_back(res1);
    }
    return std::make_shared<Model>(res, params);
}

TEST(pre_post_process, simple_mean_scale) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input().preprocess().mean(1.f).scale(2.f);
    f = p.build();
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, simple_mean_scale_getters_f16) {
    auto f = create_simple_function(element::f16, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input("tensor_input1").preprocess().mean(1).scale(2);
    f = p.build();
    EXPECT_EQ(f->get_output_element_type(0), element::f16);
}

TEST(pre_post_process, simple_mean_scale_getters_f64) {
    auto f = create_simple_function(element::f64, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input("tensor_input1").preprocess().mean(1).scale(2);
    f = p.build();
    EXPECT_EQ(f->get_output_element_type(0), element::f64);
}

TEST(pre_post_process, convert_element_type_and_scale) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::i16);
    p.input().preprocess().convert_element_type(element::f32).scale(2.f).convert_element_type(element::i8);
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i16);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
}

TEST(pre_post_process, convert_element_type_implicit) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::f32);
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_results().front()->get_element_type(), element::i32);
}

TEST(pre_post_process, convert_element_type_same) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    auto old_size = f->get_ops().size();
    auto p = PrePostProcessor(f);
    p.input("tensor_input1").tensor().set_element_type(element::i32);
    p.input("tensor_input1").preprocess().convert_element_type(element::i32);
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i32);
    EXPECT_EQ(old_size, f->get_ops().size());
}

TEST(pre_post_process, convert_element_type_default) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto type_custom1 = element::Type();
    auto type_custom2 = element::Type();
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::i32);
    p.input()
        .preprocess()
        .custom([&type_custom1](const Output<Node>& node) {
            type_custom1 = node.get_element_type();
            return node;
        })
        .convert_element_type()
        .custom([&type_custom2](const Output<Node>& node) {
            type_custom2 = node.get_element_type();
            return node;
        });
    f = p.build();
    EXPECT_EQ(type_custom1, element::i32);
    EXPECT_EQ(type_custom2, element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i32);
    EXPECT_EQ(f->get_results().front()->get_element_type(), element::f32);
}

TEST(pre_post_process, empty_preprocess) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::i8);
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i8);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
}

TEST(pre_post_process, preprocess_assert_input_without_index) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_ANY_THROW(p.input().preprocess().mean(0.f); f = p.build());
    EXPECT_ANY_THROW(p.input("some_non_existing_name").preprocess().mean(0.f); f = p.build());
}

TEST(pre_post_process, convert_element_type_from_unknown) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    ASSERT_THROW(p.input().preprocess().convert_element_type(element::dynamic).convert_element_type(element::i32);
                 f = p.build();
                 , ov::AssertFailure);
}

TEST(pre_post_process, scale_not_float) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    ASSERT_THROW(p.input().preprocess().convert_element_type(element::i32).scale(2.0f);
                 f = p.build(), ov::AssertFailure);
}

TEST(pre_post_process, mean_not_float) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    ASSERT_THROW(p.input().preprocess().convert_element_type(element::i32).mean(2.0f);
                 f = p.build(), ov::AssertFailure);
}

TEST(pre_post_process, tensor_element_type_and_scale) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 1, 1});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::f32);
    p.input().preprocess().scale(2.0f).convert_element_type(element::i8);
    f = p.build();

    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), Layout());
}

TEST(pre_post_process, convert_color_nv12_rgb_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto p = PrePostProcessor(f);
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    p.input().tensor().set_element_type(element::u8).set_color_format(ColorFormat::NV12_SINGLE_PLANE);
    p.input().preprocess().convert_color(ColorFormat::RGB).convert_element_type(element::f32);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters().front()->get_partial_shape(), (PartialShape{Dimension::dynamic(), 3, 2, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_nv12_bgr_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::NV12_SINGLE_PLANE);
    p.input().preprocess().convert_color(ColorFormat::BGR);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters().front()->get_partial_shape(), (PartialShape{Dimension::dynamic(), 3, 2, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes) {
    auto f = create_simple_function(element::f32, Shape{5, 2, 2, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"TestY", "TestUV"});
    p.input().preprocess().convert_color(ColorFormat::BGR);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_friendly_name(), "input1/TestY");
    EXPECT_EQ(*f->get_parameters()[0]->output(0).get_tensor().get_names().begin(), "tensor_input1/TestY");
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{5, 2, 2, 1}));

    EXPECT_EQ(f->get_parameters()[1]->get_friendly_name(), "input1/TestUV");
    EXPECT_EQ(*f->get_parameters()[1]->output(0).get_tensor().get_names().begin(), "tensor_input1/TestUV");
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{5, 1, 1, 2}));
}

TEST(pre_post_process, convert_color_nv12_rgb_2_planes) {
    auto f = create_simple_function(element::f32, Shape{5, 2, 2, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
    p.input().preprocess().convert_color(ColorFormat::RGB);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{5, 2, 2, 1}));
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{5, 1, 1, 2}));
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes_u8) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
    p.input().preprocess().convert_color(ColorFormat::BGR);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{1, 2, 2, 1}));
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{1, 1, 1, 2}));
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes_el_type) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::f32).set_color_format(ColorFormat::NV12_TWO_PLANES);
    p.input().preprocess().convert_element_type(element::u8).convert_color(ColorFormat::BGR);
    f = p.build();

    ASSERT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
}

TEST(pre_post_process, convert_color_i420_bgr_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->input().get_tensor().get_names();
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::I420_SINGLE_PLANE);
    p.input().preprocess().convert_color(ColorFormat::BGR);
    f = p.build();

    EXPECT_EQ(f->inputs().size(), 1);
    EXPECT_EQ(f->input().get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->input().get_partial_shape(), (PartialShape{Dimension::dynamic(), 3, 2, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->input().get_tensor().get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_i420_rgb_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 4, 4, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->input().get_tensor().get_names();
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::I420_SINGLE_PLANE);
    p.input().preprocess().convert_color(ColorFormat::RGB);
    f = p.build();

    EXPECT_EQ(f->inputs().size(), 1);
    EXPECT_EQ(f->input().get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->input().get_partial_shape(), (PartialShape{Dimension::dynamic(), 6, 4, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->input().get_tensor().get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_i420_bgr_3_planes) {
    auto f = create_simple_function(element::f32, Shape{5, 30, 20, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::I420_THREE_PLANES, {"TestY", "TestU", "TestV"});
    p.input().preprocess().convert_color(ColorFormat::BGR);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 3);

    EXPECT_EQ(f->get_parameters()[0]->get_friendly_name(), "input1/TestY");
    EXPECT_EQ(*f->input(0).get_tensor().get_names().begin(), "tensor_input1/TestY");
    EXPECT_EQ(f->input(0).get_element_type(), element::f32);
    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{5, 30, 20, 1}));

    EXPECT_EQ(f->get_parameters()[1]->get_friendly_name(), "input1/TestU");
    EXPECT_EQ(*f->input(1).get_tensor().get_names().begin(), "tensor_input1/TestU");
    EXPECT_EQ(f->input(1).get_element_type(), element::f32);
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{5, 15, 10, 1}));

    EXPECT_EQ(f->get_parameters()[2]->get_friendly_name(), "input1/TestV");
    EXPECT_EQ(*f->input(2).get_tensor().get_names().begin(), "tensor_input1/TestV");
    EXPECT_EQ(f->input(2).get_element_type(), element::f32);
    EXPECT_EQ(f->input(2).get_partial_shape(), (PartialShape{5, 15, 10, 1}));
}

TEST(pre_post_process, convert_color_i420_rgb_3_planes) {
    auto f = create_simple_function(element::u8, Shape{5, 20, 20, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::I420_THREE_PLANES);
    p.input().preprocess().convert_color(ColorFormat::RGB);
    f = p.build();

    EXPECT_EQ(f->inputs().size(), 3);
    EXPECT_EQ(f->input(0).get_element_type(), element::u8);
    EXPECT_EQ(f->input(1).get_element_type(), element::u8);
    EXPECT_EQ(f->input(2).get_element_type(), element::u8);
    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{5, 20, 20, 1}));
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{5, 10, 10, 1}));
    EXPECT_EQ(f->input(2).get_partial_shape(), (PartialShape{5, 10, 10, 1}));
}

TEST(pre_post_process, convert_color_same_type) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_color_format(ColorFormat::RGB);
    p.input().preprocess().convert_color(ColorFormat::RGB);
    f = p.build();

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, convert_color_unsupported) {
    // Feel free to update this test when more color conversions are supported in future
    auto f = create_simple_function(element::f32, PartialShape{1, 4, 4, 3});

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_SINGLE_PLANE);
                 p.input().preprocess().convert_color(ColorFormat::UNDEFINED);
                 f = p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
                 p.input().preprocess().convert_color(ColorFormat::UNDEFINED);
                 f = p.build(), ov::AssertFailure);

    auto colors = {ColorFormat::NV12_TWO_PLANES, ColorFormat::NV12_SINGLE_PLANE, ColorFormat::RGB, ColorFormat::BGR};
    for (const auto& color : colors) {
        EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::UNDEFINED);
                     p.input().preprocess().convert_color(color);
                     f = p.build(), ov::AssertFailure);

        EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(color);
                     p.input().preprocess().convert_color(ColorFormat::UNDEFINED);
                     f = p.build(), ov::AssertFailure);
    }
}

TEST(pre_post_process, convert_color_incorrect_subnames) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    EXPECT_THROW(auto p = PrePostProcessor(f);
                 p.input().tensor().set_color_format(ColorFormat::NV12_SINGLE_PLANE, {"Test"});
                 p.input().preprocess().convert_color(ColorFormat::RGB);
                 p.build(), ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_SINGLE_PLANE, {"Test"});
            p.input().preprocess().convert_color(ColorFormat::RGB);
            p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f);
                 p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"Test"});
                 p.build(), ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_THREE_PLANES, {"Test"});
            p.input().preprocess().convert_color(ColorFormat::BGR);
            p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f);
                 p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"1", "2", "3"});
                 f = p.build(), ov::AssertFailure);
}

TEST(pre_post_process, convert_color_duplicate_subnames) {
    auto f = create_n_inputs<2>(element::f32, PartialShape{1, 2, 2, 3});
    f->get_parameters()[0]->get_output_tensor(0).set_names({"tensor_input1"});
    f->get_parameters()[1]->get_output_tensor(0).set_names({"tensor_input1/CustomUV"});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_color_format(ColorFormat::NV12_SINGLE_PLANE, {"CustomY", "CustomUV"});
                 p.input().preprocess().convert_color(ColorFormat::RGB);
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, convert_color_duplicate_internal_subnames_mean) {
    auto f = create_simple_function(element::f32, PartialShape{1, 2, 2, 3});
    for (int i = 0; i < 10; i++) {
        // Create preprocessing step several times (try to duplicate internal node names this way)
        EXPECT_NO_THROW(auto p = PrePostProcessor(f); p.input().preprocess().mean(0.1f); f = p.build());
        EXPECT_NO_THROW(auto p = PrePostProcessor(f); p.input().preprocess().scale(1.1f); f = p.build());
        EXPECT_NO_THROW(auto p = PrePostProcessor(f);
                        p.input().preprocess().convert_element_type(element::u8).convert_element_type(element::f32);
                        f = p.build());
    }
    f = create_simple_function(element::f32, PartialShape{1, 2, 2, 3});
    for (int i = 0; i < 10; i++) {
        auto p = PrePostProcessor(f);
        p.input().tensor().set_layout("NHWC");
        p.input().preprocess().convert_layout("NCHW");
        p.input().model().set_layout("NHWC");
        f = p.build();
    }
    f = create_simple_function(element::f32, PartialShape{1, 2, 2, 3});
    auto p = PrePostProcessor(f);
    for (int i = 10; i < 20; i++) {
        p.input().preprocess().resize(ResizeAlgorithm::RESIZE_LINEAR, i, i);
    }
    p.input().preprocess().resize(ResizeAlgorithm::RESIZE_LINEAR);
    p.input().tensor().set_spatial_static_shape(480, 640);
    p.input().model().set_layout("NHWC");
    EXPECT_NO_THROW(f = p.build());
}

TEST(pre_post_process, unsupported_model_color_format) {
    auto f = create_simple_function(element::f32, PartialShape{1, 4, 4, 3});
    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_SINGLE_PLANE);
                 f = p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
                 f = p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
                 p.input().preprocess().convert_layout("NCHW").convert_color(ColorFormat::RGB);
                 f = p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
                 p.input().preprocess().mean(0.1f).convert_color(ColorFormat::RGB);
                 f = p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f); p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES);
                 p.input().preprocess().scale(2.1f).convert_color(ColorFormat::RGB);
                 f = p.build(), ov::AssertFailure);
}

TEST(pre_post_process, unsupported_model_color_format_i420) {
    auto f = create_simple_function(element::f32, PartialShape{1, 4, 4, 3});
    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_SINGLE_PLANE);
            f = p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_THREE_PLANES);
            f = p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_SINGLE_PLANE);
            p.input().preprocess().convert_layout("NCHW").convert_color(ColorFormat::RGB);
            f = p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_color_format(ColorFormat::I420_THREE_PLANES);
            p.input().preprocess().scale(2.1).convert_color(ColorFormat::BGR);
            f = p.build();
        },
        ov::AssertFailure);
}

TEST(pre_post_process, custom_preprocessing) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 1, 1});
    auto p = PrePostProcessor(f);
    p.input().preprocess().custom([](const Output<Node>& node) {
        return std::make_shared<op::v0::Abs>(node);
    });
    f = p.build();
    EXPECT_EQ(f->get_output_element_type(0), element::i32);
}

TEST(pre_post_process, test_2_inputs_basic) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 1, 1});
    auto p = PrePostProcessor(f);
    p.input(1).preprocess().mean(1.f).scale(2.0f);
    f = p.build();
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    EXPECT_EQ(f->get_output_element_type(1), element::f32);
}

TEST(pre_post_process, reuse_model_layout_no_tensor_info) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 2, 1});
    f->get_parameters().front()->set_layout("NC??");
    auto p = PrePostProcessor(f);
    p.input().preprocess().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f});
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
}

TEST(pre_post_process, reuse_model_layout_tensor_info) {
    auto f = create_simple_function(element::u8, PartialShape{Dimension::dynamic(), 3, 2, 1});
    f->get_parameters().front()->set_layout("NC??");
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::f32);
    p.input().preprocess().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f}).convert_element_type(element::u8);
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
}

TEST(pre_post_process, mean_scale_vector_tensor_layout) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 2, 1});
    auto name = f->get_parameters().front()->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    auto p = PrePostProcessor(f);
    p.input().tensor().set_layout("NC??");
    p.input().preprocess().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f});
    f = p.build();
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, mean_scale_dynamic_layout) {
    auto f = create_simple_function(element::f32,
                                    PartialShape{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), 3});
    auto name = f->get_parameters().front()->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    auto p = PrePostProcessor(f);

    p.input().tensor().set_layout("N...C");
    p.input().preprocess().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f});
    f = p.build();

    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "N...C");
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, scale_vector_no_channels_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("N?HW"); p.input().preprocess().scale({0.1f, 0.2f, 0.3f});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, scale_vector_dim_mismatch) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("NCHW"); p.input().preprocess().scale({0.1f, 0.2f, 0.3f, 0.4f});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, scale_vector_channels_out_of_range) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    ASSERT_EQ(f->get_output_element_type(0), element::f32);
    auto p = PrePostProcessor(f);
    ASSERT_THROW(p.input().tensor().set_layout("0123C"); p.input().preprocess().scale({0.1f, 0.2f, 0.3f});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, mean_vector_no_layout) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 224, 224});
    ASSERT_EQ(f->get_output_element_type(0), element::f32);
    auto p = PrePostProcessor(f);
    ASSERT_THROW(p.input().preprocess().mean({0.1f, 0.2f, 0.3f}); p.build(), ov::AssertFailure);
}

TEST(pre_post_process, mean_vector_dynamic_channels_shape) {
    auto f = create_simple_function(
        element::f32,
        PartialShape{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    auto p = PrePostProcessor(f);
    EXPECT_NO_THROW(p.input().tensor().set_layout("NCHW"); p.input().preprocess().mean({0.1f, 0.2f, 0.3f}); p.build());
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

// Error cases for 'resize'
TEST(pre_post_process, resize_no_model_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("NHWC"); p.input().preprocess().resize(ResizeAlgorithm::RESIZE_CUBIC);
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, tensor_spatial_shape_no_layout_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("NC?W").set_spatial_static_shape(480, 640);
                 p.input().preprocess().resize(ResizeAlgorithm::RESIZE_CUBIC);
                 p.build(), ov::AssertFailure);

    EXPECT_THROW(p.input().tensor().set_layout("NCH?").set_spatial_static_shape(480, 640);
                 p.input().preprocess().resize(ResizeAlgorithm::RESIZE_CUBIC);
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, tensor_set_shape_incompatible) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_shape({1, 4, 224, 224}); p.build(), ov::AssertFailure);
}

// Check that 'set_shape' shall not be used together with set_spatial_..._shape
// This test can be removed if this requirement is relaxed in future releases
TEST(pre_post_process, tensor_set_shape_with_spatial) {
    auto f = create_simple_function(element::f32, PartialShape{-1, -1, -1, -1});
    {
        auto p = PrePostProcessor(f);
        p.input().tensor().set_layout("NCHW");
        EXPECT_THROW(p.input().tensor().set_shape({1, 3, 224, 224}).set_spatial_static_shape(448, 448);
                     p.build(), ov::AssertFailure);
    }
    {
        auto p = PrePostProcessor(f);
        p.input().tensor().set_layout("NCHW");
        EXPECT_THROW(p.input().tensor().set_spatial_static_shape(448, 448).set_shape({1, 3, 224, 224});
                     p.build(), ov::AssertFailure);
    }
    {
        auto p = PrePostProcessor(f);
        p.input().tensor().set_layout("NCHW");
        EXPECT_THROW(p.input().tensor().set_shape({1, 3, 224, 224}).set_spatial_dynamic_shape();
                     p.build(), ov::AssertFailure);
    }
    {
        auto p = PrePostProcessor(f);
        p.input().tensor().set_layout("NCHW");
        EXPECT_THROW(p.input().tensor().set_spatial_dynamic_shape().set_shape({1, 3, 224, 224});
                     p.build(), ov::AssertFailure);
    }
}

TEST(pre_post_process, resize_no_tensor_height) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("N?WC"); p.input().preprocess().resize(ResizeAlgorithm::RESIZE_LINEAR);
                 p.input().model().set_layout("NHWC");
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, resize_no_tensor_width) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("NH?C"); p.input().preprocess().resize(ResizeAlgorithm::RESIZE_LINEAR);
                 p.input().model().set_layout("NHWC");
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, preprocess_convert_layout_implicit) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto name = f->get_results().front()->get_friendly_name();
    auto name_last_op = f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name();
    auto tensor_names = f->output().get_tensor().get_names();

    auto p = PrePostProcessor(f);

    p.input().tensor().set_layout("NHWC");
    p.input().model().set_layout("NCHW");
    p.build();
    EXPECT_EQ(f->get_parameters()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
    EXPECT_EQ(name, f->get_results().front()->get_friendly_name());
    EXPECT_EQ(name_last_op,
              f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name());
    EXPECT_EQ(tensor_names, f->output().get_tensor().get_names());
}

TEST(pre_post_process, preprocess_convert_layout_default) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);

    p.input().tensor().set_layout("NHWC");
    p.input().preprocess().convert_layout();
    p.input().model().set_layout("NCHW");
    p.build();
    EXPECT_EQ(f->get_parameters()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, preprocess_convert_layout_same_various) {
    for (size_t i = 1; i < 100; i++) {
        auto f = create_simple_function(element::f32, PartialShape::dynamic(static_cast<int64_t>(i)));
        auto p = PrePostProcessor(f);
        std::stringstream stream;
        stream << "[0";
        for (auto j = 1; j < i; j++) {
            stream << "," << std::to_string(j);
        }
        stream << "]";
        auto l = stream.str();
        p.input().tensor().set_layout(ov::Layout(l));
        p.input().model().set_layout(ov::Layout(std::string(i, '?')));
        EXPECT_NO_THROW(p.build());
    }
}

TEST(pre_post_process, preprocess_convert_layout_same) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto size_old = f->get_ordered_ops().size();

    auto p = PrePostProcessor(f);

    p.input().tensor().set_layout("NCHW");
    p.input().preprocess().convert_layout("NCHW");
    p.input().model().set_layout("NCHW");
    p.build();
    EXPECT_EQ(f->get_parameters()[0]->get_layout(), "NCHW");
    EXPECT_EQ(f->get_parameters()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 3, 2, 2}));
    // Verify that redundant ops were not added
    EXPECT_EQ(size_old, f->get_ordered_ops().size());
}

TEST(pre_post_process, preprocess_convert_layout_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 480, 640});

    auto p = PrePostProcessor(f);
    p.input().preprocess().convert_layout({0, 3, 1, 2});
    p.build();

    EXPECT_EQ(f->input().get_partial_shape(), (PartialShape{1, 480, 640, 3}));
}

TEST(pre_post_process, preprocess_convert_layout_dims_empty) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 480, 640});

    auto p = PrePostProcessor(f);

    p.input().preprocess().convert_layout(std::vector<uint64_t>{});
    p.build();

    EXPECT_EQ(f->input().get_partial_shape(), (PartialShape{1, 3, 480, 640}));
}

TEST(pre_post_process, preprocess_convert_layout_dims_dyn_shape) {
    auto f = create_simple_function(element::f32, PartialShape::dynamic());

    auto p = PrePostProcessor(f);
    p.input().preprocess().convert_layout({0, 3, 1, 2});
    p.build();

    EXPECT_EQ(f->input().get_partial_shape(), (PartialShape::dynamic()));
}

TEST(pre_post_process, preprocess_convert_layout_invalid_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().preprocess().convert_layout({0, 3, 2, 2}); p.build(), ov::AssertFailure);

    EXPECT_THROW(p.input().preprocess().convert_layout({0, 3, 1, std::numeric_limits<uint64_t>::max()});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, preprocess_convert_layout_invalid_dims_dyn_shape) {
    auto f = create_simple_function(element::f32, PartialShape::dynamic());
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().preprocess().convert_layout({0, 3, 2, 2}); p.build(), ov::AssertFailure);

    EXPECT_THROW(p.input().preprocess().convert_layout({0, 3, 1, std::numeric_limits<uint64_t>::max()});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, preprocess_convert_layout_partially_defined) {
    auto f = create_n_inputs<8>(element::f32, Shape{1, 2, 3, 4, 5});

    auto p = PrePostProcessor(f);
    p.input(0).tensor().set_layout("nc???");
    p.input(0).model().set_layout("????c");

    p.input(1).tensor().set_layout("...c??");
    p.input(1).model().set_layout("ndhwc");

    p.input(2).tensor().set_layout("?cwh...");
    p.input(2).model().set_layout("...hwc");

    p.input(3).tensor().set_layout("...c");
    p.input(3).model().set_layout("c...");

    p.input(4).tensor().set_layout("...");
    p.input(4).model().set_layout("c...");

    p.input(5).tensor().set_layout("...c");
    p.input(5).model().set_layout("...");

    p.input(6).tensor().set_layout("ndhwc");
    p.input(6).model().set_layout("ndh?c");

    p.input(7).tensor().set_layout("ndh?c");
    p.input(7).model().set_layout("ndhwc");

    f = p.build();
    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{1, 5, 2, 3, 4}));
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{1, 2, 5, 3, 4}));
    EXPECT_EQ(f->input(2).get_partial_shape(), (PartialShape{1, 5, 4, 3, 2}));
    EXPECT_EQ(f->input(3).get_partial_shape(), (PartialShape{2, 3, 4, 5, 1}));
    EXPECT_EQ(f->input(4).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(5).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(6).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(7).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
}

TEST(pre_post_process, preprocess_convert_layout_partially_defined_trivial) {
    auto f = create_n_inputs<4>(element::f32, Shape{1, 2, 3, 4, 5});
    auto ops_num = f->get_ordered_ops().size();

    auto p = PrePostProcessor(f);
    p.input(0).tensor().set_layout("...");
    p.input(0).model().set_layout("c...");

    p.input(1).tensor().set_layout("...c");
    p.input(1).model().set_layout("...");

    p.input(2).tensor().set_layout("ndhwc");
    p.input(2).model().set_layout("ndh?c");

    p.input(3).tensor().set_layout("ndh?c");
    p.input(3).model().set_layout("ndhwc");

    f = p.build();
    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(2).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    EXPECT_EQ(f->input(3).get_partial_shape(), (PartialShape{1, 2, 3, 4, 5}));
    // Verify that no preprocessing Nodes are inserted
    EXPECT_EQ(ops_num, f->get_ordered_ops().size());
}

TEST(pre_post_process, preprocess_convert_layout_partially_defined_error) {
    auto f = create_simple_function(element::f32, Shape{1, 2, 3, 4, 5});

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_layout("nch??");
            p.input().model().set_layout("???wc");
            f = p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_layout("nch??");
            p.input().model().set_layout("???wc?");
            f = p.build();
        },
        ov::AssertFailure);
}

TEST(pre_post_process, preprocess_convert_layout_partially_defined_error_diff_rank) {
    auto f = create_simple_function(element::f32, Shape{1, 2, 3, 4, 5});
}

TEST(pre_post_process, preprocess_convert_layout_partially_defined_error_dyn_rank) {
    auto f = create_simple_function(element::f32, PartialShape::dynamic());

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_layout("nchw");
            p.input().model().set_layout("...wc");
            f = p.build();
        },
        ov::AssertFailure);

    EXPECT_THROW(
        {
            auto p = PrePostProcessor(f);
            p.input().tensor().set_layout("nchw");
            p.input().model().set_layout("??wc?");
            f = p.build();
        },
        ov::AssertFailure);
}

TEST(pre_post_process, preprocess_reverse_channels_multiple_planes) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(

        p.input().tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"Y", "UV"});
        p.input().preprocess().reverse_channels();
        p.build(), ov::AssertFailure);
}

TEST(pre_post_process, preprocess_reverse_channels_no_c_dim) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.input().tensor().set_layout("N?HW"); p.input().preprocess().reverse_channels();
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, preprocess_reverse_channels_no_shape_inference) {
    auto f = create_simple_function(element::f32,
                                    PartialShape{Dimension::dynamic(), 3, Dimension::dynamic(), Dimension::dynamic()});
    auto out_shape = f->output(0).get_partial_shape();

    using namespace ov::preprocess;
    PrePostProcessor p(f);
    p.input(0).tensor().set_layout("NCHW");
    p.input(0).preprocess().reverse_channels();
    ASSERT_NO_THROW(p.build());
    // Ensure that {?,3,?,?} is not transformed to {?,?,?,?}
    EXPECT_EQ(out_shape, f->output(0).get_partial_shape());
}

TEST(pre_post_process, preprocess_preserve_rt_info) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    f->get_parameters()[0]->get_rt_info()["someKey"] = "someValue";
    f->input().get_rt_info()["someKey_in"] = "someValue_in";
    auto p = PrePostProcessor(f);
    p.input().tensor().set_element_type(element::u8);
    f = p.build();
    EXPECT_EQ(f->input().get_element_type(), element::u8);

    ASSERT_EQ(f->get_parameters()[0]->get_rt_info().count("someKey"), 1);
    auto var0 = f->get_parameters()[0]->get_rt_info()["someKey"].as<std::string>();
    EXPECT_EQ(var0, "someValue");

    ASSERT_EQ(f->input().get_rt_info().count("someKey_in"), 1);
    auto var0_in = f->input().get_rt_info()["someKey_in"].as<std::string>();
    EXPECT_EQ(var0_in, "someValue_in");
}

TEST(pre_post_process, preprocess_memory_type) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_memory_type("abc");
    f = p.build();
    ASSERT_EQ(f->input().get_rt_info().count(TensorInfoMemoryType::get_type_info_static()), 1);
    auto var0 = f->input().get_rt_info()[TensorInfoMemoryType::get_type_info_static()].as<TensorInfoMemoryType>().value;
    EXPECT_EQ(var0, "abc");
}

TEST(pre_post_process, preprocess_memory_type_clear) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    f->input().get_rt_info()[TensorInfoMemoryType::get_type_info_static()] = TensorInfoMemoryType("abc");
    auto p = PrePostProcessor(f);
    p.input().tensor().set_memory_type("");
    f = p.build();
    EXPECT_EQ(f->input().get_rt_info().count(TensorInfoMemoryType::get_type_info_static()), 0);
}

TEST(pre_post_process, preprocess_memory_type_not_cleared) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.input().tensor().set_memory_type("abc").set_layout("NHWC");
    f = p.build();

    ASSERT_EQ(f->input().get_rt_info().count(TensorInfoMemoryType::get_type_info_static()), 1);
    auto var0 = f->input().get_rt_info()[TensorInfoMemoryType::get_type_info_static()].as<TensorInfoMemoryType>().value;
    EXPECT_EQ(var0, "abc");
}

// --- PostProcess - set/convert element type ---

TEST(pre_post_process, postprocess_convert_element_type_explicit) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto name = f->output().get_node_shared_ptr()->get_friendly_name();
    auto name_last_op = f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name();
    auto old_names = f->output().get_tensor().get_names();
    auto p = PrePostProcessor(f);

    p.output().postprocess().convert_element_type(element::u8);
    p.build();
    EXPECT_EQ(f->get_results().size(), 1);
    EXPECT_EQ(f->get_results()[0]->get_element_type(), element::u8);
    EXPECT_EQ(f->output().get_tensor().get_names(), old_names);
    EXPECT_EQ(old_names.count("tensor_output1"), 1);
    auto ops = f->get_ordered_ops();
    auto res_count = std::count_if(ops.begin(), ops.end(), [](const std::shared_ptr<ov::Node>& n) {
        return std::dynamic_pointer_cast<ov::op::v0::Result>(n) != nullptr;
    });
    EXPECT_EQ(res_count, 1);
    auto names_count = std::count_if(ops.begin(), ops.end(), [](std::shared_ptr<ov::Node> n) {
        return n->output(0).get_tensor().get_names().count("tensor_output1") > 0;
    });
    EXPECT_EQ(names_count, 2);  // last node + result referencing to it
    EXPECT_EQ(name, f->output().get_node_shared_ptr()->get_friendly_name());
    EXPECT_EQ(name_last_op,
              f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name());
}

TEST(pre_post_process, postprocess_convert_element_type_default) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 2, 2});
    auto name = f->output(1).get_node_shared_ptr()->get_friendly_name();
    auto name_last_op = f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name();
    auto tensor_names = f->output(1).get_tensor().get_names();
    auto p = PrePostProcessor(f);

    p.output(1).postprocess().convert_element_type();
    p.output(1).tensor().set_element_type(element::u8);
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_results()[1]->get_element_type(), element::u8);
    EXPECT_EQ(name, f->output(1).get_node_shared_ptr()->get_friendly_name());
    EXPECT_EQ(name_last_op,
              f->get_results().front()->get_input_source_output(0).get_node_shared_ptr()->get_friendly_name());
    EXPECT_EQ(tensor_names, f->output(1).get_tensor().get_names());
}

TEST(pre_post_process, postprocess_convert_element_type_same) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto size_old = f->get_ordered_ops().size();
    auto p = PrePostProcessor(f);

    p.output("tensor_output1").postprocess().convert_element_type(element::f32);
    p.output("tensor_output1").tensor().set_element_type(element::f32);
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_element_type(), element::f32);

    // Verify that redundant ops were not added
    EXPECT_EQ(size_old, f->get_ordered_ops().size());
}

TEST(pre_post_process, postprocess_convert_element_type_default_error) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.output().postprocess().convert_element_type(); p.build(), ov::AssertFailure);
}

TEST(pre_post_process, postprocess_convert_element_type_implicit) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.output().tensor().set_element_type(element::u8);
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_element_type(), element::u8);
}

TEST(pre_post_process, preprocess_keep_params_order) {
    auto f = create_n_inputs<3>(element::f32, Shape{1, 2, 2, 3});
    auto p = PrePostProcessor(f);

    p.input(1).tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"Y", "UV"});
    p.input(1).preprocess().convert_color(ColorFormat::RGB);
    p.input(0).tensor().set_layout("NCHW");
    p.input(2).tensor().set_color_format(ColorFormat::NV12_TWO_PLANES, {"Y", "UV"});
    p.input(2).preprocess().convert_color(ColorFormat::RGB);
    p.build();
    ASSERT_EQ(f->get_parameters().size(), 5);
    EXPECT_EQ(f->get_parameters()[0]->get_layout(), "NCHW");
    EXPECT_EQ(f->get_parameters()[1]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters()[2]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters()[3]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters()[4]->get_layout(), "NHWC");

    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{1, 2, 2, 1}));
    EXPECT_EQ(f->input(2).get_partial_shape(), (PartialShape{1, 1, 1, 2}));
    EXPECT_EQ(f->input(3).get_partial_shape(), (PartialShape{1, 2, 2, 1}));
    EXPECT_EQ(f->input(4).get_partial_shape(), (PartialShape{1, 1, 1, 2}));

    EXPECT_EQ(f->input(0).get_tensor().get_names(), std::unordered_set<std::string>{"tensor_input0"});
    EXPECT_EQ(f->input(1).get_tensor().get_names(), std::unordered_set<std::string>{"tensor_input1/Y"});
    EXPECT_EQ(f->input(2).get_tensor().get_names(), std::unordered_set<std::string>{"tensor_input1/UV"});
    EXPECT_EQ(f->input(3).get_tensor().get_names(), std::unordered_set<std::string>{"tensor_input2/Y"});
    EXPECT_EQ(f->input(4).get_tensor().get_names(), std::unordered_set<std::string>{"tensor_input2/UV"});
}

// --- PostProcess - set/convert layout ---
TEST(pre_post_process, postprocess_set_layout_model) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    p.output().model().set_layout("NCHW");
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NCHW");
}

TEST(pre_post_process, postprocess_convert_layout_implicit) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});

    auto p = PrePostProcessor(f);

    p.output().model().set_layout("NCHW");
    p.output().tensor().set_layout("NHWC");
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, postprocess_convert_layout_explicit_no_target) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);

    p.output(1).model().set_layout("NCHW");
    p.output(1).postprocess().convert_layout("NHWC");
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 3, 2, 2}));
    EXPECT_EQ(f->get_results()[1]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, postprocess_convert_layout_default) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});

    auto p = PrePostProcessor(f);

    p.output().model().set_layout("NCHW");
    p.output().postprocess().convert_layout();
    p.output().tensor().set_layout("NHWC");
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, postprocess_convert_layout_default_getters) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});

    auto p = PrePostProcessor(f);
    auto& out = p.output();
    out.model().set_layout("NCHW");
    out.postprocess().convert_layout();
    out.tensor().set_layout("NHWC");
    f = p.build();
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, postprocess_convert_layout_same) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto size_old = f->get_ordered_ops().size();

    auto p = PrePostProcessor(f);

    p.output().model().set_layout("NCHW");
    p.output().postprocess().convert_layout("NCHW");
    p.output().tensor().set_layout("NCHW");
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NCHW");
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 3, 2, 2}));
    // Verify that redundant ops were not added
    EXPECT_EQ(size_old, f->get_ordered_ops().size());
}

TEST(pre_post_process, postprocess_convert_layout_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 480, 640});

    auto p = PrePostProcessor(f);
    p.output().postprocess().convert_layout({0, 2, 3, 1});
    p.build();

    EXPECT_EQ(f->output().get_partial_shape(), (PartialShape{1, 480, 640, 3}));
}

TEST(pre_post_process, postprocess_convert_layout_dims_empty) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 480, 640});

    auto p = PrePostProcessor(f);

    p.output().postprocess().convert_layout(std::vector<uint64_t>{});
    p.build();

    EXPECT_EQ(f->output().get_partial_shape(), (PartialShape{1, 3, 480, 640}));
}

TEST(pre_post_process, postprocess_convert_layout_has_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 480, 640});

    auto p = PrePostProcessor(f);

    p.output().model().set_layout("NC??");
    p.output().postprocess().convert_layout({0, 2, 3, 1});
    p.build();

    EXPECT_EQ(f->output().get_partial_shape(), (PartialShape{1, 480, 640, 3}));
    EXPECT_EQ(f->get_results()[0]->get_layout(), "N??C");
}

TEST(pre_post_process, postprocess_convert_layout_invalid_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.output().postprocess().convert_layout({0, 3, 2, 2}); p.build(), ov::AssertFailure);

    EXPECT_THROW(p.output().postprocess().convert_layout({0, 3, 1, std::numeric_limits<uint64_t>::max()});
                 p.build(), ov::AssertFailure);
}

TEST(pre_post_process, postprocess_convert_layout_invalid_dims_dyn_shape) {
    auto f = create_simple_function(element::f32, PartialShape::dynamic());
    auto p = PrePostProcessor(f);
    EXPECT_THROW(p.output().postprocess().convert_layout({0, 3, 2, 2}); p.build(), ov::AssertFailure);

    EXPECT_THROW(p.output().postprocess().convert_layout({0, 3, 1, std::numeric_limits<uint64_t>::max()});
                 p.build(), ov::AssertFailure);
}

// Postprocessing - other

TEST(pre_post_process, postprocess_preserve_rt_info) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    f->get_results()[0]->get_rt_info()["someKey"] = "someValue";
    f->input().get_rt_info()["someKey_in"] = "someValue_in";
    f->output().get_rt_info()["someKey_out"] = "someValue_out";
    auto p = PrePostProcessor(f);
    p.output().tensor().set_element_type(element::u8);
    f = p.build();
    EXPECT_EQ(f->output().get_element_type(), element::u8);

    ASSERT_EQ(f->get_results()[0]->get_rt_info().count("someKey"), 1);
    auto var0 = f->get_results()[0]->get_rt_info()["someKey"].as<std::string>();
    EXPECT_EQ(var0, "someValue");

    ASSERT_EQ(f->input().get_rt_info().count("someKey_in"), 1);
    auto var0_in = f->input().get_rt_info()["someKey_in"].as<std::string>();
    EXPECT_EQ(var0_in, "someValue_in");

    ASSERT_EQ(f->output().get_rt_info().count("someKey_out"), 1);
    auto var0_out = f->output().get_rt_info()["someKey_out"].as<std::string>();
    EXPECT_EQ(var0_out, "someValue_out");
}

TEST(pre_post_process, postprocess_custom_step) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    bool hit = false;
    auto p = PrePostProcessor(f);

    p.output().postprocess().custom([&hit](const ov::Output<Node>& node) {
        auto abs = std::make_shared<op::v0::Abs>(node);
        hit = true;
        return abs;
    });
    p.build();
    EXPECT_TRUE(hit);

    EXPECT_EQ(std::string(f->get_results()[0]->get_input_source_output(0).get_node()->get_type_name()),
              std::string(op::v0::Abs::get_type_info_static().name));
}

TEST(pre_post_process, postprocess_implicit_convert_element_type_and_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);

    p.output().model().set_layout("NCHW");
    p.output().tensor().set_layout("NHWC").set_element_type(element::u8);
    p.build();
    EXPECT_EQ(f->get_results()[0]->get_element_type(), element::u8);
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->get_results()[0]->get_output_tensor(0).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, postprocess_assert_output_without_index) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 2, 2});
    auto p = PrePostProcessor(f);
    EXPECT_ANY_THROW(p.output().tensor().set_element_type(element::f32); p.build());
    EXPECT_ANY_THROW(p.output("some_non_existing_name").tensor().set_element_type(element::f32); p.build());
}

TEST(pre_post_process, postprocess_keep_results_order) {
    auto f = create_n_inputs<3>(element::f32, Shape{1, 3, 2, 2});
    auto names0 = f->output(0).get_tensor().get_names();
    auto names1 = f->output(1).get_tensor().get_names();
    auto names2 = f->output(2).get_tensor().get_names();
    auto p = PrePostProcessor(f);

    p.output(0).model().set_layout("NCHW");
    p.output(1).model().set_layout("NCHW");
    p.output(1).tensor().set_layout("NHWC").set_element_type(element::u8);
    p.build();
    ASSERT_EQ(f->get_results().size(), 3);
    EXPECT_EQ(f->output(0).get_element_type(), element::f32);
    EXPECT_EQ(f->output(1).get_element_type(), element::u8);
    EXPECT_EQ(f->output(2).get_element_type(), element::f32);

    EXPECT_EQ(f->get_results()[0]->get_layout(), "NCHW") << f->get_results()[0]->get_layout().to_string();
    EXPECT_EQ(f->get_results()[1]->get_layout(), "NHWC") << f->get_results()[1]->get_layout().to_string();
    EXPECT_EQ(f->get_results()[2]->get_layout(), "") << f->get_results()[2]->get_layout().to_string();

    EXPECT_EQ(f->output(0).get_partial_shape(), (PartialShape{1, 3, 2, 2}));
    EXPECT_EQ(f->output(1).get_partial_shape(), (PartialShape{1, 2, 2, 3}));
    EXPECT_EQ(f->output(2).get_partial_shape(), (PartialShape{1, 3, 2, 2}));

    EXPECT_EQ(f->output(0).get_tensor().get_names(), names0);
    EXPECT_EQ(f->output(1).get_tensor().get_names(), names1);
    EXPECT_EQ(f->output(2).get_tensor().get_names(), names2);
}

TEST(pre_post_process, postprocess_many) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    bool custom_called = false;

    auto p = PrePostProcessor(f);
    p.output("tensor_output1").model().set_layout("NCHW");
    p.output("tensor_output1")
        .postprocess()
        .convert_layout()
        .convert_element_type()
        .custom([&custom_called](const ov::Output<Node>& node) {
            custom_called = true;
            return std::make_shared<op::v0::Abs>(node);
        });
    p.output("tensor_output1").tensor().set_layout("NHWC").set_element_type(element::u8);

    f = p.build();
    EXPECT_EQ(f->get_results().size(), 1);
    EXPECT_EQ(f->output().get_tensor().get_names().count("tensor_output1"), 1);
    EXPECT_EQ(f->output().get_node_shared_ptr()->get_friendly_name(), "Result1");
    EXPECT_EQ(f->output().get_element_type(), element::u8);
    EXPECT_EQ(f->get_results()[0]->get_layout(), "NHWC");
    EXPECT_EQ(f->output().get_partial_shape(), (PartialShape{1, 2, 2, 3}));
    EXPECT_TRUE(custom_called);
}

TEST(pre_post_process, exception_safety) {
    auto f = create_n_inputs<2>(element::f32, Shape{1, 3, 224, 224});
    auto name0 = f->input(0).get_node_shared_ptr()->get_friendly_name();
    auto tensor_names0 = f->input(0).get_tensor().get_names();
    auto name1 = f->input(1).get_node_shared_ptr()->get_friendly_name();
    auto tensor_names1 = f->input(1).get_tensor().get_names();
    auto out_name0 = f->output(0).get_node_shared_ptr()->get_friendly_name();
    auto out_tensor_names0 = f->output(0).get_tensor().get_names();
    auto out_name1 = f->output(1).get_node_shared_ptr()->get_friendly_name();
    auto out_tensor_names1 = f->output(1).get_tensor().get_names();
    EXPECT_THROW(auto p = PrePostProcessor(f); p.input(0)  // this one is correct
                                                   .tensor()
                                                   .set_element_type(element::u8);
                 p.input(0).preprocess().convert_element_type(element::f32);
                 p.input(1)  // This one is not
                     .tensor()
                     .set_color_format(ColorFormat::NV12_TWO_PLANES);
                 p.input().preprocess().custom([](const Output<Node>& node) -> Output<Node> {
                     throw ngraph::ngraph_error("test error");
                 });
                 p.build(), ov::AssertFailure);

    EXPECT_THROW(auto p = PrePostProcessor(f);

                 p.output(0)  // this one is correct
                     .tensor()
                     .set_element_type(element::u8);
                 p.output(1)  // This one is not
                     .postprocess()
                     .custom([](const Output<Node>& node) -> Output<Node> {
                         throw ngraph::ngraph_error("test error");
                     });
                 p.build(), ngraph::ngraph_error);
    EXPECT_EQ(f->get_parameters().size(), 2);

    EXPECT_EQ(f->input(0).get_element_type(), element::f32);
    EXPECT_EQ(f->input(0).get_partial_shape(), (PartialShape{1, 3, 224, 224}));
    EXPECT_EQ(f->input(0).get_node_shared_ptr()->get_friendly_name(), name0);
    EXPECT_EQ(f->input(0).get_tensor().get_names(), tensor_names0);

    EXPECT_EQ(f->input(1).get_element_type(), element::f32);
    EXPECT_EQ(f->input(1).get_partial_shape(), (PartialShape{1, 3, 224, 224}));
    EXPECT_EQ(f->input(1).get_node_shared_ptr()->get_friendly_name(), name1);
    EXPECT_EQ(f->input(1).get_tensor().get_names(), tensor_names1);

    EXPECT_EQ(f->output(0).get_node_shared_ptr()->get_friendly_name(), out_name0);
    EXPECT_EQ(f->output(0).get_tensor().get_names(), out_tensor_names0);

    EXPECT_EQ(f->output(1).get_node_shared_ptr()->get_friendly_name(), out_name1);
    EXPECT_EQ(f->output(1).get_tensor().get_names(), out_tensor_names1);
}
