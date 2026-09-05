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


def emit_concat(node_entries: list[str], inputs: list[int], input_shapes: list[list[int]],
                axis: int, output_shape: list[int]) -> int:
    if not inputs or len(inputs) != len(input_shapes):
        raise RuntimeError("Concat requires a shape for each input and at least one input")
    if any(len(shape) != 4 for shape in [*input_shapes, output_shape]):
        raise RuntimeError("Concat requires rank-4 inputs and output")
    if not -4 <= axis < 4:
        raise RuntimeError("Concat axis is out of range")
    axis %= 4
    for shape in [*input_shapes, output_shape]:
        if any(not 0 < size < 2**32 for size in shape):
            raise RuntimeError("Concat dimensions must be positive UINT values")
        if any(shape[d] != input_shapes[0][d] for d in range(4) if d != axis):
            raise RuntimeError("Concat non-axis dimensions must match")
    if sum(shape[axis] for shape in input_shapes) != output_shape[axis]:
        raise RuntimeError("Concat output shape does not match its inputs")

    left_index = inputs[0]
    shape = list(input_shapes[0])
    for right_index, right_shape in zip(inputs[1:], input_shapes[1:]):
        shape[axis] += right_shape[axis]
        node_entries.append(f"Concat{{{left_index}, {right_index}, {axis}u, {dimensions(shape)}}}")
        left_index = len(node_entries) - 1
    return left_index


