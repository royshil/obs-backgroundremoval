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
        "#include <initializer_list>",
        "#include <memory>",
        "#include <stdexcept>",
        "#ifndef NOMINMAX",
        "#define NOMINMAX",
        "#endif",
        "#include <windows.h>",
        "",
    ]
    # REUSE-IgnoreEnd
    def dimensions(values: list[int]) -> str:
        padded = [*values, *([0] * (4 - len(values)))]
        return f"{{{len(values)}u, {{{', '.join(f'{value}u' for value in padded)}}}}}"

    def braced(items: list[int]) -> str:
        return "{" + ", ".join(f"{item}u" for item in items) + "}"

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
    if len(weight_data) >= 2**32:
        raise RuntimeError("weight data exceeds the BaselineDml 32-bit offset range")

    lines.extend(
        [
            "namespace BackgroundRemoval {",
            "",
            "namespace {",
            "",
            "using namespace BaselineDml;",
            "",
            "constexpr auto dimensions(std::initializer_list<std::uint32_t> values) noexcept -> Dimensions",
            "{",
            "\tDimensions result{};",
            "\tresult.count = static_cast<std::uint32_t>(values.size());",
            "\tstd::size_t index = 0;",
            "\tfor (const auto value : values)",
            "\t\tresult.values[index++] = value;",
            "\treturn result;",
            "}",
            "constexpr auto node(NodeType type, Tensor output) noexcept -> Node",
            "{",
            "\tNode result{}; result.type = type; result.output = output; return result;",
            "}",
            "constexpr auto constant(Node result, std::uint32_t inputIndex, Tensor tensor) noexcept -> Node",
            "{ result.constants[result.constantCount++] = {inputIndex, tensor}; return result; }",
            "constexpr auto reshape(Tensor output) noexcept -> Node { return node(NodeType::reshape, output); }",
            "constexpr auto transpose(Tensor output, std::initializer_list<std::uint32_t> permutation) noexcept -> Node",
            "{ auto result = node(NodeType::transpose, output); result.first = dimensions(permutation); return result; }",
            "constexpr auto add(Tensor output) noexcept -> Node { return node(NodeType::add, output); }",
            "constexpr auto add(Tensor right, Tensor output) noexcept -> Node",
            "{ return constant(add(output), 1u, right); }",
            "constexpr auto multiply(Tensor output) noexcept -> Node { return node(NodeType::multiply, output); }",
            "constexpr auto multiply(Tensor left, Tensor output) noexcept -> Node",
            "{ return constant(multiply(output), 0u, left); }",
            "constexpr auto clip(Tensor output, float minimum, float maximum) noexcept -> Node",
            "{ auto result = node(NodeType::clip, output); result.minimum = minimum; result.maximum = maximum; return result; }",
            "constexpr auto relu(Tensor output) noexcept -> Node { return node(NodeType::relu, output); }",
            "constexpr auto sigmoid(Tensor output) noexcept -> Node { return node(NodeType::sigmoid, output); }",
            "constexpr auto convolution(Tensor filter, Tensor bias, Tensor output, ConvolutionDirection direction,",
            "\tstd::initializer_list<std::uint32_t> strides, std::initializer_list<std::uint32_t> dilations,",
            "\tstd::initializer_list<std::uint32_t> startPadding, std::initializer_list<std::uint32_t> endPadding,",
            "\tstd::uint32_t groupCount) noexcept -> Node",
            "{ auto result = constant(node(NodeType::convolution, output), 1u, filter); if (bias.weightOffset != noIndex) result = constant(result, 2u, bias);",
            "\tresult.direction = direction; result.first = dimensions(strides);",
            "\tresult.second = dimensions(dilations); result.third = dimensions(startPadding); result.fourth = dimensions(endPadding);",
            "\tresult.value = groupCount; return result; }",
            "constexpr auto averagePool(Tensor output, std::initializer_list<std::uint32_t> strides,",
            "\tstd::initializer_list<std::uint32_t> window, std::initializer_list<std::uint32_t> startPadding,",
            "\tstd::initializer_list<std::uint32_t> endPadding) noexcept -> Node",
            "{ auto result = node(NodeType::averagePool, output); result.first = dimensions(strides); result.second = dimensions(window);",
            "\tresult.third = dimensions(startPadding); result.fourth = dimensions(endPadding); return result; }",
            "constexpr auto gemm(Tensor right, Tensor output) noexcept -> Node",
            "{ return constant(node(NodeType::gemm, output), 1u, right); }",
            "constexpr auto upsample(Tensor output, std::uint32_t height, std::uint32_t width) noexcept -> Node",
            "{ auto result = node(NodeType::upsample, output); result.first = dimensions({height, width}); return result; }",
            "constexpr auto join(Tensor output, std::uint32_t axis) noexcept -> Node",
            "{ auto result = node(NodeType::join, output); result.value = axis; return result; }",
            "",
        ]
    )

    value_sources: dict[str, tuple[str, int]] = {}
    input_tensors: list[str] = []
    input_name = model.graph.input[0].name
    value_sources[input_name] = ("input", 0)
    input_tensors.append(f"{{{dimensions(shapes[input_name])}, noIndex}}")

    def weight_tensor(name: str) -> str:
        shape = list(initializers[name].dims) or [1]
        return f"{{{dimensions(shape)}, {weight_offsets[name]}u}}"

    node_entries: list[str] = []
    input_edges: list[str] = []
    intermediate_edges: list[str] = []
    node_types = {
        "Reshape": "reshape",
        "Transpose": "transpose",
        "Add": "add",
        "Mul": "multiply",
        "Clip": "clip",
        "Relu": "relu",
        "Sigmoid": "sigmoid",
        "Conv": "convolution",
        "ConvTranspose": "convolution",
        "AveragePool": "averagePool",
        "MatMul": "gemm",
        "Resize": "upsample",
        "Concat": "join",
    }

    for node_index, node in enumerate(nodes):
        if node.op_type not in node_types:
            raise RuntimeError(f"unsupported node {node_index}: {node.op_type}")
        if len(node.output) != 1:
            raise RuntimeError(f"node {node_index} has {len(node.output)} outputs")
        for input_index, name in enumerate(node.input):
            if name not in value_sources:
                continue
            source_type, source_index = value_sources[name]
            if source_type == "input":
                input_edges.append(f"{{{source_index}u, {node_index}u, {input_index}u}}")
            else:
                intermediate_edges.append(f"{{{source_index}u, 0u, {node_index}u, {input_index}u}}")

        output_name = node.output[0]
        output_shape = shapes[output_name]
        attribute = attrs(node)
        direction = "forward"
        first = second = third = fourth = dimensions([])
        minimum = maximum = "0.0F"
        value = "0u"
        if node.op_type == "Transpose":
            permutation = list(attribute["perm"])
            first = dimensions(permutation)
        elif node.op_type == "Clip":
            minimum = float(numpy_helper.to_array(initializers[node.input[1]]).reshape(-1)[0])
            maximum = float(numpy_helper.to_array(initializers[node.input[2]]).reshape(-1)[0])
            minimum, maximum = float_literal(minimum), float_literal(maximum)
        elif node.op_type in {"Conv", "ConvTranspose"}:
            strides = list(attribute.get("strides", [1, 1]))
            dilations = list(attribute.get("dilations", [1, 1]))
            pads = list(attribute.get("pads", [0, 0, 0, 0]))
            direction = "backward" if node.op_type == "ConvTranspose" else "forward"
            first, second = dimensions(strides), dimensions(dilations)
            third, fourth = dimensions(pads[:2]), dimensions(pads[2:])
            value = f"{int(attribute.get('group', 1))}u"
        elif node.op_type == "AveragePool":
            kernel = list(attribute["kernel_shape"])
            strides = list(attribute.get("strides", kernel))
            pads = list(attribute.get("pads", [0, 0, 0, 0]))
            first, second = dimensions(strides), dimensions(kernel)
            third, fourth = dimensions(pads[:2]), dimensions(pads[2:])
        elif node.op_type == "Resize":
            source_shape = shapes[node.input[0]]
            first = dimensions([output_shape[-2] // source_shape[-2], output_shape[-1] // source_shape[-1]])
        elif node.op_type == "Concat":
            value = f"{int(attribute['axis'])}u"

        output_tensor = f"{{{dimensions(output_shape)}, noIndex}}"
        constants = [(index, name) for index, name in enumerate(node.input) if name in weight_offsets and index not in ignored_inputs.get(node.op_type, set())]
        if node.op_type == "Transpose":
            entry = f"transpose({output_tensor}, {braced(permutation)})"
        elif node.op_type == "Clip":
            entry = f"clip({output_tensor}, {minimum}, {maximum})"
        elif node.op_type in {"Conv", "ConvTranspose"}:
            filter_tensor = weight_tensor(node.input[1])
            bias_tensor = weight_tensor(node.input[2]) if len(node.input) > 2 and node.input[2] in weight_offsets else "{}"
            entry = (f"convolution({filter_tensor}, {bias_tensor}, {output_tensor}, ConvolutionDirection::{direction}, {braced(strides)}, "
                     f"{braced(dilations)}, {braced(pads[:2])}, {braced(pads[2:])}, {value})")
        elif node.op_type == "AveragePool":
            entry = f"averagePool({output_tensor}, {braced(strides)}, {braced(kernel)}, {braced(pads[:2])}, {braced(pads[2:])})"
        elif node.op_type == "Resize":
            entry = f"upsample({output_tensor}, {output_shape[-2] // source_shape[-2]}u, {output_shape[-1] // source_shape[-1]}u)"
        elif node.op_type == "Concat":
            entry = f"join({output_tensor}, {value})"
        elif node.op_type == "MatMul":
            entry = f"gemm({weight_tensor(node.input[1])}, {output_tensor})"
        elif node.op_type in {"Add", "Mul"} and constants:
            input_index, name = constants[0]
            expected_index = 1 if node.op_type == "Add" else 0
            if input_index != expected_index:
                raise RuntimeError(f"unsupported constant position for {node.op_type}: {input_index}")
            entry = f"{node_types[node.op_type]}({weight_tensor(name)}, {output_tensor})"
        else:
            entry = f"{node_types[node.op_type]}({output_tensor})"
        node_entries.append(entry)
        value_sources[output_name] = ("node", node_index)

    output_source_type, output_source_index = value_sources[model.graph.output[0].name]
    if output_source_type != "node":
        raise RuntimeError("graph output is not produced by a node")

    def emit_table(type_name: str, name: str, entries: list[str]) -> None:
        lines.append(f"constexpr {type_name} {name}[] = {{")
        lines.extend(f"\t{entry}," for entry in entries)
        lines.extend(["};", ""])

    lines.append("")
    emit_table("Tensor", "graphInputs", input_tensors)
    emit_table("Node", "graphNodes", node_entries)
    emit_table("InputEdge", "graphInputEdges", input_edges)
    emit_table("IntermediateEdge", "graphIntermediateEdges", intermediate_edges)
    emit_table("OutputEdge", "graphOutputEdges", [f"{{{output_source_index}u, 0u, 0u}}"])
    lines.extend(
        [
            "constexpr Graph graph{",
            f"\t{len(input_tensors)}u, graphInputs,",
            "\t1u,",
            f"\t{len(node_entries)}u, graphNodes,",
            f"\t{len(input_edges)}u, graphInputEdges,",
            f"\t{len(intermediate_edges)}u, graphIntermediateEdges,",
            "\t1u, graphOutputEdges,",
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
            "} // namespace BackgroundRemoval",
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
