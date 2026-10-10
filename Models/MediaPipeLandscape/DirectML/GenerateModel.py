# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Generate the immutable DirectML 1.0 graph for MediaPipe Selfie Segmenter."""

from __future__ import annotations

import argparse
import os
import pathlib

import numpy as np
import onnx
from onnx import numpy_helper, shape_inference
from huggingface_hub import hf_hub_download

MODEL_REPOSITORY = "onnx-community/mediapipe_selfie_segmentation_landscape"
MODEL_REVISION = "2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3"
MODEL_FILENAME = "onnx/model.onnx"
INPUT_SHAPE = [1, 3, 144, 256]
OUTPUT_SHAPE = [1, 1, 144, 256]


def float_literal(value: float) -> str:
    result = f"{value:.9g}"
    if "." not in result and "e" not in result.lower():
        result += ".0"
    return result + "F"


def attrs(node: onnx.NodeProto) -> dict[str, object]:
    return {item.name: onnx.helper.get_attribute_value(item) for item in node.attribute}


def lower_model(model: onnx.ModelProto) -> onnx.ModelProto:
    """Specialize the pinned model and lower it to the DirectML 1.0 operator set."""
    onnx.checker.check_model(model)
    if len(model.graph.input) != 1 or len(model.graph.output) != 1:
        raise RuntimeError("MediaPipe Landscape requires one input and one output")
    input_shape = model.graph.input[0].type.tensor_type.shape
    if [d.dim_value for d in input_shape.dim[1:]] != INPUT_SHAPE[1:]:
        raise RuntimeError("Expected MediaPipe Landscape NCHW RGB input")
    input_shape.dim[0].dim_value = 1
    model = shape_inference.infer_shapes(model, data_prop=True)
    shapes = {
        v.name: [d.dim_value for d in v.type.tensor_type.shape.dim]
        for v in [*model.graph.input, *model.graph.value_info, *model.graph.output]
    }
    if shapes[model.graph.output[0].name] != OUTPUT_SHAPE:
        raise RuntimeError("Expected MediaPipe Landscape single-channel alpha output")

    def constant(name: str, value: np.ndarray) -> str:
        model.graph.initializer.append(numpy_helper.from_array(value, name))
        return name

    lowered = []
    for index, node in enumerate(model.graph.node):
        prefix = f"baseline_dml_{index}"
        if node.op_type == "HardSwish":
            three = constant(prefix + "_three", np.full((1, 1, 1, 1), 3, dtype=np.float32))
            zero = constant(prefix + "_zero", np.array(0, dtype=np.float32))
            six = constant(prefix + "_six", np.array(6, dtype=np.float32))
            sixth = constant(prefix + "_sixth", np.full((1, 1, 1, 1), 1 / 6, dtype=np.float32))
            lowered.extend([
                onnx.helper.make_node("Add", [node.input[0], three], [prefix + "_add"]),
                onnx.helper.make_node("Clip", [prefix + "_add", zero, six], [prefix + "_clip"]),
                onnx.helper.make_node("Mul", [sixth, prefix + "_clip"], [prefix + "_scale"]),
                onnx.helper.make_node("Mul", [node.input[0], prefix + "_scale"], list(node.output)),
            ])
        elif node.op_type == "ReduceMean":
            attribute = attrs(node)
            if list(attribute.get("axes", [])) != [2, 3] or attribute.get("keepdims", 1) != 1:
                raise RuntimeError("Expected spatial ReduceMean with keepdims")
            lowered.append(onnx.helper.make_node(
                "AveragePool", list(node.input), list(node.output),
                kernel_shape=shapes[node.input[0]][2:], strides=[1, 1],
            ))
        elif node.op_type == "Resize":
            attribute = attrs(node)
            source, destination = shapes[node.input[0]], shapes[node.output[0]]
            if (attribute.get("mode") != b"linear"
                    or attribute.get("coordinate_transformation_mode") != b"half_pixel"
                    or source[:2] != destination[:2]
                    or any(d <= 0 or d % s for s, d in zip(source[2:], destination[2:]))):
                raise RuntimeError("Expected integer half-pixel linear spatial upsampling")
            sizes = constant(prefix + "_sizes", np.array(destination, dtype=np.int64))
            replacement = onnx.NodeProto()
            replacement.CopyFrom(node)
            del replacement.input[:]
            replacement.input.extend([node.input[0], "", "", sizes])
            lowered.append(replacement)
        else:
            lowered.append(node)

    # Resize sizes are now static, so the original Shape/Slice/Concat chains are dead.
    needed = {model.graph.output[0].name}
    live = []
    for node in reversed(lowered):
        if needed.intersection(node.output):
            live.append(node)
            needed.update(name for name in node.input if name)
    del model.graph.node[:]
    model.graph.node.extend(reversed(live))
    del model.graph.value_info[:]
    model = shape_inference.infer_shapes(model, data_prop=True)
    onnx.checker.check_model(model)
    return model


