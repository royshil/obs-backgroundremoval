// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "DmlProgramOperations.hpp"
#include "DmlProgramCompiler.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

#include "Detail.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

using namespace BaselineDml::Definition;

namespace BaselineDml {
namespace Detail {

auto createBuffer(ID3D12Device *device, std::uint64_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state,
		  D3D12_RESOURCE_FLAGS flags) -> ComOwner<ID3D12Resource>
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = heapType;
	D3D12_RESOURCE_DESC description{};
	description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	description.Width = std::max<std::uint64_t>(size, 4);
	description.Height = 1;
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.SampleDesc.Count = 1;
	description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	description.Flags = flags;
	ComOwner<ID3D12Resource> resource;
	ID3D12Resource *resourcePointer = nullptr;
	const HRESULT resourceResult = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state,
								       nullptr, IID_PPV_ARGS(&resourcePointer));
	resource.reset(resourcePointer);
	if (const HRESULT result = resourceResult; FAILED(result))
		throw std::runtime_error(std::string("ID3D12Device::CreateCommittedResource") + " failed (HRESULT " +
					 std::to_string(result) + ")");
	return resource;
}

auto transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
	-> D3D12_RESOURCE_BARRIER
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	return barrier;
}

auto bufferBinding(ID3D12Resource *resource, std::uint64_t offset, std::uint64_t size) -> DML_BUFFER_BINDING
{
	return {resource, offset, size};
}

auto bindingDesc(const DML_BUFFER_BINDING *binding) -> DML_BINDING_DESC
{
	return {binding ? DML_BINDING_TYPE_BUFFER : DML_BINDING_TYPE_NONE, binding};
}