def emit_transpose(node_entries: list[str], input_index: int, input_shape: list[int],
                   permutation: list[int] | None, output_shape: list[int]) -> int:
    rank = len(input_shape)
    if not 1 <= rank <= 4:
        raise RuntimeError("Transpose requires rank 1 through 4")
    if permutation is None:
        permutation = list(reversed(range(rank)))
    if sorted(permutation) != list(range(rank)):
        raise RuntimeError("Transpose permutation must contain each input axis exactly once")
    if output_shape != [input_shape[axis] for axis in permutation]:
        raise RuntimeError("Transpose output shape does not match its permutation")
    if rank == 1:
        return input_index
    node_entries.append(f"TransposeRank{rank}{{{input_index}, {dimensions(permutation)}}}")
    return len(node_entries) - 1


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
    ignored_inputs = {"Reshape": {1}, "Clip": {1, 2}, "Resize": {1, 2, 3}}
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
        '#include "DmlProgram/BaselineDmlProgram.hpp"',
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
        if not 1 <= len(values) <= 4 or any(value <= 0 for value in values):
            raise RuntimeError(f"unsupported shape: {values}")
        return f"{len(values)}, {dimensions(values)}"

    weight_offsets: dict[str, int] = {}
    weight_data = bytearray()
    for name, initializer in initializers.items():
        if name not in used_initializers:
            continue
        array = numpy_helper.to_array(initializer)
        if array.dtype != np.float32:
            raise RuntimeError(f"initializer {name} is not float32")
        weight_offsets[name] = len(weight_data)
        weight_data.extend(array.astype("<f4", copy=False).tobytes(order="C"))
    if len(weight_data) >= 2**63:
        raise RuntimeError("weight data exceeds the BaselineDml signed 64-bit offset range")

    lines.extend(["namespace BaselineDml {", "", "namespace {", ""])
    value_sources: dict[str, int] = {model.graph.input[0].name: 0}
    node_entries: list[str] = ["Input{0}"]

    def source(name: str) -> str:
        if name not in value_sources:
            if name not in weight_offsets:
                raise RuntimeError(f"input {name} is not a constant or preceding node")
            shape = list(initializers[name].dims) or [1]
            node_entries.append(f"Constant{{{weight_offsets[name]}LL, {shape_fields(shape)}}}")
            value_sources[name] = len(node_entries) - 1
        return f"{value_sources[name]}"

    for node_index, node in enumerate(nodes):
        if len(node.output) != 1:
            raise RuntimeError(f"node {node_index} has {len(node.output)} outputs")
        attribute = attrs(node)
        output_shape = shapes[node.output[0]]
        output = shape_fields(output_shape)
        inputs = [source(name) for index, name in enumerate(node.input)
                  if name and index not in ignored_inputs.get(node.op_type, set())]
        if node.op_type == "Reshape":
            entry = f"Reshape{{{inputs[0]}, {output}}}"
        elif node.op_type == "Transpose":
            input_name = node.input[0]
            input_shape = list(initializers[input_name].dims) if input_name in initializers else shapes[input_name]
            value_sources[node.output[0]] = emit_transpose(
                node_entries, int(inputs[0]), input_shape,
                list(attribute["perm"]) if "perm" in attribute else None, output_shape,
            )
            continue
        elif node.op_type in {"Add", "Mul"}:
            if len(output_shape) != 4:
                raise RuntimeError(f"{node.op_type} requires a rank-4 output")
            entry = f"{node.op_type}Rank4{{{inputs[0]}, {inputs[1]}, {dimensions(output_shape)}}}"
        elif node.op_type == "MatMul":
            if len(output_shape) != 4:
                raise RuntimeError("MatMul requires a rank-4 output")
            entry = f"MatMulRank4{{{inputs[0]}, {inputs[1]}, {dimensions(output_shape)}}}"
        elif node.op_type == "Clip":
            minimum = float(numpy_helper.to_array(initializers[node.input[1]]).reshape(-1)[0])
            maximum = float(numpy_helper.to_array(initializers[node.input[2]]).reshape(-1)[0])
            entry = f"Clip{{{inputs[0]}, {float_literal(minimum)}, {float_literal(maximum)}}}"
        elif node.op_type in {"Relu", "Sigmoid"}:
            entry = f"{node.op_type}{{{inputs[0]}}}"
        elif node.op_type in {"Conv", "ConvTranspose"}:
            strides = list(attribute.get("strides", [1, 1]))
            dilations = list(attribute.get("dilations", [1, 1]))
            pads = list(attribute.get("pads", [0, 0, 0, 0]))
            filter_shape = list(initializers[node.input[1]].dims)
            if (len(shapes[node.input[0]]) != 4 or len(filter_shape) != 4
                    or len(output_shape) != 4 or len(strides) != 2
                    or len(dilations) != 2 or len(pads) != 4):
                raise RuntimeError(f"{node.op_type} requires two spatial dimensions")
            kind = ("Biased" if len(inputs) > 2 else "") + node.op_type + "2d"
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
            entry = (f"ResizeRank4{{{inputs[0]}, "
                     f"{output_shape[-2] // input_shape[-2]}u, {output_shape[-1] // input_shape[-1]}u, {dimensions(output_shape)}}}")
        elif node.op_type == "Concat":
            value_sources[node.output[0]] = emit_concat(
                node_entries, [int(value) for value in inputs],
                [list(initializers[name].dims) if name in initializers else shapes[name]
                 for name in node.input],
                int(attribute["axis"]), output_shape,
            )
            continue
        else:
            raise RuntimeError(f"unsupported node {node_index}: {node.op_type}")
        node_entries.append(entry)
        value_sources[node.output[0]] = len(node_entries) - 1

    def emit_table(type_name: str, name: str, entries: list[str]) -> None:
        lines.append(f"const {type_name} {name}[] = {{")
        lines.extend(f"\t{entry}," for entry in entries)
        lines.extend(["};", ""])

    emit_table("InputSpec", "inputSpecs", [f"std::array<UINT, {len(INPUT_SHAPE)}>{dimensions(INPUT_SHAPE)}"])
    emit_table("Node", "graphNodes", node_entries)
    lines.extend(
        [
            "const Graph graph{",
            "\t1, inputSpecs,",
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
            "auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDmlProgram>",
            "{",
            "\treturn std::make_unique<BaselineDmlProgram>(graph, weights());",
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