def dimensions(values: list[int]) -> str:
    return "{" + ", ".join(f"{value}u" for value in values) + "}"


DATA_TYPES = {
    onnx.TensorProto.FLOAT: "Float32", onnx.TensorProto.FLOAT16: "Float16",
    onnx.TensorProto.UINT32: "UInt32", onnx.TensorProto.UINT16: "UInt16",
    onnx.TensorProto.UINT8: "UInt8", onnx.TensorProto.INT32: "Int32",
    onnx.TensorProto.INT16: "Int16", onnx.TensorProto.INT8: "Int8",
    onnx.TensorProto.DOUBLE: "Float64", onnx.TensorProto.UINT64: "UInt64",
    onnx.TensorProto.INT64: "Int64",
}
GROUPS = {
    "AveragePool2d": "Onnx11Node", "BiasedConv2d": "Onnx11Node",
    "BiasedConvTranspose2d": "Onnx11Node",
    "Clip": "Onnx13Node", "Resize": "Onnx13Node", "Sigmoid": "Onnx13Node",
    "Add": "Onnx14Node", "Mul": "Onnx14Node", "Relu": "Onnx14Node",
}


def data_type(element_type: int) -> str:
    if element_type not in DATA_TYPES:
        raise RuntimeError(f"unsupported tensor data type: {element_type}")
    return "DataType::" + DATA_TYPES[element_type]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--offline", action="store_true", help="Use the cached pinned model only")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("resource", type=pathlib.Path)
    parser.add_argument("weights", type=pathlib.Path)
    args = parser.parse_args()

    model_path = hf_hub_download(
        repo_id=MODEL_REPOSITORY, revision=MODEL_REVISION, filename=MODEL_FILENAME,
        local_files_only=args.offline,
    )
    model = lower_model(onnx.load(model_path))
    shapes = {
        value.name: [int(dimension.dim_value) for dimension in value.type.tensor_type.shape.dim]
        for value in [*model.graph.input, *model.graph.value_info, *model.graph.output]
    }
    initializers = {item.name: item for item in model.graph.initializer}
    nodes = list(model.graph.node)
    ignored_inputs = {"Clip": {1, 2}, "Resize": {1, 2, 3}}
    allowed = {"Add", "Mul", "Relu", "Clip", "Resize", "Sigmoid", "AveragePool", "Conv", "ConvTranspose"}
    for index, node in enumerate(nodes):
        if node.op_type not in allowed:
            raise RuntimeError(f"node {index}: operator {node.op_type} is outside the pinned Landscape variants")
    types = {
        value.name: data_type(value.type.tensor_type.elem_type)
        for value in [*model.graph.input, *model.graph.value_info, *model.graph.output]
    }
    used_initializers = {
        name
        for node in nodes
        for index, name in enumerate(node.input)
        if name in initializers and index not in ignored_inputs.get(node.op_type, set())
    }

    # REUSE-IgnoreStart
    lines = [
        "// Generated by Models/MediaPipeLandscape/DirectML/GenerateModel.py. Do not edit.",
        f"// Source: {MODEL_REPOSITORY}@{MODEL_REVISION}/{MODEL_FILENAME}",
        "// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// clang-format off",
        "",
        '#include "DmlProgram/BaselineDmlProgramRuntime.hpp"',
        "#include <memory>",
        "#include <stdexcept>",
        "#ifndef NOMINMAX",
        "#define NOMINMAX",
        "#endif",
        "#include <windows.h>",
        "",
    ]
    # REUSE-IgnoreEnd
    def shape_fields(values: list[int]) -> str:
        if not 1 <= len(values) <= 4 or any(not 0 < value < 2**32 for value in values):
            raise RuntimeError(f"unsupported shape: {values}")
        return f"{len(values)}, {dimensions(values)}"

    spec_entries = []
    spec_indices = {}
    runtime_inputs = [value for value in model.graph.input if value.name not in initializers]
    for value in runtime_inputs:
        spec_indices[value.name] = len(spec_entries)
        spec_entries.append(f"{{{types[value.name]}, ItemType::RuntimeInput, {shape_fields(shapes[value.name])}, 0LL}}")
    weight_data = bytearray()
    for name, initializer in initializers.items():
        if name not in used_initializers:
            continue
        array = numpy_helper.to_array(initializer)
        dtype = data_type(initializer.data_type)
        # Preserve packed little-endian elements; GPU allocation padding belongs to the runtime.
        offset = len(weight_data)
        weight_data.extend(array.astype(array.dtype.newbyteorder("<"), copy=False).tobytes(order="C"))
        spec_indices[name] = len(spec_entries)
        spec_entries.append(f"{{{dtype}, ItemType::Weight, {shape_fields(list(initializer.dims) or [1])}, {offset}LL}}")
    if len(weight_data) >= 2**63:
        raise RuntimeError("weight data exceeds the BaselineDml signed 64-bit offset range")
    if not spec_entries or len(spec_entries) > 2**31 - 1:
        raise RuntimeError("input specification count is outside the Definition Size range")

    lines.extend(["namespace BaselineDml {", "", "namespace {", "", "using namespace ProgramDefinition;", ""])
    value_sources = {}
    node_entries = []
    for value in runtime_inputs:
        value_sources[value.name] = len(node_entries)
        node_entries.append(f"InputItem{{{spec_indices[value.name]}}}")

    def source(name: str) -> str:
        if name not in value_sources:
            if name not in spec_indices:
                raise RuntimeError(f"input {name} is not a constant or preceding node")
            node_entries.append(f"InputItem{{{spec_indices[name]}}}")
            value_sources[name] = len(node_entries) - 1
        return str(value_sources[name])

    for node_index, node in enumerate(nodes):
        if len(node.output) != 1:
            raise RuntimeError(f"node {node_index} has {len(node.output)} outputs")
        attribute = attrs(node)
        output_shape = shapes[node.output[0]]
        output = shape_fields(output_shape)
        inputs = [source(name) for index, name in enumerate(node.input)
                  if name and index not in ignored_inputs.get(node.op_type, set())]
        if node.op_type in {"Add", "Mul"}:
            entry = f"{node.op_type}{{{inputs[0]}, {inputs[1]}, {output}}}"
        elif node.op_type == "Clip":
            minimum = float(numpy_helper.to_array(initializers[node.input[1]]).reshape(-1)[0])
            maximum = float(numpy_helper.to_array(initializers[node.input[2]]).reshape(-1)[0])
            entry = f"Clip{{{inputs[0]}, {output}, {float_literal(minimum)}, {float_literal(maximum)}}}"
        elif node.op_type in {"Relu", "Sigmoid"}:
            entry = f"{node.op_type}{{{inputs[0]}, {output}}}"
        elif node.op_type in {"Conv", "ConvTranspose"}:
            strides = list(attribute.get("strides", [1, 1]))
            dilations = list(attribute.get("dilations", [1, 1]))
            pads = list(attribute.get("pads", [0, 0, 0, 0]))
            filter_shape = list(initializers[node.input[1]].dims)
            if (len(shapes[node.input[0]]) != 4 or len(filter_shape) != 4
                    or len(output_shape) != 4 or len(strides) != 2
                    or len(dilations) != 2 or len(pads) != 4):
                raise RuntimeError(f"{node.op_type} requires two spatial dimensions")
            if len(inputs) != 3:
                raise RuntimeError(f"node {node_index}: unbiased {node.op_type} is outside the pinned Landscape variants")
            kind = "Biased" + node.op_type + "2d"
            connections = ", ".join(inputs)
            group = int(attribute.get("group", 1))
            entry = (f"{kind}{{{connections}, "
                     f"{dimensions(strides)}, {dimensions(dilations)}, "
                     f"{dimensions(pads[:2])}, {dimensions(pads[2:])}, {group}u, {dimensions(output_shape)}}}")
        elif node.op_type == "AveragePool":
            kernel = list(attribute["kernel_shape"])
            strides = list(attribute.get("strides", kernel))
            pads = list(attribute.get("pads", [0, 0, 0, 0]))
            if (len(shapes[node.input[0]]) != 4 or len(output_shape) != 4
                    or len(kernel) != 2 or len(strides) != 2 or len(pads) != 4):
                raise RuntimeError("AveragePool requires two spatial dimensions")
            entry = (f"AveragePool2d{{{inputs[0]}, {dimensions(strides)}, "
                     f"{dimensions(kernel)}, {dimensions(pads[:2])}, {dimensions(pads[2:])}, {dimensions(output_shape)}}}")
        elif node.op_type == "Resize":
            input_shape = shapes[node.input[0]]
            if len(input_shape) != 4 or len(output_shape) != 4:
                raise RuntimeError("Resize requires rank-4 input and output")
            entry = (f"Resize{{{inputs[0]}, {len(output_shape)}, "
                     f"{output_shape[-2] // input_shape[-2]}u, {output_shape[-1] // input_shape[-1]}u, {dimensions(output_shape)}}}")
        else:
            raise RuntimeError(f"unsupported node {node_index}: {node.op_type}")
        kind = entry.split("{", 1)[0]
        dtype = types[node.output[0]]
        entry = entry.replace("{", "{" + dtype + ", ", 1)
        entry = f"{GROUPS[kind]}{{{entry}}}"
        node_entries.append(entry)
        value_sources[node.output[0]] = len(node_entries) - 1

    def emit_table(type_name: str, name: str, entries: list[str]) -> None:
        lines.append(f"const {type_name} {name}[] = {{")
        lines.extend(f"\t{entry}," for entry in entries)
        lines.extend(["};", ""])

    if len(node_entries) > 2**31 - 1:
        raise RuntimeError("node indices exceed the Definition Size range")
    emit_table("InputSpecItem", "inputSpecs", spec_entries)
    emit_table("Node", "graphNodes", node_entries)
    lines.extend(
        [
            "const Graph graph{",
            f"\t{len(spec_entries)}, inputSpecs,",
            f"\t{len(node_entries)}LL, graphNodes,",
            f"\t{value_sources[model.graph.output[0].name]},",
            "};",
            "",
            "auto weights() -> std::span<const std::byte>",
            "{",
            "\tHMODULE module = nullptr;",
            "\tif (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,",
            "\t\t\t       reinterpret_cast<LPCWSTR>(&graph), &module))",
            '\t\tthrow std::runtime_error("GetModuleHandleExW failed for MediaPipe Landscape weights");',
            "\tconst auto resource = FindResourceW(module, MAKEINTRESOURCEW(101), RT_RCDATA);",
            '\tif (!resource) throw std::runtime_error("MediaPipe Landscape weight resource was not found");',
            '\tconst auto size = SizeofResource(module, resource);',
            '\tconst auto handle = LoadResource(module, resource);',
            '\tconst auto data = handle ? LockResource(handle) : nullptr;',
            "\tif (!data || size != " + str(len(weight_data)) + "u)",
            '\t\tthrow std::runtime_error("MediaPipe Landscape weight resource is invalid");',
            "\treturn {static_cast<const std::byte *>(data), size};",
            "}",
            "",
            "} // namespace",
            "",
            "auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDmlProgramRuntime>",
            "{",
            "\treturn std::make_unique<BaselineDmlProgramRuntime>(graph, weights());",
            "}",
            "",
            "} // namespace BaselineDml",
            "// clang-format on",
            "",
        ]
    )
    args.source.parent.mkdir(parents=True, exist_ok=True)
    with args.source.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines))
    args.weights.parent.mkdir(parents=True, exist_ok=True)
    args.weights.write_bytes(weight_data)
    resource_path = os.path.relpath(args.weights, args.resource.parent).replace("\\", "/")
    # REUSE-IgnoreStart
    resource_lines = [
        "// Generated by Models/MediaPipeLandscape/DirectML/GenerateModel.py. Do not edit.",
        "// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        f'101 RCDATA "{resource_path}"',
        "",
    ]
    # REUSE-IgnoreEnd
    args.resource.parent.mkdir(parents=True, exist_ok=True)
    with args.resource.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(resource_lines))


if __name__ == "__main__":
    main()