void allocateInternalTensors(ID3D12Device *device, std::span<const InternalTensorDefinition> definitions,
			     InternalTensors &tensors)
{
	for (const auto &definition : definitions)
		tensors.emplace(definition.key, createBuffer(device, definition.spec.size, D3D12_HEAP_TYPE_DEFAULT,
							     D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
							     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
}

} // namespace Detail

using namespace Detail;

namespace Detail {

// Construction-only helper. No definition or operational behavior is stored in DmlProgram.
class ProgramBuilder final {
public:
	ProgramBuilder(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition)
		: definition_(definition),
		  d3dDevice_(device),
		  dmlDevice_(dmlDevice)
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> initialization;
		TraceLoggingWriteStart(initialization, "Program.Prepare");
		if (!(device && dmlDevice))
			throw std::runtime_error("Compilation requires D3D12 and DirectML devices");
		ComOwner<ID3D12Device> owner;
		ID3D12Device *ownerPointer = nullptr;
		const HRESULT ownerResult = dmlDevice_->GetParentDevice(IID_PPV_ARGS(&ownerPointer));
		owner.reset(ownerPointer);
		if (const HRESULT result = ownerResult; FAILED(result))
			throw std::runtime_error(std::string("Get DirectML parent device") + " failed (HRESULT " +
						 std::to_string(result) + ")");
		if (!(owner.get() == device))
			throw std::runtime_error("DirectML and D3D12 device mismatch");
		IDMLCommandRecorder *recorderPointer = nullptr;
		const HRESULT recorderResult = dmlDevice_->CreateCommandRecorder(IID_PPV_ARGS(&recorderPointer));
		recorder_.reset(recorderPointer);
		if (const HRESULT result = recorderResult; FAILED(result))
			throw std::runtime_error(std::string("CreateCommandRecorder") + " failed (HRESULT " +
						 std::to_string(result) + ")");

		DmlProgramCompiler compiler(dmlDevice_, definition.nodes, definition.params,
					    std::as_bytes(definition.weights));
		steps_ = compiler.takeSteps();
		internalTensors_ = compiler.takeInternalTensors();
		allocateResources(std::as_bytes(definition.weights));
		prepareExecutionTransitions();
	}

	auto takeProgram() -> DmlProgram
	{
		return DmlProgram{std::move(storages_),
				  std::move(internalTensors_),
				  std::move(steps_),
				  std::move(recorder_),
				  std::move(initializationHeap_),
				  std::move(descriptorHeap_),
				  std::move(weightUpload_),
				  std::move(weight_),
				  std::move(inputReadyTransitions_),
				  std::move(inputRestoreTransitions_),
				  std::move(outputTransitions_)};
	}

private:
	void prepareExecutionTransitions()
	{
		// Each execution ends in precisely these starting states, permitting execution-only list replay.
		for (const auto id : definition_.params.inputTensors) {
			inputReadyTransitions_.push_back(transition(resource(id), D3D12_RESOURCE_STATE_COPY_DEST,
								    D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			inputRestoreTransitions_.push_back(transition(
				resource(id), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST));
		}
		outputTransitions_ = {
			transition(resource(definition_.params.outputTensor), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				   D3D12_RESOURCE_STATE_COPY_SOURCE),
			transition(resource(definition_.params.outputTensor), D3D12_RESOURCE_STATE_COPY_SOURCE,
				   D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		};
	}
	auto resource(std::int64_t id) const noexcept -> ID3D12Resource *
	{
		const auto &tensor = definition_.params.tensors[id];
		return isWeightTensor(tensor) ? weight_.get() : storages_[definition_.params.placements[id]].get();
	}

	// SDK-dependent requirements exist only while constructing binding tables.
	struct StepBindings final {
		DML_BINDING_PROPERTIES operationProperties{};
		DML_BINDING_PROPERTIES initializerProperties{};
		std::uint32_t descriptorOffset = 0;
		std::uint32_t descriptorCount = 0;
	};

	void allocateResources(std::span<const std::byte> weights)
	{
		storages_.resize(definition_.params.storages.size());
		for (const auto id : definition_.params.inputTensors) {
			const auto index = definition_.params.placements[id];
			storages_[index] = createBuffer(d3dDevice_, definition_.params.storages[index].size,
							D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
		for (std::size_t index = 0; index < storages_.size(); ++index) {
			if (!storages_[index])
				storages_[index] = createBuffer(d3dDevice_, definition_.params.storages[index].size,
								D3D12_HEAP_TYPE_DEFAULT,
								D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
								D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
		if (definition_.params.weightBytes) {
			weightUpload_ = createBuffer(d3dDevice_, definition_.params.weightBytes, D3D12_HEAP_TYPE_UPLOAD,
						     D3D12_RESOURCE_STATE_GENERIC_READ);
			weight_ = createBuffer(d3dDevice_, definition_.params.weightBytes, D3D12_HEAP_TYPE_DEFAULT,
					       D3D12_RESOURCE_STATE_COPY_DEST,
					       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			void *mapped = nullptr;
			if (const HRESULT result = weightUpload_->Map(0, nullptr, &mapped); FAILED(result))
				throw std::runtime_error(std::string("Map generated weight upload buffer") +
							 " failed (HRESULT " + std::to_string(result) + ")");
			std::memcpy(mapped, weights.data(), weights.size());
			weightUpload_->Unmap(0, nullptr);
		}

		std::vector<StepBindings> bindings(steps_.size());
		std::uint32_t descriptorCount = 0;
		for (std::size_t index = 0; index < steps_.size(); ++index) {
			auto &step = steps_[index];
			auto &binding = bindings[index];
			const auto &description = definition_.params.steps[index];
			if (description.output == noTensor)
				continue;
			if (description.shader != CompiledShader::None) {
				if (description.shader == CompiledShader::MPLD10DepthwiseConv2dRelu)
					createMPLD10DepthwiseConv2dReluPipeline(step);
				else
					createMPLD10MaskPipeline(step);
				continue;
			}
			binding.operationProperties = step.operation->GetBindingProperties();
			IDMLCompiledOperator *operations[] = {step.operation.get()};
			IDMLOperatorInitializer *initializerPointer = nullptr;
			const HRESULT initializerResult =
				dmlDevice_->CreateOperatorInitializer(1, operations, IID_PPV_ARGS(&initializerPointer));
			step.initializer.reset(initializerPointer);
			if (const HRESULT result = initializerResult; FAILED(result))
				throw std::runtime_error(std::string("CreateOperatorInitializer") +
							 " failed (HRESULT " + std::to_string(result) + ")");
			binding.initializerProperties = step.initializer->GetBindingProperties();
			const auto temporarySize = std::max(binding.operationProperties.TemporaryResourceSize,
							    binding.initializerProperties.TemporaryResourceSize);
			if (temporarySize)
				step.dmlOpaquePointers.temporary =
					createBuffer(d3dDevice_, temporarySize, D3D12_HEAP_TYPE_DEFAULT,
						     D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			if (binding.operationProperties.PersistentResourceSize) {
				step.dmlOpaquePointers.persistent =
					createBuffer(d3dDevice_, binding.operationProperties.PersistentResourceSize,
						     D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			}
			binding.descriptorOffset = descriptorCount;
			binding.descriptorCount =
				std::max<UINT>(1, std::max(binding.operationProperties.RequiredDescriptorCount,
							   binding.initializerProperties.RequiredDescriptorCount));
			if (binding.descriptorCount > std::numeric_limits<std::uint32_t>::max() - descriptorCount)
				throw std::runtime_error("DirectML descriptor count overflow");
			descriptorCount += binding.descriptorCount;
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDescription.NumDescriptors = std::max<UINT>(1, descriptorCount);
		heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ID3D12DescriptorHeap *initializationHeapPointer = nullptr;
		const HRESULT initializationHeapResult =
			d3dDevice_->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&initializationHeapPointer));
		initializationHeap_.reset(initializationHeapPointer);
		if (const HRESULT result = initializationHeapResult; FAILED(result))
			throw std::runtime_error(std::string("Create initialization descriptor heap") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		ID3D12DescriptorHeap *descriptorHeapPointer = nullptr;
		const HRESULT descriptorHeapResult =
			d3dDevice_->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&descriptorHeapPointer));
		descriptorHeap_.reset(descriptorHeapPointer);
		if (const HRESULT result = descriptorHeapResult; FAILED(result))
			throw std::runtime_error(std::string("CreateDescriptorHeap") + " failed (HRESULT " +
						 std::to_string(result) + ")");
		createInitializationTables(bindings);
		createExecutionTables(bindings);
	}

	auto createTable(ID3D12DescriptorHeap *heap, IDMLDispatchable *dispatchable, std::uint32_t descriptorOffset,
			 UINT descriptorCount) -> ComOwner<IDMLBindingTable>
	{
		DML_BINDING_TABLE_DESC description{};
		description.Dispatchable = dispatchable;
		description.CPUDescriptorHandle = heap->GetCPUDescriptorHandleForHeapStart();
		description.CPUDescriptorHandle.ptr +=
			static_cast<SIZE_T>(descriptorOffset) *
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		description.GPUDescriptorHandle = heap->GetGPUDescriptorHandleForHeapStart();
		description.GPUDescriptorHandle.ptr +=
			static_cast<UINT64>(descriptorOffset) *
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		description.SizeInDescriptors = descriptorCount;
		ComOwner<IDMLBindingTable> table;
		IDMLBindingTable *tablePointer = nullptr;
		const auto result = dmlDevice_->CreateBindingTable(&description, IID_PPV_ARGS(&tablePointer));
		table.reset(tablePointer);
		if (FAILED(result)) {
			throw std::runtime_error("CreateBindingTable failed (HRESULT " + std::to_string(result) +
						 ", device reason " +
						 std::to_string(dmlDevice_->GetDeviceRemovedReason()) + ")");
		}
		return table;
	}

	void bindTemporary(IDMLBindingTable *table, ID3D12Resource *resource, std::uint64_t size)
	{
		if (!size) {
			return;
		}
		const auto buffer = bufferBinding(resource, 0, size);
		const auto binding = bindingDesc(&buffer);
		table->BindTemporaryResource(&binding);
	}

	void createInitializationTables(std::span<const StepBindings> bindings)
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> scope;
		TraceLoggingWriteStart(scope, "Program.PrepareInitializationBindings");
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			const auto &description = definition_.params.steps[stepIndex];
			const auto &binding = bindings[stepIndex];
			const auto inputs = std::span(description.inputs).first(description.inputCount);
			if (description.output == noTensor || description.shader != CompiledShader::None)
				continue;
			ComOwner<IDMLBindingTable> table;
			try {
				table = createTable(
					initializationHeap_.get(), step.initializer.get(), binding.descriptorOffset,
					std::max<UINT>(1, binding.initializerProperties.RequiredDescriptorCount));
			} catch (const std::exception &error) {
				throw std::runtime_error("DirectML initializer step " + std::to_string(stepIndex) +
							 ": " + error.what());
			}
			std::vector<DML_BUFFER_BINDING> buffers(inputs.size());
			for (std::size_t index = 0; index < inputs.size(); ++index) {
				const auto id = inputs[index];
				if (id != noTensor && isWeightTensor(definition_.params.tensors[id])) {
					const auto &tensor = definition_.params.tensors[id];
					buffers[index] = bufferBinding(
						weight_.get(), static_cast<std::uint64_t>(tensorByteOffset(tensor)),
						tensorSize(tensor));
				}
			}
			DML_BUFFER_ARRAY_BINDING bufferArray{static_cast<UINT>(buffers.size()), buffers.data()};
			const DML_BINDING_DESC inputBinding{DML_BINDING_TYPE_BUFFER_ARRAY, &bufferArray};
			table->BindInputs(1, &inputBinding);
			if (const HRESULT result = dmlDevice_->GetDeviceRemovedReason(); FAILED(result))
				throw std::runtime_error(
					std::string(("initializer step " + std::to_string(stepIndex) + " BindInputs")
							    .c_str()) +
					" failed (HRESULT " + std::to_string(result) + ")");
			bindTemporary(table.get(), step.dmlOpaquePointers.temporary.get(),
				      binding.initializerProperties.TemporaryResourceSize);
			if (const HRESULT result = dmlDevice_->GetDeviceRemovedReason(); FAILED(result))
				throw std::runtime_error(std::string("initializer BindTemporaryResource") +
							 " failed (HRESULT " + std::to_string(result) + ")");
			if (const auto persistent = step.dmlOpaquePointers.persistent.get()) {
				const auto buffer = bufferBinding(persistent, 0,
								  binding.operationProperties.PersistentResourceSize);
				const auto binding = bindingDesc(&buffer);
				table->BindOutputs(1, &binding);
			} else {
				const DML_BINDING_DESC binding{DML_BINDING_TYPE_NONE, nullptr};
				table->BindOutputs(1, &binding);
			}
			if (const HRESULT result = dmlDevice_->GetDeviceRemovedReason(); FAILED(result))
				throw std::runtime_error(std::string("initializer BindOutputs") + " failed (HRESULT " +
							 std::to_string(result) + ")");
			step.initializationTable = std::move(table);
		}
	}

	void createMPLD10DepthwiseConv2dReluPipeline(Step &step)
	{
		static constexpr char shader[] = R"HLSL(
#define W 128
#define H 72
#define C 16
#define OW 128
#define OH 72
#define PX 1
#define PY 1
#define STRIP 8
#define THREADS 64

RWByteAddressBuffer input:register(u0);RWByteAddressBuffer weights:register(u1);RWByteAddressBuffer bias:register(u2);RWByteAddressBuffer output:register(u3);

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

)HLSL";
		ComOwner<ID3DBlob> bytecode, errors;
		ID3DBlob *bytecodePointer = nullptr;
		ID3DBlob *compileErrorsPointer = nullptr;
		const HRESULT bytecodeResult = D3DCompile(shader, sizeof(shader) - 1, "MPLD10DepthwiseConv2dRelu",
							  nullptr, nullptr, "main", "cs_5_0",
							  D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS,
							  0, &bytecodePointer, &compileErrorsPointer);
		bytecode.reset(bytecodePointer);
		errors.reset(compileErrorsPointer);
		if (const HRESULT result = bytecodeResult; FAILED(result))
			throw std::runtime_error(std::string("Compile MPLD10DepthwiseConv2dRelu") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		std::array<D3D12_ROOT_PARAMETER, 4> parameters{};
		for (UINT i = 0; i < parameters.size(); ++i) {
			parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			parameters[i].Descriptor.ShaderRegister = i;
		}
		D3D12_ROOT_SIGNATURE_DESC root{};
		root.NumParameters = static_cast<UINT>(parameters.size());
		root.pParameters = parameters.data();
		ComOwner<ID3DBlob> serialized;
		ID3DBlob *serializedPointer = nullptr;
		ID3DBlob *serializationErrorsPointer = nullptr;
		const HRESULT serializedResult = D3D12SerializeRootSignature(
			&root, D3D_ROOT_SIGNATURE_VERSION_1, &serializedPointer, &serializationErrorsPointer);
		serialized.reset(serializedPointer);
		errors.reset(serializationErrorsPointer);
		if (const HRESULT result = serializedResult; FAILED(result))
			throw std::runtime_error(std::string("Serialize MPLD10DepthwiseConv2dRelu root signature") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		ID3D12RootSignature *rootSignaturePointer = nullptr;
		const HRESULT rootSignatureResult =
			d3dDevice_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
							IID_PPV_ARGS(&rootSignaturePointer));
		step.rootSignature.reset(rootSignaturePointer);
		if (const HRESULT result = rootSignatureResult; FAILED(result))
			throw std::runtime_error(std::string("Create MPLD10DepthwiseConv2dRelu root signature") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = step.rootSignature.get();
		desc.CS = {bytecode->GetBufferPointer(), bytecode->GetBufferSize()};
		ID3D12PipelineState *pipelinePointer = nullptr;
		const HRESULT pipelineResult =
			d3dDevice_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelinePointer));
		step.pipeline.reset(pipelinePointer);
		if (const HRESULT result = pipelineResult; FAILED(result))
			throw std::runtime_error(std::string("Create MPLD10DepthwiseConv2dRelu pipeline") +
						 " failed (HRESULT " + std::to_string(result) + ")");
	}

	void createMPLD10MaskPipeline(Step &step)
	{
		static constexpr char binarize[] = R"HLSL(
RWStructuredBuffer<float> probabilities : register(u0);
RWStructuredBuffer<float> threshold : register(u1);
RWStructuredBuffer<float> output : register(u3);
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 256 * 144) return;
    output[id.x] = probabilities[id.x] < threshold[0] ? 1.0f : 0.0f;
}
)HLSL";
		ComOwner<ID3DBlob> bytecode, errors;
		ID3DBlob *bytecodePointer = nullptr;
		ID3DBlob *compileErrorsPointer = nullptr;
		const HRESULT bytecodeResult = D3DCompile(binarize, sizeof(binarize) - 1, "MPLD10BinarizeFloat32",
							  nullptr, nullptr, "main", "cs_5_0",
							  D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS,
							  0, &bytecodePointer, &compileErrorsPointer);
		bytecode.reset(bytecodePointer);
		errors.reset(compileErrorsPointer);
		if (const HRESULT result = bytecodeResult; FAILED(result))
			throw std::runtime_error(std::string("Compile MPLD10 mask shader") + " failed (HRESULT " +
						 std::to_string(result) + ")");
		std::array<D3D12_ROOT_PARAMETER, 4> parameters{};
		for (UINT i = 0; i < 4; ++i) {
			parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			parameters[i].Descriptor.ShaderRegister = i;
		}
		D3D12_ROOT_SIGNATURE_DESC root{};
		root.NumParameters = static_cast<UINT>(parameters.size());
		root.pParameters = parameters.data();
		ComOwner<ID3DBlob> serialized;
		ID3DBlob *serializedPointer = nullptr;
		ID3DBlob *serializationErrorsPointer = nullptr;
		const HRESULT serializedResult = D3D12SerializeRootSignature(
			&root, D3D_ROOT_SIGNATURE_VERSION_1, &serializedPointer, &serializationErrorsPointer);
		serialized.reset(serializedPointer);
		errors.reset(serializationErrorsPointer);
		if (const HRESULT result = serializedResult; FAILED(result))
			throw std::runtime_error(std::string("Serialize MPLD10 mask root signature") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		ID3D12RootSignature *rootSignaturePointer = nullptr;
		const HRESULT rootSignatureResult =
			d3dDevice_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
							IID_PPV_ARGS(&rootSignaturePointer));
		step.rootSignature.reset(rootSignaturePointer);
		if (const HRESULT result = rootSignatureResult; FAILED(result))
			throw std::runtime_error(std::string("Create MPLD10 mask root signature") +
						 " failed (HRESULT " + std::to_string(result) + ")");
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = step.rootSignature.get();
		desc.CS = {bytecode->GetBufferPointer(), bytecode->GetBufferSize()};
		ID3D12PipelineState *pipelinePointer = nullptr;
		const HRESULT pipelineResult =
			d3dDevice_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelinePointer));
		step.pipeline.reset(pipelinePointer);
		if (const HRESULT result = pipelineResult; FAILED(result))
			throw std::runtime_error(std::string("Create MPLD10 mask pipeline") + " failed (HRESULT " +
						 std::to_string(result) + ")");
	}

	void createExecutionTables(std::span<const StepBindings> bindings)
	{
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			const auto &description = definition_.params.steps[stepIndex];
			const auto &binding = bindings[stepIndex];
			const auto inputs = std::span(description.inputs).first(description.inputCount);
			// Graph barriers refer directly to node indices; internal tensors have separate encoded keys.
			for (const auto id : definition_.params.uavBarriers.subspan(description.barrierBegin,
										    description.barrierCount)) {
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = resource(id);
				if (!barrier.UAV.pResource)
					throw std::runtime_error("UAV barrier target resource is unavailable");
				step.barriers.push_back(barrier);
			}
			if (description.output == noTensor || description.shader != CompiledShader::None)
				continue;
			try {
				step.executionTable = createTable(
					descriptorHeap_.get(), step.operation.get(), binding.descriptorOffset,
					std::max<UINT>(1, binding.operationProperties.RequiredDescriptorCount));
			} catch (const std::exception &error) {
				throw std::runtime_error("DirectML execution step " + std::to_string(stepIndex) + ": " +
							 error.what());
			}
			std::vector<DML_BUFFER_BINDING> buffers(inputs.size());
			std::vector<DML_BINDING_DESC> bindings(inputs.size(), {DML_BINDING_TYPE_NONE, nullptr});
			for (std::size_t index = 0; index < inputs.size(); ++index) {
				const auto id = inputs[index];
				if (id != noTensor && !isWeightTensor(definition_.params.tensors[id])) {
					buffers[index] = bufferBinding(resource(id), 0,
								       tensorSize(definition_.params.tensors[id]));
					bindings[index] = bindingDesc(&buffers[index]);
				}
			}
			if (!bindings.empty()) {
				step.executionTable->BindInputs(static_cast<UINT>(bindings.size()), bindings.data());
			}
			const auto outputBuffer =
				bufferBinding(resource(description.output), 0,
					      tensorSize(definition_.params.tensors[description.output]));
			const auto outputBinding = bindingDesc(&outputBuffer);
			step.executionTable->BindOutputs(1, &outputBinding);
			if (binding.operationProperties.TemporaryResourceSize) {
				const auto temporaryBuffer =
					bufferBinding(step.dmlOpaquePointers.temporary.get(), 0,
						      binding.operationProperties.TemporaryResourceSize);
				const auto temporaryBinding = bindingDesc(&temporaryBuffer);
				step.executionTable->BindTemporaryResource(&temporaryBinding);
			}
			if (const auto persistent = step.dmlOpaquePointers.persistent.get()) {
				const auto persistentBuffer = bufferBinding(
					persistent, 0, binding.operationProperties.PersistentResourceSize);
				const auto persistentBinding = bindingDesc(&persistentBuffer);
				step.executionTable->BindPersistentResource(&persistentBinding);
			}
		}
	}

	const DmlProgramDefinition &definition_;
	ID3D12Device *const d3dDevice_;
	IDMLDevice *const dmlDevice_;
	std::vector<ComOwner<ID3D12Resource>> storages_;
	InternalTensors internalTensors_;
	std::vector<Step> steps_;
	ComOwner<IDMLCommandRecorder> recorder_;
	ComOwner<ID3D12DescriptorHeap> initializationHeap_;
	ComOwner<ID3D12DescriptorHeap> descriptorHeap_;
	ComOwner<ID3D12Resource> weightUpload_;
	ComOwner<ID3D12Resource> weight_;
	std::vector<D3D12_RESOURCE_BARRIER> inputReadyTransitions_;
	std::vector<D3D12_RESOURCE_BARRIER> inputRestoreTransitions_;
	std::vector<D3D12_RESOURCE_BARRIER> outputTransitions_;
};

} // namespace Detail

