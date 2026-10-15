// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Included only inside the runtime Impl in the opt-in diagnostic target.
void configureStorageDiagnostics(std::span<const std::uint8_t> roundedSteps)
{
	if (roundedSteps.size() != steps_.size())
		throw std::invalid_argument("Diagnostic step count");
	diagnosticRoundedSteps_.assign(roundedSteps.begin(), roundedSteps.end());
	if (diagnosticReadback_)
		return;
	UINT64 bytes = 0;
	for (const auto &step : steps_) {
		if (step.output == noTensor) {
			diagnosticOffsets_.push_back(bytes);
			diagnosticValues_.emplace_back();
			continue;
		}
		const auto &t = tensors_.at(step.output);
		if (t.dataType != ProgramDefinition::dtFloat32)
			throw std::invalid_argument("Storage diagnostics require FLOAT32");
		diagnosticOffsets_.push_back(bytes);
		diagnosticValues_.emplace_back(t.dataBytes / 4);
		bytes = (bytes + t.dataBytes + 15) & ~UINT64(15);
	}
	diagnosticReadback_ =
		createBuffer(d3dDevice_.Get(), bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	constexpr char shader[] = R"(
RWByteAddressBuffer tensor : register(u0);
cbuffer Count : register(b0) { uint count; };
uint halfNearest(float value) {
 uint bits=asuint(value), sign=(bits>>16)&0x8000, mantissa=bits&0x7fffff, exponent=(bits>>23)&255;
 if(exponent==255)return sign|0x7c00|(mantissa ? 0x200 : 0);
 int e=int(exponent)-112;
 if(e>=31)return sign|0x7c00;
 if(e<=0){if(e < -10)return sign;mantissa|=0x800000;uint shift=uint(14-e);return sign|((mantissa+((1u<<(shift-1))-1)+((mantissa>>shift)&1))>>shift);}
 uint rounded=mantissa+0xfff+((mantissa>>13)&1);return sign|((uint(e)<<10)+(rounded>>13));
}
[numthreads(64,1,1)] void main(uint3 id:SV_DispatchThreadID) {if(id.x<count){uint at=id.x*4;float v=asfloat(tensor.Load(at));tensor.Store(at,asuint(f16tof32(halfNearest(v))));}}
)";
	ComPtr<ID3DBlob> bytecode, errors;
	check(D3DCompile(shader, sizeof(shader) - 1, "DiagnosticStorageRound", nullptr, nullptr, "main", "cs_5_0",
			 D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, &errors),
	      "Compile diagnostic rounding");
	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	params[0].Descriptor.ShaderRegister = 0;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[1].Constants = {0, 0, 1};
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 2;
	desc.pParameters = params;
	ComPtr<ID3DBlob> serialized;
	check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors),
	      "Serialize diagnostic root");
	check(d3dDevice_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
					      IID_PPV_ARGS(&diagnosticRoot_)),
	      "Create diagnostic root");
	D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = diagnosticRoot_.Get();
	pso.CS = {bytecode->GetBufferPointer(), bytecode->GetBufferSize()};
	check(d3dDevice_->CreateComputePipelineState(&pso, IID_PPV_ARGS(&diagnosticPipeline_)),
	      "Create diagnostic PSO");
}
auto diagnosticIntermediateValues() const -> const std::vector<std::vector<float>> &
{
	return diagnosticValues_;
}

private:
void roundDiagnosticStep(Step &step, std::size_t index)
{
	if (step.output == noTensor || !diagnosticRoundedSteps_[index])
		return;
	auto *buffer = resource(step.output);
	D3D12_RESOURCE_BARRIER uav{};
	uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav.UAV.pResource = buffer;
	commandList_->ResourceBarrier(1, &uav);
	commandList_->SetPipelineState(diagnosticPipeline_.Get());
	commandList_->SetComputeRootSignature(diagnosticRoot_.Get());
	commandList_->SetComputeRootUnorderedAccessView(0, buffer->GetGPUVirtualAddress());
	auto count = static_cast<UINT>(tensors_.at(step.output).dataBytes / 4);
	commandList_->SetComputeRoot32BitConstant(1, count, 0);
	commandList_->Dispatch((count + 63) / 64, 1, 1);
	commandList_->ResourceBarrier(1, &uav);
}
void recordDiagnosticCopies()
{
	for (std::size_t i = 0; i < steps_.size(); ++i) {
		if (steps_[i].output == noTensor)
			continue;
		auto *buffer = resource(steps_[i].output);
		auto barrier =
			transition(buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList_->ResourceBarrier(1, &barrier);
		commandList_->CopyBufferRegion(diagnosticReadback_.Get(), diagnosticOffsets_[i], buffer, 0,
					       tensors_.at(steps_[i].output).dataBytes);
		barrier = transition(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList_->ResourceBarrier(1, &barrier);
	}
}
void readDiagnosticCopies()
{
	void *mapped = nullptr;
	check(diagnosticReadback_->Map(0, nullptr, &mapped), "Map diagnostic readback");
	for (std::size_t i = 0; i < steps_.size(); ++i) {
		if (diagnosticValues_[i].empty())
			continue;
		std::memcpy(diagnosticValues_[i].data(), static_cast<std::byte *>(mapped) + diagnosticOffsets_[i],
			    diagnosticValues_[i].size() * 4);
	}
	D3D12_RANGE noWrite{0, 0};
	diagnosticReadback_->Unmap(0, &noWrite);
}
std::vector<std::uint8_t> diagnosticRoundedSteps_;
std::vector<UINT64> diagnosticOffsets_;
std::vector<std::vector<float>> diagnosticValues_;
ComPtr<ID3D12Resource> diagnosticReadback_;
ComPtr<ID3D12RootSignature> diagnosticRoot_;
ComPtr<ID3D12PipelineState> diagnosticPipeline_;

public:
