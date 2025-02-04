# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from openvino.tools.mo.graph.graph import Node


def multi_box_prior_infer_mxnet(node: Node):
    v10 = node.has_and_set('V10_infer')
    data_H, data_W = node.in_node(0).value if v10 else node.in_node(0).shape[2:]

    num_ratios = len(node.aspect_ratio)
    num_priors = len(node.min_size) + num_ratios - 1
    if v10:
        node.out_node(0).shape = np.array([2, data_H * data_W * num_priors * 4], dtype=np.int64)
    else:
        node.out_node(0).shape = np.array([1, 2, data_H * data_W * num_priors * 4], dtype=np.int64)