auto createDmlProgram(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition) -> DmlProgram
{
	return Detail::ProgramBuilder(device, dmlDevice, definition).takeProgram();
}

} // namespace BaselineDml

// MARK: - SDK Type Assertions

static_assert(static_cast<std::uint32_t>(dtUnknown) == DML_TENSOR_DATA_TYPE_UNKNOWN);
static_assert(static_cast<std::uint32_t>(dtFloat32) == DML_TENSOR_DATA_TYPE_FLOAT32);
static_assert(static_cast<std::uint32_t>(dtFloat16) == DML_TENSOR_DATA_TYPE_FLOAT16);
static_assert(static_cast<std::uint32_t>(dtUInt32) == DML_TENSOR_DATA_TYPE_UINT32);
static_assert(static_cast<std::uint32_t>(dtUInt16) == DML_TENSOR_DATA_TYPE_UINT16);
static_assert(static_cast<std::uint32_t>(dtUInt8) == DML_TENSOR_DATA_TYPE_UINT8);
static_assert(static_cast<std::uint32_t>(dtInt32) == DML_TENSOR_DATA_TYPE_INT32);
static_assert(static_cast<std::uint32_t>(dtInt16) == DML_TENSOR_DATA_TYPE_INT16);
static_assert(static_cast<std::uint32_t>(dtInt8) == DML_TENSOR_DATA_TYPE_INT8);
static_assert(static_cast<std::uint32_t>(dtFloat64) == DML_TENSOR_DATA_TYPE_FLOAT64);
static_assert(static_cast<std::uint32_t>(dtUInt64) == DML_TENSOR_DATA_TYPE_UINT64);
static_assert(static_cast<std::uint32_t>(dtInt64) == DML_TENSOR_DATA_TYPE_INT64);
