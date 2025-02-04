# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from openvino.tools.mo.ops.gather import AttributedGather
from openvino.tools.mo.front.extractor import FrontExtractorOp
from openvino.tools.mo.front.onnx.extractors.utils import onnx_attr


class GatherFrontExtractor(FrontExtractorOp):
    op = 'Gather'
    enabled = True

    @classmethod
    def extract(cls, node):
        attrs = {
            'axis': np.array(onnx_attr(node, 'axis', 'i', default=0), dtype=np.int64)
        }

        AttributedGather.update_node_stat(node, attrs)
        return cls.enabled
