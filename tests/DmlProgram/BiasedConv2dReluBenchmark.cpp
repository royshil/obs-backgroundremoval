// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated FP32 Conv+ReLU characterization. No production dispatch changes.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include "OnnxTestModel.hpp"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <DirectML.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <span>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace BackgroundRemoval::Generated {
auto mediaPipeLandscapeDiagnosticGraph() -> BaselineDml::Graph;
auto mediaPipeLandscapeDiagnosticWeights() -> std::span<const std::byte>;
} // namespace BackgroundRemoval::Generated
namespace ConvReluExperiment {
using namespace BaselineDml;
using namespace BaselineDml::ProgramDefinition;
void check(HRESULT hr, const std::source_location where = std::source_location::current())
{
	if (FAILED(hr)) {
		std::cerr << "HRESULT=0x" << std::hex << unsigned(hr) << std::dec << " at " << where.line() << "\n";
		throw std::runtime_error("ConvRelu D3D/DirectML failure");
	}
}
auto count(const std::array<UINT, 4> &s) -> size_t
{
	return size_t(s[0]) * s[1] * s[2] * s[3];
}
auto buffer(ID3D12Device *device, UINT64 bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
	-> ComPtr<ID3D12Resource>
{
	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = type;
	D3D12_RESOURCE_DESC d{};
	d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d.Width = std::max<UINT64>(4, bytes);
	d.Height = 1;
	d.DepthOrArraySize = 1;
	d.MipLevels = 1;
	d.SampleDesc.Count = 1;
	d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if (type == D3D12_HEAP_TYPE_DEFAULT)
		d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ComPtr<ID3D12Resource> r;
	check(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r)));
	return r;
}
void transition(ID3D12GraphicsCommandList *cl, ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
	D3D12_RESOURCE_BARRIER x{};
	x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	x.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, a, b};
	cl->ResourceBarrier(1, &x);
}
void uav(ID3D12GraphicsCommandList *cl)
{
	D3D12_RESOURCE_BARRIER b{};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	cl->ResourceBarrier(1, &b);
}
struct Gpu {
	ComPtr<ID3D12Device> device;
	ComPtr<IDMLDevice> dml;
	ComPtr<IDMLCommandRecorder> recorder;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> commands;
	ComPtr<ID3D12Fence> fence;
	HANDLE event = nullptr;
	UINT64 value = 0;
	Gpu(bool warp)
	{
		if (GetEnvironmentVariableW(L"CONV_RELU_DEBUG", nullptr, 0)) {
			ComPtr<ID3D12Debug> debug;
			check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
			debug->EnableDebugLayer();
		}
		ComPtr<IDXGIAdapter> adapter;
		if (warp) {
			ComPtr<IDXGIFactory4> factory;
			check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
			check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
		}
		check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
		check(DMLCreateDevice(device.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&dml)));
		check(dml->CreateCommandRecorder(IID_PPV_ARGS(&recorder)));
		D3D12_COMMAND_QUEUE_DESC q{};
		check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)));
		check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
		check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
						IID_PPV_ARGS(&commands)));
		check(commands->Close());
		check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!event)
			throw std::runtime_error("Create GPU event");
	}
	~Gpu()
	{
		if (event)
			CloseHandle(event);
	}
	void begin()
	{
		check(allocator->Reset());
		check(commands->Reset(allocator.Get(), nullptr));
	}
	void submit()
	{
		check(commands->Close());
		ID3D12CommandList *lists[]{commands.Get()};
		queue->ExecuteCommandLists(1, lists);
		check(queue->Signal(fence.Get(), ++value));
		if (fence->GetCompletedValue() < value) {
			check(fence->SetEventOnCompletion(value, event));
			if (WaitForSingleObject(event, 30000) != WAIT_OBJECT_0)
				throw std::runtime_error("GPU wait failed");
		}
		if (FAILED(device->GetDeviceRemovedReason())) {
			ComPtr<ID3D12InfoQueue> info;
			if (SUCCEEDED(device.As(&info)))
				for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
					SIZE_T size = 0;
					info->GetMessage(i, nullptr, &size);
					std::vector<std::byte> bytes(size);
					auto *msg = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
					info->GetMessage(i, msg, &size);
					std::cerr << msg->pDescription << "\n";
				}
		}
		check(device->GetDeviceRemovedReason());
	}
};
struct Tensor {
	std::array<UINT, 4> shape, strides;
	DML_BUFFER_TENSOR_DESC buffer{};
	DML_TENSOR_DESC desc{};
	Tensor(std::array<UINT, 4> s, bool owned) : shape(s), strides{s[1] * s[2] * s[3], s[2] * s[3], s[3], 1}
	{
		buffer.DataType = DML_TENSOR_DATA_TYPE_FLOAT32;
		buffer.Flags = owned ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		buffer.DimensionCount = 4;
		buffer.Sizes = shape.data();
		buffer.Strides = strides.data();
		buffer.TotalTensorSizeInBytes = count(s) * 4;
		desc = {DML_TENSOR_TYPE_BUFFER, &buffer};
	}
};
struct Case {
	size_t index;
	BiasedConv2dRelu op;
	std::array<UINT, 4> input, filter;
	std::vector<float> weights, bias;
};
auto shape(const Node &n, const Graph &g) -> std::array<UINT, 4>
{
	return std::visit(
		[&](const auto &group) -> std::array<UINT, 4> {
			if constexpr (std::is_same_v<std::decay_t<decltype(group)>, InputItem>)
				return g.inputSpecs[group.inputSpecIndex].shape;
			else
				return std::visit(
					[](const auto &op) -> std::array<UINT, 4> {
						if constexpr (requires { op.shape; })
							return op.shape;
						else
							return {1, 1, 144, 256};
					},
					group);
		},
		n);
}
auto cases() -> std::vector<Case>
{
	const auto g = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	const auto bytes = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticWeights();
	std::vector<Case> result;
	for (size_t i = 0; i < size_t(g.nodeCount); ++i) {
		auto *f = std::get_if<FusedNode>(&g.nodes[i]);
		if (!f)
			continue;
		BiasedConv2dRelu specialized{};
		auto *op = std::get_if<BiasedConv2dRelu>(f);
		if (auto *depthwise = std::get_if<MPLDDepthwiseConv2dRelu>(f)) {
			specialized = {depthwise->dataType,
				       depthwise->inputIndex,
				       depthwise->filterIndex,
				       depthwise->biasIndex,
				       {1, 1},
				       {1, 1},
				       {1, 1},
				       {1, 1},
				       16,
				       depthwise->shape};
			op = &specialized;
		}
		if (!op)
			continue;
		Case c{i, *op, shape(g.nodes[op->inputIndex], g), shape(g.nodes[op->filterIndex], g)};
		auto load = [&](int node, size_t n) {
			auto spec = g.inputSpecs[std::get<InputItem>(g.nodes[node]).inputSpecIndex];
			if (spec.dataType != dtFloat32 || spec.byteOffset < 0)
				throw std::runtime_error("Expected FP32 weight");
			std::vector<float> v(n);
			std::memcpy(v.data(), bytes.data() + spec.byteOffset, n * 4);
			return v;
		};
		c.weights = load(op->filterIndex, count(c.filter));
		c.bias = load(op->biasIndex, op->shape[1]);
		result.push_back(std::move(c));
	}
	return result;
}
auto onnx(const Case &c) -> OnnxTestModel::Bytes
{
	using namespace OnnxTestModel;
	onnx_NodeProto conv = onnx_NodeProto_init_zero;
	auto op = text("Conv");
	Fields in{Bytes{'x'}, Bytes{'w'}, Bytes{'b'}}, out{text("z")};
	Fields attrs{attribute("strides", {c.op.strides[0], c.op.strides[1]}),
		     attribute("dilations", {c.op.dilations[0], c.op.dilations[1]}),
		     attribute("pads",
			       {c.op.startPadding[0], c.op.startPadding[1], c.op.endPadding[0], c.op.endPadding[1]}),
		     attribute("group", {c.op.groupCount}, true)};
	bind(conv.op_type, op);
	bind(conv.input, in);
	bind(conv.output, out);
	bind(conv.attribute, attrs);
	Fields nodes{encode(onnx_NodeProto_fields, &conv)};
	onnx_NodeProto relu = onnx_NodeProto_init_zero;
	auto rop = text("Relu"), ri = text("z"), ra = text("y");
	bind(relu.op_type, rop);
	bind(relu.input, ri);
	bind(relu.output, ra);
	nodes.push_back(encode(onnx_NodeProto_fields, &relu));
	Fields weights{initializer<float>("w", {c.filter[0], c.filter[1], c.filter[2], c.filter[3]}, c.weights,
					  onnx_TensorProto_DataType_FLOAT),
		       initializer<float>("b", {c.op.shape[1]}, c.bias, onnx_TensorProto_DataType_FLOAT)};
	onnx_ModelProto model = onnx_ModelProto_init_zero;
	model.has_ir_version = true;
	model.ir_version = 9;
	model.has_graph = true;
	auto name = text("isolated-real-conv-relu");
	bind(model.graph.name, name);
	bind(model.graph.node, nodes);
	Fields vi{valueInfo("x", c.input)}, vo{valueInfo("y", c.op.shape)};
	bind(model.graph.input, vi);
	bind(model.graph.output, vo);
	bind(model.graph.initializer, weights);
	onnx_OperatorSetIdProto set = onnx_OperatorSetIdProto_init_zero;
	set.has_version = true;
	set.version = 14;
	Fields sets{encode(onnx_OperatorSetIdProto_fields, &set)};
	bind(model.opset_import, sets);
	return encode(onnx_ModelProto_fields, &model);
}
struct Native {
	ComPtr<IDMLCompiledOperator> op;
	ComPtr<ID3D12DescriptorHeap> heap;
	ComPtr<IDMLBindingTable> table;
	ComPtr<ID3D12Resource> temp, persistent;
	Native(Gpu &g, const Case &c, const std::array<ComPtr<ID3D12Resource>, 3> &inputs, ID3D12Resource *output,
	       bool disable)
	{
		Tensor x(c.input, false), w(c.filter, true), b({1, c.op.shape[1], 1, 1}, true), y(c.op.shape, false);
		DML_CONVOLUTION_OPERATOR_DESC d{};
		d.InputTensor = &x.desc;
		d.FilterTensor = &w.desc;
		d.BiasTensor = &b.desc;
		d.OutputTensor = &y.desc;
		d.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
		d.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
		d.DimensionCount = 2;
		d.Strides = c.op.strides.data();
		d.Dilations = c.op.dilations.data();
		d.StartPadding = c.op.startPadding.data();
		d.EndPadding = c.op.endPadding.data();
		const std::array<UINT, 2> zero{};
		d.OutputPadding = zero.data();
		d.GroupCount = c.op.groupCount;
		DML_ACTIVATION_RELU_OPERATOR_DESC relu{};
		DML_OPERATOR_DESC activation{DML_OPERATOR_ACTIVATION_RELU, &relu};
		d.FusedActivation = &activation;
		DML_OPERATOR_DESC od{DML_OPERATOR_CONVOLUTION, &d};
		ComPtr<IDMLOperator> raw;
		check(g.dml->CreateOperator(&od, IID_PPV_ARGS(&raw)));
		check(g.dml->CompileOperator(
			raw.Get(), disable ? DML_EXECUTION_FLAG_DISABLE_META_COMMANDS : DML_EXECUTION_FLAG_NONE,
			IID_PPV_ARGS(&op)));
		IDMLCompiledOperator *ops[]{op.Get()};
		ComPtr<IDMLOperatorInitializer> init;
		check(g.dml->CreateOperatorInitializer(1, ops, IID_PPV_ARGS(&init)));
		auto ip = init->GetBindingProperties(), ep = op->GetBindingProperties();
		D3D12_DESCRIPTOR_HEAP_DESC hd{};
		hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hd.NumDescriptors = std::max<UINT>(1, std::max(ip.RequiredDescriptorCount, ep.RequiredDescriptorCount));
		check(g.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
		DML_BINDING_TABLE_DESC td{init.Get(), heap->GetCPUDescriptorHandleForHeapStart(),
					  heap->GetGPUDescriptorHandleForHeapStart(), hd.NumDescriptors};
		check(g.dml->CreateBindingTable(&td, IID_PPV_ARGS(&table)));
		temp = buffer(g.device.Get(), std::max(ip.TemporaryResourceSize, ep.TemporaryResourceSize),
			      D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		persistent = buffer(g.device.Get(), ep.PersistentResourceSize, D3D12_HEAP_TYPE_DEFAULT,
				    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		DML_BUFFER_BINDING tb{temp.Get(), 0, ip.TemporaryResourceSize},
			pb{persistent.Get(), 0, ep.PersistentResourceSize};
		DML_BINDING_DESC tbd{DML_BINDING_TYPE_BUFFER, &tb}, pbd{DML_BINDING_TYPE_BUFFER, &pb},
			none{DML_BINDING_TYPE_NONE, nullptr};
		if (ip.TemporaryResourceSize)
			table->BindTemporaryResource(&tbd);
		std::array<DML_BUFFER_BINDING, 3> ib{{{nullptr, 0, 0},
						      {inputs[1].Get(), 0, w.buffer.TotalTensorSizeInBytes},
						      {inputs[2].Get(), 0, b.buffer.TotalTensorSizeInBytes}}};
		DML_BUFFER_ARRAY_BINDING arr{3, ib.data()};
		DML_BINDING_DESC abd{DML_BINDING_TYPE_BUFFER_ARRAY, &arr};
		table->BindInputs(1, &abd);
		table->BindOutputs(1, ep.PersistentResourceSize ? &pbd : &none);
		g.begin();
		ID3D12DescriptorHeap *heaps[]{heap.Get()};
		g.commands->SetDescriptorHeaps(1, heaps);
		g.recorder->RecordDispatch(g.commands.Get(), init.Get(), table.Get());
		uav(g.commands.Get());
		g.submit();
		td.Dispatchable = op.Get();
		check(table->Reset(&td));
		tb.SizeInBytes = ep.TemporaryResourceSize;
		if (ep.TemporaryResourceSize)
			table->BindTemporaryResource(&tbd);
		if (ep.PersistentResourceSize)
			table->BindPersistentResource(&pbd);
		ib[0] = {inputs[0].Get(), 0, x.buffer.TotalTensorSizeInBytes};
		std::array<DML_BINDING_DESC, 3> bindings{{{DML_BINDING_TYPE_BUFFER, &ib[0]}, none, none}};
		table->BindInputs(3, bindings.data());
		DML_BUFFER_BINDING ob{output, 0, y.buffer.TotalTensorSizeInBytes};
		DML_BINDING_DESC obd{DML_BINDING_TYPE_BUFFER, &ob};
		table->BindOutputs(1, &obd);
	}
	void record(Gpu &g)
	{
		ID3D12DescriptorHeap *heaps[]{heap.Get()};
		g.commands->SetDescriptorHeaps(1, heaps);
		g.recorder->RecordDispatch(g.commands.Get(), op.Get(), table.Get());
	}
};
const char shader[] = R"(
RWByteAddressBuffer input:register(u0);RWByteAddressBuffer weights:register(u1);RWByteAddressBuffer bias:register(u2);RWByteAddressBuffer output:register(u3);
#ifdef STRIP
// Independent vertical strips: adjacent lanes cover adjacent x coordinates.
// Rolling rows stay thread-local; every raw load uses a clamped address.
float3 loadRow(uint channel, uint ox, int y) {
 float3 row;
 [unroll]for(uint kx=0;kx<3;++kx){
  int x=int(ox)+int(kx)-PX;
  int cx=clamp(x,0,W-1),cy=clamp(y,0,H-1);
  float v=asfloat(input.Load(4*((channel*H+cy)*W+cx)));
  row[kx]=(x>=0&&x<W&&y>=0&&y<H)?v:0;
 }
 return row;
}
[numthreads(THREADS,1,1)]void main(uint3 id:SV_DispatchThreadID){
 const uint rows=(OH+STRIP-1)/STRIP;
 uint i=id.x;if(i>=C*rows*OW)return;
 uint channel=i/(rows*OW),ox=i%OW,oy=((i/OW)%rows)*STRIP;
 float3 w0,w1,w2;
 [unroll]for(uint k=0;k<3;++k){
  w0[k]=asfloat(weights.Load(4*(channel*9+k)));
  w1[k]=asfloat(weights.Load(4*(channel*9+3+k)));
  w2[k]=asfloat(weights.Load(4*(channel*9+6+k)));
 }
 float b=asfloat(bias.Load(4*channel));
 float3 r0=loadRow(channel,ox,int(oy)-PY);
 float3 r1=loadRow(channel,ox,int(oy)+1-PY);
 [unroll]for(uint n=0;n<STRIP;++n){
  float3 r2=loadRow(channel,ox,int(oy+n)+2-PY);
  precise float sum=0;
  [unroll]for(uint k0=0;k0<3;++k0)sum=sum+r0[k0]*w0[k0];
  [unroll]for(uint k1=0;k1<3;++k1)sum=sum+r1[k1]*w1[k1];
  [unroll]for(uint k2=0;k2<3;++k2)sum=sum+r2[k2]*w2[k2];
  if(oy+n<OH)output.Store(4*((channel*OH+oy+n)*OW+ox),asuint(max(sum+b,0)));
  r0=r1;r1=r2;
 }
}
#elif defined(TILE)
#define IW ((TX-1)*SX+(KW-1)*DX+1)
#define IH ((TY-1)*SY+(KH-1)*DY+1)
groupshared float tile[IW*IH];
[numthreads(TX,TY,1)]void main(uint3 gid:SV_GroupID,uint3 tid:SV_GroupThreadID,uint idx:SV_GroupIndex){
 uint channel=gid.z;int left=int(gid.x*TX*SX)-PX,top=int(gid.y*TY*SY)-PY;
 for(uint j=idx;j<IW*IH;j+=TX*TY){int x=left+int(j%IW),y=top+int(j/IW);int cx=clamp(x,0,W-1),cy=clamp(y,0,H-1);float value=asfloat(input.Load(4*(channel*H*W+cy*W+cx)));tile[j]=(x>=0&&x<W&&y>=0&&y<H)?value:0;}
 GroupMemoryBarrierWithGroupSync();uint ox=gid.x*TX+tid.x,oy=gid.y*TY+tid.y;if(ox>=OW||oy>=OH)return;
 precise float sum=0;[unroll]for(uint ky=0;ky<KH;++ky)[unroll]for(uint kx=0;kx<KW;++kx)sum=sum+tile[(tid.y*SY+ky*DY)*IW+tid.x*SX+kx*DX]*asfloat(weights.Load(4*((channel*KH+ky)*KW+kx)));
 output.Store(4*((channel*OH+oy)*OW+ox),asuint(max(sum+asfloat(bias.Load(4*(channel))),0)));
}
#else
[numthreads(THREADS,1,1)]void main(uint3 id:SV_DispatchThreadID){uint i=id.x;if(i>=C*OH*OW)return;uint channel=i/(OH*OW),oy=(i/OW)%OH,ox=i%OW;precise float sum=0;
 [unroll]for(uint ky=0;ky<KH;++ky)[unroll]for(uint kx=0;kx<KW;++kx){int x=int(ox*SX+kx*DX)-PX,y=int(oy*SY+ky*DY)-PY;int cx=clamp(x,0,W-1),cy=clamp(y,0,H-1);float value=asfloat(input.Load(4*((channel*H+cy)*W+cx)));if(x>=0&&x<W&&y>=0&&y<H)sum=sum+value*asfloat(weights.Load(4*((channel*KH+ky)*KW+kx)));}
 output.Store(4*(i),asuint(max(sum+asfloat(bias.Load(4*(channel))),0)));
}
#endif
)";
struct Custom {
	std::string name;
	UINT tx, ty;
	bool tiled;
	UINT strip;
	ComPtr<ID3D12RootSignature> root;
	ComPtr<ID3D12PipelineState> pso;
	Custom(Gpu &g, const Case &c, UINT x, UINT y, bool tile, UINT outputs = 1)
		: name(tile ? "tile-" + std::to_string(x) + "x" + std::to_string(y) : "direct-" + std::to_string(x)),
		  tx(x),
		  ty(y),
		  tiled(tile),
		  strip(outputs)
	{
		std::vector<std::pair<std::string, std::string>> definitions{
			{"W", std::to_string(c.input[3])},
			{"H", std::to_string(c.input[2])},
			{"C", std::to_string(c.input[1])},
			{"OW", std::to_string(c.op.shape[3])},
			{"OH", std::to_string(c.op.shape[2])},
			{"KW", std::to_string(c.filter[3])},
			{"KH", std::to_string(c.filter[2])},
			{"SX", std::to_string(c.op.strides[1])},
			{"SY", std::to_string(c.op.strides[0])},
			{"DX", std::to_string(c.op.dilations[1])},
			{"DY", std::to_string(c.op.dilations[0])},
			{"PX", std::to_string(c.op.startPadding[1])},
			{"PY", std::to_string(c.op.startPadding[0])},
			{"TX", std::to_string(tx)},
			{"TY", std::to_string(ty)},
			{"THREADS", std::to_string(tx)}};
		if (strip > 1) {
			name = "strip-" + std::to_string(strip) + "-" + std::to_string(tx);
			definitions.emplace_back("STRIP", std::to_string(strip));
		}
		if (tile)
			definitions.emplace_back("TILE", "1");
		std::vector<D3D_SHADER_MACRO> macros;
		for (auto &[k, v] : definitions)
			macros.push_back({k.c_str(), v.c_str()});
		macros.push_back({nullptr, nullptr});
		ComPtr<ID3DBlob> code, errors;
		auto hr = D3DCompile(shader, sizeof(shader) - 1, "IsolatedDepthwiseRelu", macros.data(), nullptr,
				     "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS, 0,
				     &code, &errors);
		if (FAILED(hr) && errors)
			std::cerr << static_cast<const char *>(errors->GetBufferPointer());
		check(hr);
		std::array<D3D12_ROOT_PARAMETER, 4> params{};
		for (UINT i = 0; i < 4; ++i) {
			params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			params[i].Descriptor.ShaderRegister = i;
		}
		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = 4;
		desc.pParameters = params.data();
		ComPtr<ID3DBlob> sig;
		check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &errors));
		check(g.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
						    IID_PPV_ARGS(&root)));
		D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
		d.pRootSignature = root.Get();
		d.CS = {code->GetBufferPointer(), code->GetBufferSize()};
		check(g.device->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso)));
	}
	void record(Gpu &g, const Case &c, const std::array<ComPtr<ID3D12Resource>, 3> &in, ID3D12Resource *out)
	{
		g.commands->SetComputeRootSignature(root.Get());
		g.commands->SetPipelineState(pso.Get());
		for (UINT i = 0; i < 3; ++i)
			g.commands->SetComputeRootUnorderedAccessView(i, in[i]->GetGPUVirtualAddress());
		g.commands->SetComputeRootUnorderedAccessView(3, out->GetGPUVirtualAddress());
		if (tiled)
			g.commands->Dispatch((c.op.shape[3] + tx - 1) / tx, (c.op.shape[2] + ty - 1) / ty,
					     c.op.shape[1]);
		else {
			const size_t threads =
				size_t(c.op.shape[1]) * ((c.op.shape[2] + strip - 1) / strip) * c.op.shape[3];
			g.commands->Dispatch(UINT((threads + tx - 1) / tx), 1, 1);
		}
	}
};
void upload(Gpu &g, ID3D12Resource *dst, std::span<const float> data)
{
	auto u = buffer(g.device.Get(), data.size_bytes(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void *p;
	check(u->Map(0, nullptr, &p));
	std::memcpy(p, data.data(), data.size_bytes());
	u->Unmap(0, nullptr);
	g.begin();
	transition(g.commands.Get(), dst, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
	g.commands->CopyBufferRegion(dst, 0, u.Get(), 0, data.size_bytes());
	transition(g.commands.Get(), dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	g.submit();
}
void dumpValues(const std::filesystem::path &path, std::span<const float> values)
{
	std::ofstream file(path, std::ios::binary);
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.write(reinterpret_cast<const char *>(values.data()), values.size_bytes());
}
void runCase(Gpu &g, const Case &c, bool verify, bool custom, std::ofstream &csv, std::ofstream &manifest,
	     const std::filesystem::path &directory, bool dump, const std::filesystem::path &sample)
{
	using namespace winrt::Windows::AI::MachineLearning;
	using namespace winrt::Windows::Storage::Streams;
	std::array<ComPtr<ID3D12Resource>, 3> in{buffer(g.device.Get(), count(c.input) * 4, D3D12_HEAP_TYPE_DEFAULT,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
						 buffer(g.device.Get(), c.weights.size() * 4, D3D12_HEAP_TYPE_DEFAULT,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
						 buffer(g.device.Get(), c.bias.size() * 4, D3D12_HEAP_TYPE_DEFAULT,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
	auto out = buffer(g.device.Get(), count(c.op.shape) * 4, D3D12_HEAP_TYPE_DEFAULT,
			  D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
	     readback = buffer(g.device.Get(), count(c.op.shape) * 4, D3D12_HEAP_TYPE_READBACK,
			       D3D12_RESOURCE_STATE_COPY_DEST);
	upload(g, in[1].Get(), c.weights);
	upload(g, in[2].Get(), c.bias);
	Native normal(g, c, in, out.Get(), false), disabled(g, c, in, out.Get(), true);
	bool depthwise = c.op.groupCount == c.input[1] && c.op.shape[1] == c.input[1] && c.filter[1] == 1;
	std::vector<Custom> cs;
	if (depthwise && custom) {
		for (UINT t : {64u, 128u, 256u})
			cs.emplace_back(g, c, t, 1, false);
		cs.emplace_back(g, c, 16, 4, true);
		cs.emplace_back(g, c, 16, 8, true);
		cs.emplace_back(g, c, 32, 8, true);
		if (c.filter[2] == 3 && c.filter[3] == 3 && c.op.strides == std::array<UINT, 2>{1, 1} &&
		    c.op.dilations == std::array<UINT, 2>{1, 1})
			for (UINT outputs : {2u, 4u, 8u})
				for (UINT threads : {64u, 128u, 256u})
					cs.emplace_back(g, c, threads, 1, false, outputs);
	}
	const UINT methods = 2 + UINT(cs.size());
	auto record = [&](UINT m) {
		if (m == 0)
			normal.record(g);
		else if (m == 1)
			disabled.record(g);
		else
			cs[m - 2].record(g, c, in, out.Get());
	};
	auto label = [&](UINT m) -> std::string {
		return m == 0 ? "dml" : m == 1 ? "dml-no-meta" : cs[m - 2].name;
	};
	const auto bytes = onnx(c);
	const InMemoryRandomAccessStream stream;
	const DataWriter writer(stream);
	writer.WriteBytes(bytes);
	writer.StoreAsync().get();
	writer.DetachStream();
	stream.Seek(0);
	const auto model = LearningModel::LoadFromStream(RandomAccessStreamReference::CreateFromStream(stream));
	const LearningModelSession session(model, LearningModelDevice(LearningModelDeviceKind::Cpu));
	float worst = 0;
	std::vector<float> input(count(c.input));
	for (int scenario = 0; scenario < (sample.empty() ? 3 : 1); ++scenario) {
		for (size_t i = 0; i < input.size(); ++i)
			input[i] = scenario == 0   ? float(int((i * 37 + 11) % 257) - 128) / 131.0F
				   : scenario == 1 ? float(int(i % 7) - 3) * 8.0F
						   : 0.0F;
		if (!sample.empty()) {
			std::ifstream file(sample, std::ios::binary | std::ios::ate);
			file.exceptions(std::ios::badbit | std::ios::failbit);
			if (file.tellg() != static_cast<std::streamoff>(input.size() * sizeof(float)))
				throw std::invalid_argument("Input sample byte size mismatch");
			file.seekg(0);
			file.read(reinterpret_cast<char *>(input.data()), input.size() * sizeof(float));
		}
		upload(g, in[0].Get(), input);
		const LearningModelBinding binding(session);
		binding.Bind(L"x",
			     TensorFloat::CreateFromArray(winrt::single_threaded_vector<int64_t>(
								  {c.input[0], c.input[1], c.input[2], c.input[3]}),
							  input));
		auto reference = session.Evaluate(binding, L"conv-relu")
					 .Outputs()
					 .Lookup(L"y")
					 .as<TensorFloat>()
					 .GetAsVectorView();
		if (reference.Size() != count(c.op.shape))
			throw std::runtime_error("WinML output size");
		const auto prefix =
			directory / ("node" + std::to_string(c.index) + "-scenario" + std::to_string(scenario));
		if (dump) {
			const std::vector<float> expected(reference.begin(), reference.end());
			dumpValues(prefix.string() + "-winml.f32", expected);
			dumpValues(prefix.string() + "-input.f32", input);
		}
		for (UINT m = 0; m < methods; ++m) {
			std::cout << "Verify " << c.index << " scenario " << scenario << " " << label(m) << std::endl;
			g.begin();
			record(m);
			uav(g.commands.Get());
			transition(g.commands.Get(), out.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				   D3D12_RESOURCE_STATE_COPY_SOURCE);
			g.commands->CopyBufferRegion(readback.Get(), 0, out.Get(), 0, count(c.op.shape) * 4);
			transition(g.commands.Get(), out.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
				   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			g.submit();
			void *p;
			check(readback->Map(0, nullptr, &p));
			bool bad = false;
			for (UINT i = 0; i < reference.Size(); ++i) {
				float actual = static_cast<float *>(p)[i], expected = reference.GetAt(i),
				      error = std::abs(actual - expected);
				worst = std::max(worst, error);
				if (!std::isfinite(actual) || error > 2e-5F + 2e-5F * std::abs(expected))
					bad = true;
			}
			std::vector<float> copied;
			if (dump)
				copied.assign(static_cast<float *>(p), static_cast<float *>(p) + reference.Size());
			readback->Unmap(0, nullptr);
			if (dump)
				dumpValues(prefix.string() + "-" + label(m) + ".f32", copied);
			if (bad) {
				std::cerr << "node " << c.index << " " << label(m) << " scenario " << scenario
					  << " max error " << worst << "\n";
				throw std::runtime_error("ConvRelu WinML comparison failed");
			}
		}
	}
	manifest << c.index << ',' << c.input[1] << ',' << c.input[2] << ',' << c.input[3] << ',' << c.op.shape[1]
		 << ',' << c.op.shape[2] << ',' << c.op.shape[3] << ',' << c.filter[2] << ',' << c.filter[3] << ','
		 << c.op.groupCount << ',' << methods << ',' << worst << '\n';
	manifest.flush();
	if (!verify) {
		if (sample.empty())
			for (size_t i = 0; i < input.size(); ++i)
				input[i] = float(int((i * 37 + 11) % 257) - 128) / 131.0F;
		upload(g, in[0].Get(), input);
		constexpr UINT warmup = 10, samples = 50;
		const UINT queries = (warmup + samples) * methods * 2;
		D3D12_QUERY_HEAP_DESC q{};
		q.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		q.Count = queries;
		ComPtr<ID3D12QueryHeap> heap;
		check(g.device->CreateQueryHeap(&q, IID_PPV_ARGS(&heap)));
		auto stamps = buffer(g.device.Get(), UINT64(queries) * 8, D3D12_HEAP_TYPE_READBACK,
				     D3D12_RESOURCE_STATE_COPY_DEST);
		UINT64 frequency;
		check(g.queue->GetTimestampFrequency(&frequency));
		std::vector<std::pair<UINT, UINT>> order;
		g.begin();
		UINT at = 0;
		for (UINT iteration = 0; iteration < warmup + samples; ++iteration)
			for (UINT n = 0; n < methods; ++n) {
				UINT m = (n + iteration) % methods;
				uav(g.commands.Get());
				g.commands->EndQuery(heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, at++);
				record(m);
				g.commands->EndQuery(heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, at++);
				order.emplace_back(iteration, m);
			}
		uav(g.commands.Get());
		g.commands->ResolveQueryData(heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, queries, stamps.Get(), 0);
		g.submit();
		void *p;
		check(stamps->Map(0, nullptr, &p));
		const auto *t = static_cast<const UINT64 *>(p);
		for (size_t j = 0; j < order.size(); ++j) {
			auto [iteration, m] = order[j];
			if (iteration >= warmup)
				csv << c.index << ',' << label(m) << ',' << iteration - warmup << ','
				    << double(t[j * 2 + 1] - t[j * 2]) * 1000.0 / double(frequency) << '\n';
		}
		stamps->Unmap(0, nullptr);
		csv.flush();
	}
	std::cout << "Node " << c.index << " validated " << methods << " implementations, max abs " << worst << "\n";
}
int run(int argc, char **argv)
{
	bool verify = false, warp = false, custom = false, dump = false;
	int selected = -1;
	std::filesystem::path directory = ".", sample;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--input" && i + 1 < argc)
			sample = argv[++i];
		else if (a == "--dump")
			dump = true;
		else if (a == "--warp")
			warp = true;
		else if (a == "--custom")
			custom = true;
		else if (a == "--verify")
			verify = true;
		else if (a == "--node" && i + 1 < argc)
			selected = std::stoi(argv[++i]);
		else if (a == "--output" && i + 1 < argc)
			directory = argv[++i];
		else
			throw std::invalid_argument("Unknown ConvRelu benchmark option");
	}
	if (!sample.empty() && selected < 0)
		throw std::invalid_argument("--input requires --node");
	std::filesystem::create_directories(directory);
	std::ofstream csv(directory / "conv-relu-samples.csv"), manifest(directory / "conv-relu-cases.csv");
	csv.exceptions(std::ios::badbit | std::ios::failbit);
	manifest.exceptions(std::ios::badbit | std::ios::failbit);
	csv << std::setprecision(10) << "node,method,iteration,gpu_ms\n";
	manifest << "node,ic,ih,iw,oc,oh,ow,kh,kw,groups,implementations,max_abs_error\n";
	std::cout << (warp ? "WARP software validation" : "Hardware DirectML experiment") << std::endl;
	Gpu g(warp);
	size_t tested = 0;
	for (const auto &c : cases())
		if (selected < 0 || int(c.index) == selected) {
			runCase(g, c, verify, custom, csv, manifest, directory, dump, sample);
			++tested;
		}
	if (!tested)
		throw std::invalid_argument("No matching ConvRelu node");
	return 0;
}
} // namespace ConvReluExperiment
int main(int argc, char **argv)
{
	try {
		winrt::init_apartment();
		return ConvReluExperiment::run(argc, argv);
	} catch (const winrt::hresult_error &e) {
		std::cerr << winrt::to_string(e.message()) << '\n';
		return 1;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
