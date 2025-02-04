# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
from openvino.tools.mo.back.FakeOutputResolver import FakeOutputResolver
from openvino.tools.mo.back.replacement import BackReplacementPattern
from openvino.tools.mo.graph.graph import Graph
from openvino.tools.mo.ops.result import Result


class MaxPool(BackReplacementPattern):
    """
    Rename Pooling/max to MaxPool
    """
    enabled = True

    def run_after(self):
        return [FakeOutputResolver]

    def pattern(self):
        return dict(
            nodes=[
                ('pooling', {'type': 'Pooling', 'pool_method': 'max'})
            ],
            edges=[]
        )

    def replace_pattern(self, graph: Graph, match: dict):
        node = match['pooling']
        node.type = 'MaxPool'
        del node['pool_method']
        if 'exclude_pad' in node:
            del node['exclude_pad']

        # adding missed outputs for MaxPool node
        if node.out_port(0).disconnected():
            output = Result(node.graph, {'name': node.name + '/Result_port_0/',
                                         'keep_output_port': node.has_and_set('remove_values_output')}).create_node()
            node.out_port(0).get_connection().set_destination(output.in_port(0))

        if node.out_port(1).disconnected():
            output = Result(node.graph, {'name': node.name + '/Result_port_1/',
                                         'keep_output_port': node.has_and_set('remove_values_output')}).create_node()
            node.out_port(1).get_connection().set_destination(output.in_port(0))
