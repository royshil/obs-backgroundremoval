// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "onnx.pb.h"
#include "pb_encode.h"
#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace OnnxTestModel {
using Bytes = std::vector<uint8_t>;
using Fields = std::vector<Bytes>;
inline Bytes encode(const pb_msgdesc_t *descriptor, const void *message)
{
	size_t size;
	if (!pb_get_encoded_size(&size, descriptor, message))
		throw std::runtime_error("ONNX size encoding failed");
	Bytes bytes(size);
	auto stream = pb_ostream_from_buffer(bytes.data(), bytes.size());
	if (!pb_encode(&stream, descriptor, message))
		throw std::runtime_error(PB_GET_ERROR(&stream));
	return bytes;
}
// Strings, bytes and already-encoded submessages all use length-delimited fields.
inline void bind(pb_callback_t &field, Fields &values)
{
	field.arg = &values;
	field.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
		for (const auto &bytes : *static_cast<const Fields *>(*arg))
			if (!pb_encode_tag_for_field(stream, field) ||
			    !pb_encode_string(stream, bytes.data(), bytes.size()))
				return false;
		return true;
	};
}
inline Fields text(const std::string &value)
{
	return {Bytes(value.begin(), value.end())};
}
inline void bindInts(pb_callback_t &field, std::vector<int64_t> &values)
{
	field.arg = &values;
	field.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
		for (auto value : *static_cast<const std::vector<int64_t> *>(*arg))
			if (!pb_encode_tag_for_field(stream, field) ||
			    !pb_encode_varint(stream, static_cast<uint64_t>(value)))
				return false;
		return true;
	};
}
inline Bytes valueInfo(const std::string &name, const std::array<uint32_t, 4> &shape)
{
	onnx_ValueInfoProto info = onnx_ValueInfoProto_init_zero;
	auto names = text(name);
	bind(info.name, names);
	info.has_type = true;
	info.type.which_value = onnx_TypeProto_tensor_type_tag;
	auto &tensor = info.type.value.tensor_type;
	tensor.has_elem_type = true;
	tensor.elem_type = onnx_TensorProto_DataType_FLOAT;
	tensor.has_shape = true;
	Fields dimensions;
	for (auto size : shape) {
		onnx_TensorShapeProto_Dimension dim = onnx_TensorShapeProto_Dimension_init_zero;
		dim.which_value = onnx_TensorShapeProto_Dimension_dim_value_tag;
		dim.value.dim_value = size;
		dimensions.push_back(encode(onnx_TensorShapeProto_Dimension_fields, &dim));
	}
	bind(tensor.shape.dim, dimensions);
	return encode(onnx_ValueInfoProto_fields, &info);
}
inline Bytes attribute(const std::string &name, std::vector<int64_t> values, bool scalar = false)
{
	onnx_AttributeProto attr = onnx_AttributeProto_init_zero;
	auto names = text(name);
	bind(attr.name, names);
	attr.has_type = true;
	if (scalar) {
		attr.type = onnx_AttributeProto_AttributeType_INT;
		attr.has_i = true;
		attr.i = values.at(0);
	} else {
		attr.type = onnx_AttributeProto_AttributeType_INTS;
		bindInts(attr.ints, values);
	}
	return encode(onnx_AttributeProto_fields, &attr);
}
inline Bytes stringAttribute(const std::string &name, const std::string &value)
{
	onnx_AttributeProto attr = onnx_AttributeProto_init_zero;
	auto names = text(name), content = text(value);
	bind(attr.name, names);
	bind(attr.s, content);
	attr.has_type = true;
	attr.type = onnx_AttributeProto_AttributeType_STRING;
	return encode(onnx_AttributeProto_fields, &attr);
}
template<class T>
Bytes initializer(const std::string &name, std::vector<int64_t> dims, std::span<const T> data, int32_t dtype)
{
	onnx_TensorProto tensor = onnx_TensorProto_init_zero;
	auto names = text(name);
	bind(tensor.name, names);
	bindInts(tensor.dims, dims);
	tensor.has_data_type = true;
	tensor.data_type = dtype;
	const auto *start = reinterpret_cast<const uint8_t *>(data.data());
	Fields raw{Bytes(start, start + data.size_bytes())};
	bind(tensor.raw_data, raw);
	return encode(onnx_TensorProto_fields, &tensor);
}
inline Bytes build(const std::string &name, const std::array<uint32_t, 4> &shape,
		   const std::array<uint32_t, 4> &outputShape)
{
	onnx_NodeProto node = onnx_NodeProto_init_zero;
	const bool fused = name == "ConvRelu" || name == "DepthwiseConvRelu";
	auto op = text(name == "MPLDConvTranspose" ? "ConvTranspose" : name == "HardSwishFused" ? "HardSwish" : name == "DepthwiseConv" || fused ? "Conv" : name);
	auto inputs = text("x"), outputs = text(fused ? "z" : "y");
	Fields attributes, weights;
	if (name == "ReduceMean") {
		attributes = {attribute("axes", {2, 3}), attribute("keepdims", {1}, true)};
	} else if (name == "Resize") {
		inputs = {Bytes{'x'}, Bytes{}, Bytes{}, Bytes{'s', 'i', 'z', 'e', 's'}};
		const std::array<int64_t, 4> sizes{outputShape[0], outputShape[1], outputShape[2], outputShape[3]};
		weights.push_back(initializer<int64_t>("sizes", {4}, sizes, onnx_TensorProto_DataType_INT64));
		attributes = {stringAttribute("mode", "linear"),
			      stringAttribute("coordinate_transformation_mode", "half_pixel")};
	} else if (name == "Add" || name == "Mul") {
		inputs.push_back(Bytes{'b'});
		const std::array<float, 2> b{0.5F, -1.25F};
		weights.push_back(initializer<float>("b", {1, 2, 1, 1}, b, onnx_TensorProto_DataType_FLOAT));
	} else if (name == "Clip") {
		inputs.push_back(Bytes{'m', 'i', 'n', 'i', 'm', 'u', 'm'});
		inputs.push_back(Bytes{'m', 'a', 'x', 'i', 'm', 'u', 'm'});
		const std::array<float, 1> low{0}, high{6};
		weights = {initializer<float>("minimum", {}, low, onnx_TensorProto_DataType_FLOAT),
			   initializer<float>("maximum", {}, high, onnx_TensorProto_DataType_FLOAT)};
	} else if (name == "Conv" || name == "DepthwiseConv" || name == "ConvTranspose" || name == "MPLDConvTranspose" || fused) {
		const bool depthwise = name == "DepthwiseConv" || name == "DepthwiseConvRelu",
			   transpose = name == "ConvTranspose" || name == "MPLDConvTranspose";
		const int64_t inputChannels = name == "MPLDConvTranspose" ? 16 : 2;
		const int64_t outputChannels = name == "MPLDConvTranspose" ? 1 : 2;
		const int64_t k = transpose ? 2 : 3;
		std::vector<float> filter(inputChannels * (depthwise ? 1 : outputChannels) * k * k);
		for (size_t i = 0; i < filter.size(); ++i)
			filter[i] = float(int(i % 7) - 3) / 16;
		const std::vector<float> bias = outputChannels == 1 ? std::vector<float>{0.25F} : std::vector<float>{0.25F, -0.5F};
		inputs.push_back(Bytes{'w'});
		inputs.push_back(Bytes{'b'});
		weights = {initializer<float>("w", {inputChannels, depthwise ? 1 : outputChannels, k, k}, filter,
					      onnx_TensorProto_DataType_FLOAT),
			   initializer<float>("b", {outputChannels}, bias, onnx_TensorProto_DataType_FLOAT)};
		attributes = {attribute("strides", transpose ? std::vector<int64_t>{2, 2} : std::vector<int64_t>{1, 1}),
			      attribute("dilations", {1, 1}),
			      attribute("pads", transpose ? std::vector<int64_t>{0, 0, 0, 0}
							  : std::vector<int64_t>{1, 1, 1, 1}),
			      attribute("group", {depthwise ? 2 : 1}, true)};
	}
	bind(node.op_type, op);
	bind(node.input, inputs);
	bind(node.output, outputs);
	bind(node.attribute, attributes);
	Fields nodes{encode(onnx_NodeProto_fields, &node)}, in{valueInfo("x", shape)}, out{valueInfo("y", outputShape)};
	if (fused) {
		onnx_NodeProto relu = onnx_NodeProto_init_zero;
		auto reluOp = text("Relu"), reluInput = text("z"), reluOutput = text("y");
		bind(relu.op_type, reluOp);
		bind(relu.input, reluInput);
		bind(relu.output, reluOutput);
		nodes.push_back(encode(onnx_NodeProto_fields, &relu));
	}
	onnx_ModelProto model = onnx_ModelProto_init_zero;
	model.has_ir_version = true;
	model.ir_version = 8;
	model.has_graph = true;
	auto graphName = text(name);
	bind(model.graph.name, graphName);
	bind(model.graph.node, nodes);
	bind(model.graph.input, in);
	bind(model.graph.output, out);
	bind(model.graph.initializer, weights);
	onnx_OperatorSetIdProto opset = onnx_OperatorSetIdProto_init_zero;
	opset.has_version = true;
	opset.version = 14;
	Fields imports{encode(onnx_OperatorSetIdProto_fields, &opset)};
	bind(model.opset_import, imports);
	return encode(onnx_ModelProto_fields, &model);
}
} // namespace OnnxTestModel
