// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in diagnostic: isolated Conv descriptors; no production runtime changes.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <DirectML.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace ConvMetaCommandProbe {
void check(HRESULT hr)
{
	if (FAILED(hr)) {
		std::cerr << "HRESULT=0x" << std::hex << static_cast<unsigned>(hr) << std::dec << "\n";
		throw std::runtime_error("D3D/DirectML failure");
	}
}
auto buffer(ID3D12Device *device, UINT64 bytes, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state)
	-> ComPtr<ID3D12Resource>
{
	D3D12_HEAP_PROPERTIES hp{};
	hp.Type = heap;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = std::max<UINT64>(bytes, 4);
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if (heap == D3D12_HEAP_TYPE_DEFAULT)
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ComPtr<ID3D12Resource> result;
	check(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&result)));
	return result;
}
struct Tensor {
	std::array<UINT, 4> sizes, strides;
	DML_BUFFER_TENSOR_DESC buffer{};
	DML_TENSOR_DESC desc{};
	Tensor(std::array<UINT, 4> shape, bool half, bool nhwc, bool owned, UINT alignment) : sizes(shape)
	{
		strides = nhwc ? std::array<UINT, 4>{shape[1] * shape[2] * shape[3], 1, shape[3] * shape[1], shape[1]}
			       : std::array<UINT, 4>{shape[1] * shape[2] * shape[3], shape[2] * shape[3], shape[3], 1};
		buffer.DataType = half ? DML_TENSOR_DATA_TYPE_FLOAT16 : DML_TENSOR_DATA_TYPE_FLOAT32;
		buffer.Flags = owned ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		buffer.DimensionCount = 4;
		buffer.Sizes = sizes.data();
		buffer.Strides = strides.data();
		buffer.TotalTensorSizeInBytes =
			(UINT64(shape[0]) * shape[1] * shape[2] * shape[3] * (half ? 2 : 4) + 3) & ~UINT64(3);
		buffer.GuaranteedBaseOffsetAlignment = alignment;
		desc = {DML_TENSOR_TYPE_BUFFER, &buffer};
	}
};
int enumerateMetaCommands()
{
	ComPtr<ID3D12Device5> device;
	check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	UINT count = 0;
	check(device->EnumerateMetaCommands(&count, nullptr));
	std::wcout << L"MetaCommand count: " << count << L"\n";
	std::vector<D3D12_META_COMMAND_DESC> commands(count);
	check(device->EnumerateMetaCommands(&count, commands.data()));
	for (const auto &command : commands) {
		const auto &g = command.Id;
		std::wcout << L"COMMAND " << command.Name << L" GUID " << std::hex << g.Data1 << L"-" << g.Data2 << L"-"
			   << g.Data3 << L"-";
		for (auto b : g.Data4)
			std::wcout << unsigned(b) << L":";
		std::wcout << std::dec << L"\n";
		for (auto stage :
		     {D3D12_META_COMMAND_PARAMETER_STAGE_CREATION, D3D12_META_COMMAND_PARAMETER_STAGE_INITIALIZATION,
		      D3D12_META_COMMAND_PARAMETER_STAGE_EXECUTION}) {
			UINT bytes = 0, parameters = 0;
			check(device->EnumerateMetaCommandParameters(command.Id, stage, &bytes, &parameters, nullptr));
			std::vector<D3D12_META_COMMAND_PARAMETER_DESC> descs(parameters);
			check(device->EnumerateMetaCommandParameters(command.Id, stage, &bytes, &parameters,
								     descs.data()));
			std::wcout << L" STAGE " << unsigned(stage) << L" bytes " << bytes << L" count " << parameters
				   << L"\n";
			for (const auto &d : descs)
				std::wcout << L"  " << d.Name << L" type " << unsigned(d.Type) << L" offset "
					   << d.StructureOffset << L" flags " << unsigned(d.Flags) << L" state "
					   << unsigned(d.RequiredResourceState) << L"\n";
		}
	}
	return 0;
}
// Exploratory ABI probe. Field offsets/types come from the installed driver's
// public enumeration. Enum candidates follow published NVAPI ML definitions;
// uniform-type controls must succeed before interpreting mixed-type failures.
int probeMetaCreation(bool staticSweep = false)
{
	ComPtr<ID3D12Device5> device;
	check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	UINT count = 0;
	check(device->EnumerateMetaCommands(&count, nullptr));
	std::vector<D3D12_META_COMMAND_DESC> commands(count);
	check(device->EnumerateMetaCommands(&count, commands.data()));
	for (const auto &command : commands) {
		bool gemm = std::wstring_view(command.Name).find(L"GEMM (") == 0;
		if (!gemm && std::wstring_view(command.Name).find(L"Conv (") != 0)
			continue;
		UINT bytes = 0, count = 0;
		check(device->EnumerateMetaCommandParameters(command.Id, D3D12_META_COMMAND_PARAMETER_STAGE_CREATION,
							     &bytes, &count, nullptr));
		std::vector<D3D12_META_COMMAND_PARAMETER_DESC> fields(count);
		check(device->EnumerateMetaCommandParameters(command.Id, D3D12_META_COMMAND_PARAMETER_STAGE_CREATION,
							     &bytes, &count, fields.data()));
		bool modern = std::any_of(fields.begin(), fields.end(),
					  [](auto &f) { return std::wstring_view(f.Name) == L"OutputDesc.DataType"; });
		std::cout
			<< "COMMAND," << (gemm ? "GEMM" : "Conv") << (modern ? "-v2" : "-v1")
			<< "\nshape,layout,input_type,filter_type,bias_type,output_type,precision,bind_flags,static_weight,bias_null,hresult\n";
		for (unsigned shape = 0; shape < 2; ++shape)
			for (bool nhwc : {false, true})
				for (UINT64 inputType : {0ull, 1ull, 2ull})
					for (UINT64 filterType : {0ull, 1ull, 2ull})
						for (UINT64 biasType : {0ull, 1ull, 2ull})
							for (UINT64 outputType : {0ull, 1ull, 2ull})
								for (UINT64 precision : {0ull, 1ull, 2ull})
									for (UINT64 bind : {0ull, 1ull, 2ull, 3ull})
										for (UINT64 staticWeight : {0ull, 1ull})
											for (UINT64 biasNull :
											     {0ull, 1ull}) {
												if (!staticSweep &&
												    (staticWeight ||
												     biasNull))
													continue;
												if (staticSweep &&
												    (outputType !=
													     inputType ||
												     biasType !=
													     inputType))
													continue;
												std::vector<std::byte>
													data(bytes);
												auto fieldName = [&](std::wstring
															     name) {
													if (!modern)
														return name;
													for (auto pair :
													     {std::pair{
														      L"DescIn",
														      L"InputDesc"},
													      std::pair{
														      L"DescFilter",
														      L"FilterDesc"},
													      std::pair{
														      L"DescBias",
														      L"BiasDesc"},
													      std::pair{
														      L"DescOut",
														      L"OutputDesc"},
													      std::pair{
														      L"DescA",
														      L"ADesc"},
													      std::pair{
														      L"DescB",
														      L"BDesc"},
													      std::pair{
														      L"DescC",
														      L"CDesc"}})
														if (name.starts_with(
															    pair.first))
															return std::wstring(
																       pair.second) +
															       name.substr(
																       std::wstring_view(
																	       pair.first)
																	       .size());
													if (name ==
													    L"TransA")
														name = L"ATransform";
													if (name ==
													    L"TransB")
														name = L"BTransform";
													return L"Attributes." +
													       name;
												};
												auto set = [&](const std::wstring
														       &originalName,
													       UINT64 value) {
													auto name = fieldName(
														originalName);
													auto it = std::find_if(
														fields.begin(),
														fields.end(),
														[&](auto &f) {
															return name ==
															       f.Name;
														});
													if (it == fields.end() ||
													    it->Type !=
														    D3D12_META_COMMAND_PARAMETER_TYPE_UINT64 ||
													    it->StructureOffset +
															    8 >
														    bytes)
														throw std::runtime_error(
															"Unexpected MetaCommand creation field");
													std::memcpy(
														data.data() +
															it->StructureOffset,
														&value,
														8);
												};
												auto tensor = [&](const std::wstring
															  &prefix,
														  UINT64 type,
														  std::array<
															  UINT64,
															  4>
															  size) {
													set(prefix +
														    L".DataType",
													    type);
													if (!modern)
														set(prefix +
															    L".Layout",
														    1);
													set(prefix +
														    L".DimensionCount",
													    4);
													if (modern) {
														set(prefix +
															    L".PhysicalSizeInElements",
														    size[0] *
															    size[1] *
															    size[2] *
															    size[3]);
														set(prefix +
															    L".BaseAlignmentInBytes",
														    16);
														for (unsigned d =
															     0;
														     d <
														     5;
														     ++d)
															set(prefix +
																    L".StrideAlignment[" +
																    std::to_wstring(
																	    d) +
																    L"]",
															    1);
													}
													std::array<UINT64, 4> stride =
														nhwc ? std::array<
															       UINT64,
															       4>{size[1] *
																	  size[2] *
																	  size[3],
																  1,
																  size[3] *
																	  size[1],
																  size[1]}
														     : std::array<
															       UINT64,
															       4>{
															       size[1] *
																       size[2] *
																       size[3],
															       size[2] *
																       size[3],
															       size[3],
															       1};
													for (unsigned d =
														     0;
													     d < 4;
													     ++d) {
														set(prefix +
															    L".Size[" +
															    std::to_wstring(
																    d) +
															    L"]",
														    size[d]);
														set(prefix +
															    L".Stride[" +
															    std::to_wstring(
																    d) +
															    L"]",
														    stride[d]);
													}
												};
												if (gemm) {
													if (nhwc)
														continue;
													UINT64 m = shape ? 576
															 : 8,
													       k = shape ? 128
															 : 16,
													       n = shape ? 24
															 : 8;
													tensor(L"DescA",
													       inputType,
													       {1, 1, m,
														k});
													tensor(L"DescB",
													       filterType,
													       {1, 1, k,
														n});
													tensor(L"DescC",
													       biasType,
													       {1, 1, m,
														n});
													tensor(L"DescOut",
													       outputType,
													       {1, 1, m,
														n});
													set(L"DescC.IsNull",
													    biasNull);
													set(L"DescB.Flags",
													    staticWeight);
													auto alpha = std::find_if(
														fields.begin(),
														fields.end(),
														[&](auto &f) {
															return std::wstring_view(
																       f.Name) ==
															       fieldName(
																       L"Alpha");
														});
													if (alpha ==
														    fields.end() ||
													    alpha->Type !=
														    D3D12_META_COMMAND_PARAMETER_TYPE_FLOAT)
														throw std::runtime_error(
															"Unexpected Alpha");
													float one = 1;
													std::memcpy(
														data.data() +
															alpha->StructureOffset,
														&one,
														4);
												} else {
													UINT64 ic = shape ? 128
															  : 8,
													       oc = shape ? 24
															  : 8,
													       h = shape ? 18
															 : 4,
													       w = shape ? 32
															 : 4;
													tensor(L"DescIn",
													       inputType,
													       {1, ic,
														h, w});
													tensor(L"DescFilter",
													       filterType,
													       {oc, ic,
														1, 1});
													tensor(L"DescOut",
													       outputType,
													       {1, oc,
														h, w});
													tensor(L"DescBias",
													       biasType,
													       {1, oc,
														1, 1});
													set(L"DescBias.IsNull",
													    biasNull);
													set(L"DescFilter.Flags",
													    staticWeight);
													set(L"Mode", 1);
													set(L"Direction",
													    0);
													for (unsigned d =
														     0;
													     d < 3;
													     ++d) {
														set(L"Stride[" +
															    std::to_wstring(
																    d) +
															    L"]",
														    1);
														set(L"Dilation[" +
															    std::to_wstring(
																    d) +
															    L"]",
														    1);
													}
													set(L"DimensionCount",
													    2);
													set(L"GroupCount",
													    1);
												}
												set(L"Precision",
												    precision);
												set(L"Activation.IsNull",
												    1);
												set(L"BindFlags", bind);
												ComPtr<ID3D12MetaCommand>
													meta;
												HRESULT hr = device->CreateMetaCommand(
													command.Id, 0,
													data.data(),
													data.size(),
													IID_PPV_ARGS(
														&meta));
												std::cout
													<< shape << ","
													<< nhwc << ","
													<< inputType
													<< ","
													<< filterType
													<< ","
													<< biasType
													<< ","
													<< outputType
													<< ","
													<< precision
													<< "," << bind
													<< ","
													<< staticWeight
													<< ","
													<< biasNull
													<< ",0x"
													<< std::hex
													<< static_cast<
														   unsigned>(
														   hr)
													<< std::dec
													<< "\n";
											}
	}
	return 0;
}
int probeDirectMlTypes()
{
	ComPtr<ID3D12Device> device;
	check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	ComPtr<IDMLDevice> dml;
	check(DMLCreateDevice(device.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&dml)));
	std::cout << "input_fp16,filter_fp16,bias_fp16,output_fp16,create_hr,compile_hr\n";
	for (bool i : {false, true})
		for (bool w : {false, true})
			for (bool b : {false, true})
				for (bool o : {false, true}) {
					Tensor input({1, 8, 4, 4}, i, false, false, 0),
						filter({8, 8, 1, 1}, w, false, false, 0),
						bias({1, 8, 1, 1}, b, false, false, 0),
						output({1, 8, 4, 4}, o, false, false, 0);
					std::array<UINT, 2> one{1, 1}, zero{};
					DML_CONVOLUTION_OPERATOR_DESC c{};
					c.InputTensor = &input.desc;
					c.FilterTensor = &filter.desc;
					c.BiasTensor = &bias.desc;
					c.OutputTensor = &output.desc;
					c.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
					c.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
					c.DimensionCount = 2;
					c.Strides = one.data();
					c.Dilations = one.data();
					c.StartPadding = zero.data();
					c.EndPadding = zero.data();
					c.OutputPadding = zero.data();
					c.GroupCount = 1;
					DML_OPERATOR_DESC desc{DML_OPERATOR_CONVOLUTION, &c};
					ComPtr<IDMLOperator> op;
					HRESULT hr = dml->CreateOperator(&desc, IID_PPV_ARGS(&op)),
						compiled = E_PENDING;
					if (SUCCEEDED(hr)) {
						ComPtr<IDMLCompiledOperator> result;
						compiled = dml->CompileOperator(
							op.Get(),
							i ? DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION
							  : DML_EXECUTION_FLAG_NONE,
							IID_PPV_ARGS(&result));
					}
					std::cout << i << "," << w << "," << b << "," << o << ",0x" << std::hex
						  << unsigned(hr) << ",0x" << unsigned(compiled) << std::dec << "\n";
				}
	return 0;
}
int run()
{
	ComPtr<ID3D12Device> device;
	check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	ComPtr<IDMLDevice> dml;
	check(DMLCreateDevice(device.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&dml)));
	ComPtr<IDMLCommandRecorder> recorder;
	check(dml->CreateCommandRecorder(IID_PPV_ARGS(&recorder)));
	D3D12_COMMAND_QUEUE_DESC qd{};
	ComPtr<ID3D12CommandQueue> queue;
	check(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));
	ComPtr<ID3D12CommandAllocator> allocator;
	check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
	ComPtr<ID3D12GraphicsCommandList> commands;
	check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
					IID_PPV_ARGS(&commands)));
	check(commands->Close());
	ComPtr<ID3D12Fence> fence;
	check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!event)
		throw std::runtime_error("CreateEvent");
	struct EventClose {
		HANDLE value;
		~EventClose() { CloseHandle(value); }
	} close{event};
	UINT64 fenceValue = 0;
	auto begin = [&] {
		check(allocator->Reset());
		check(commands->Reset(allocator.Get(), nullptr));
	};
	auto submit = [&] {
		check(commands->Close());
		ID3D12CommandList *lists[]{commands.Get()};
		queue->ExecuteCommandLists(1, lists);
		check(queue->Signal(fence.Get(), ++fenceValue));
		if (fence->GetCompletedValue() < fenceValue) {
			check(fence->SetEventOnCompletion(fenceValue, event));
			if (WaitForSingleObject(event, 30000) != WAIT_OBJECT_0)
				throw std::runtime_error("GPU timeout");
		}
		check(device->GetDeviceRemovedReason());
	};
	std::ofstream manifest("probe-cases.csv");
	manifest.exceptions(std::ios::badbit | std::ios::failbit);
	manifest << "case,shape,type,flags,layout,alignment,owned,expected,repetitions\n";
	unsigned caseIndex = 0;
	for (unsigned modelShape = 0; modelShape < 2; ++modelShape)
		for (bool half : {false, true})
			for (unsigned flags : {0u, 1u, 2u})
				for (bool nhwc : {false, true})
					for (UINT alignment : {0u, 16u})
						for (bool owned : {true, false}) {
							if (flags == 2 && (nhwc || alignment || !owned))
								continue;
							UINT channels = modelShape ? 128 : 8,
							     outputs = modelShape ? 24 : 8,
							     height = modelShape ? 18 : 4, width = modelShape ? 32 : 4;
							Tensor input({1, channels, height, width}, half, nhwc, false,
								     alignment),
								filter({outputs, channels, 1, 1}, half, nhwc, owned,
								       alignment),
								bias({1, outputs, 1, 1}, half, nhwc, owned, alignment),
								output({1, outputs, height, width}, half, nhwc, false,
								       alignment);
							const std::array<UINT, 2> one{1, 1}, zero{};
							DML_CONVOLUTION_OPERATOR_DESC conv{};
							conv.InputTensor = &input.desc;
							conv.FilterTensor = &filter.desc;
							conv.BiasTensor = &bias.desc;
							conv.OutputTensor = &output.desc;
							conv.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
							conv.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
							conv.DimensionCount = 2;
							conv.Strides = one.data();
							conv.Dilations = one.data();
							conv.StartPadding = zero.data();
							conv.EndPadding = zero.data();
							conv.OutputPadding = zero.data();
							conv.GroupCount = 1;
							DML_OPERATOR_DESC od{DML_OPERATOR_CONVOLUTION, &conv};
							ComPtr<IDMLOperator> op;
							check(dml->CreateOperator(&od, IID_PPV_ARGS(&op)));
							ComPtr<IDMLCompiledOperator> compiled;
							check(dml->CompileOperator(
								op.Get(), static_cast<DML_EXECUTION_FLAGS>(flags),
								IID_PPV_ARGS(&compiled)));
							IDMLCompiledOperator *ops[]{compiled.Get()};
							ComPtr<IDMLOperatorInitializer> init;
							check(dml->CreateOperatorInitializer(1, ops,
											     IID_PPV_ARGS(&init)));
							auto ip = init->GetBindingProperties(),
							     ep = compiled->GetBindingProperties();
							D3D12_DESCRIPTOR_HEAP_DESC hd{};
							hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
							hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
							hd.NumDescriptors =
								std::max<UINT>(1, std::max(ip.RequiredDescriptorCount,
											   ep.RequiredDescriptorCount));
							ComPtr<ID3D12DescriptorHeap> heap;
							check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
							DML_BINDING_TABLE_DESC td{
								init.Get(), heap->GetCPUDescriptorHandleForHeapStart(),
								heap->GetGPUDescriptorHandleForHeapStart(),
								hd.NumDescriptors};
							ComPtr<IDMLBindingTable> table;
							check(dml->CreateBindingTable(&td, IID_PPV_ARGS(&table)));
							auto temp = buffer(device.Get(),
									   std::max(ip.TemporaryResourceSize,
										    ep.TemporaryResourceSize),
									   D3D12_HEAP_TYPE_DEFAULT,
									   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
							auto persistent = buffer(device.Get(),
										 ep.PersistentResourceSize,
										 D3D12_HEAP_TYPE_DEFAULT,
										 D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
							std::array<ComPtr<ID3D12Resource>, 3> inputs, uploads;
							std::array<Tensor *, 3> tensors{&input, &filter, &bias};
							const float values[]{.25F, .5F, .125F};
							begin();
							for (unsigned i = 0; i < 3; ++i) {
								auto bytes = tensors[i]->buffer.TotalTensorSizeInBytes;
								inputs[i] = buffer(device.Get(), bytes,
										   D3D12_HEAP_TYPE_DEFAULT,
										   D3D12_RESOURCE_STATE_COPY_DEST);
								uploads[i] = buffer(device.Get(), bytes,
										    D3D12_HEAP_TYPE_UPLOAD,
										    D3D12_RESOURCE_STATE_GENERIC_READ);
								void *ptr = nullptr;
								check(uploads[i]->Map(0, nullptr, &ptr));
								for (UINT64 j = 0; j < bytes / (half ? 2 : 4); ++j) {
									if (half)
										static_cast<uint16_t *>(ptr)[j] =
											DirectX::PackedVector::
												XMConvertFloatToHalf(
													values[i]);
									else
										static_cast<float *>(ptr)[j] =
											values[i];
								}
								uploads[i]->Unmap(0, nullptr);
								commands->CopyBufferRegion(inputs[i].Get(), 0,
											   uploads[i].Get(), 0, bytes);
								D3D12_RESOURCE_BARRIER b{};
								b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
								b.Transition = {inputs[i].Get(),
										D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
										D3D12_RESOURCE_STATE_COPY_DEST,
										D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
								commands->ResourceBarrier(1, &b);
							}
							auto out = buffer(device.Get(),
									  output.buffer.TotalTensorSizeInBytes,
									  D3D12_HEAP_TYPE_DEFAULT,
									  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
							auto readback = buffer(device.Get(),
									       output.buffer.TotalTensorSizeInBytes,
									       D3D12_HEAP_TYPE_READBACK,
									       D3D12_RESOURCE_STATE_COPY_DEST);
							DML_BUFFER_BINDING tb{temp.Get(), 0, ip.TemporaryResourceSize},
								pb{persistent.Get(), 0, ep.PersistentResourceSize};
							DML_BINDING_DESC tbd{DML_BINDING_TYPE_BUFFER, &tb},
								pbd{DML_BINDING_TYPE_BUFFER, &pb},
								none{DML_BINDING_TYPE_NONE, nullptr};
							if (ip.TemporaryResourceSize)
								table->BindTemporaryResource(&tbd);
							std::array<DML_BUFFER_BINDING, 3> ib{};
							if (owned) {
								ib[1] = {inputs[1].Get(), 0,
									 filter.buffer.TotalTensorSizeInBytes};
								ib[2] = {inputs[2].Get(), 0,
									 bias.buffer.TotalTensorSizeInBytes};
							}
							DML_BUFFER_ARRAY_BINDING ab{3, ib.data()};
							DML_BINDING_DESC abd{DML_BINDING_TYPE_BUFFER_ARRAY, &ab};
							table->BindInputs(1, &abd);
							table->BindOutputs(1, ep.PersistentResourceSize ? &pbd : &none);
							ID3D12DescriptorHeap *heaps[]{heap.Get()};
							commands->SetDescriptorHeaps(1, heaps);
							recorder->RecordDispatch(commands.Get(), init.Get(),
										 table.Get());
							D3D12_RESOURCE_BARRIER uav{};
							uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
							commands->ResourceBarrier(1, &uav);
							submit();
							td.Dispatchable = compiled.Get();
							check(table->Reset(&td));
							tb.SizeInBytes = ep.TemporaryResourceSize;
							if (ep.TemporaryResourceSize)
								table->BindTemporaryResource(&tbd);
							if (ep.PersistentResourceSize)
								table->BindPersistentResource(&pbd);
							std::array<DML_BINDING_DESC, 3> bindings;
							for (unsigned i = 0; i < 3; ++i) {
								ib[i] = {inputs[i].Get(), 0,
									 tensors[i]->buffer.TotalTensorSizeInBytes};
								bindings[i] = (owned && i > 0)
										      ? none
										      : DML_BINDING_DESC{
												DML_BINDING_TYPE_BUFFER,
												&ib[i]};
							}
							table->BindInputs(3, bindings.data());
							DML_BUFFER_BINDING ob{out.Get(), 0,
									      output.buffer.TotalTensorSizeInBytes};
							DML_BINDING_DESC obd{DML_BINDING_TYPE_BUFFER, &ob};
							table->BindOutputs(1, &obd);
							begin();
							commands->SetDescriptorHeaps(1, heaps);
							for (unsigned repeat = 0; repeat < 3; ++repeat) {
								recorder->RecordDispatch(commands.Get(), compiled.Get(),
											 table.Get());
								commands->ResourceBarrier(1, &uav);
							}
							D3D12_RESOURCE_BARRIER transition{};
							transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
							transition.Transition = {
								out.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
								D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
								D3D12_RESOURCE_STATE_COPY_SOURCE};
							commands->ResourceBarrier(1, &transition);
							commands->CopyBufferRegion(
								readback.Get(), 0, out.Get(), 0,
								output.buffer.TotalTensorSizeInBytes);
							submit();
							void *mapped = nullptr;
							check(readback->Map(0, nullptr, &mapped));
							float expected = channels * .125F + .125F;
							for (UINT64 i = 0; i < UINT64(outputs) * height * width; ++i) {
								float actual =
									half ? DirectX::PackedVector::XMConvertHalfToFloat(
										       static_cast<uint16_t *>(
											       mapped)[i])
									     : static_cast<float *>(mapped)[i];
								if (actual != expected)
									throw std::runtime_error(
										"Conv numeric mismatch");
							}
							readback->Unmap(0, nullptr);
							manifest << caseIndex++ << ","
								 << (modelShape ? "node169" : "minimal") << ","
								 << (half ? "fp16" : "fp32") << "," << flags << ","
								 << (nhwc ? "NHWC" : "NCHW") << "," << alignment << ","
								 << owned << "," << expected << ",3\n";
						}
	std::cout << "Validated " << caseIndex << " isolated Conv cases.\n";
	return 0;
}
} // namespace ConvMetaCommandProbe
int main(int argc, char **argv)
{
	try {
		if (argc == 2 && std::string_view(argv[1]) == "--probe-dml-types")
			return ConvMetaCommandProbe::probeDirectMlTypes();
		if (argc == 2 && std::string_view(argv[1]) == "--enumerate-meta")
			return ConvMetaCommandProbe::enumerateMetaCommands();
		if (argc == 2 && std::string_view(argv[1]) == "--probe-meta-static")
			return ConvMetaCommandProbe::probeMetaCreation(true);
		if (argc == 2 && std::string_view(argv[1]) == "--probe-meta-creation")
			return ConvMetaCommandProbe::probeMetaCreation();
		if (argc != 1) throw std::invalid_argument("Unknown probe option");
		return ConvMetaCommandProbe::run();
	} catch (const std::exception &e) {
		std::cerr << e.what() << "\n";
		return 1;
	}
}
