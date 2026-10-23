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

#include "ProgramTestSession.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef DML_TARGET_VERSION
#error "DML_TARGET_VERSION is already defined"
#else
#define DML_TARGET_VERSION 0x1000
#include <d3d12.h>
#include <DirectML.h>
#endif
#include <wrl/client.h>

namespace DmlProgramTest {

void check(std::int32_t result, const char *message)
{
	if (FAILED(result))
		throw std::runtime_error(std::string(message) + " failed (HRESULT " + std::to_string(result) + ")");
}

// Test/benchmark-owned CPU transfer and submission harness; not part of the production program API.
class ProgramTestSession::Impl final {
public:
	Impl(std::span<const BaselineDml::Definition::Node> nodes, const BaselineDml::Definition::ModelParams &params,
	     std::span<const std::byte> weights)
		: definition_(BaselineDml::makeDmlProgramDefinition(nodes, params, weights))
	{
		if (!event_)
			throw std::runtime_error("Create test completion event failed");
		check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)), "Create test device");
		check(DMLCreateDevice(device_.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&dmlDevice_)),
		      "Create test DirectML device");
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		check(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_)), "Create test queue");
		check(device_->CreateCommandAllocator(desc.Type, IID_PPV_ARGS(&allocator_)), "Create test allocator");
		check(device_->CreateCommandList(0, desc.Type, allocator_.Get(), nullptr, IID_PPV_ARGS(&commands_)),
		      "Create test commands");
		check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "Create test fence");
		program_ = std::make_unique<BaselineDml::DmlProgram>(
			BaselineDml::createDmlProgram(device_.Get(), dmlDevice_.Get(), definition_));
		BaselineDml::recordInitialization(definition_, *program_, commands_.Get());
		submitAndWait();
	}

	auto device() const noexcept -> ID3D12Device * { return device_.Get(); }
	auto inputInfo(std::size_t index) const noexcept -> BaselineDml::TensorInfo
	{
		return BaselineDml::inputInfo(definition_, index);
	}
	auto outputInfo() const noexcept -> BaselineDml::TensorInfo { return BaselineDml::outputInfo(definition_); }

	void run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output)
	{
		if (inputs.size() != BaselineDml::inputCount(definition_))
			throw std::invalid_argument("Test input count mismatch");
		if (output.size() != outputInfo().dataBytes)
			throw std::invalid_argument("Test output byte size mismatch");
		for (std::size_t i = 0; i < inputs.size(); ++i)
			if (inputs[i].size() != inputInfo(i).dataBytes)
				throw std::invalid_argument("Test input " + std::to_string(i) + " byte size mismatch");
		// Allocate transfer buffers only once so benchmark iterations retain the same resources.
		if (uploads_.empty()) {
			for (const auto &input : inputs)
				uploads_.push_back(buffer((input.size() + 3) / 4 * 4, D3D12_HEAP_TYPE_UPLOAD,
							  D3D12_RESOURCE_STATE_GENERIC_READ));
		}
		if (!readback_)
			readback_ = buffer(output.size(), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		std::vector<BaselineDml::GpuInput> gpuInputs;
		for (std::size_t i = 0; i < inputs.size(); ++i) {
			void *mapped = nullptr;
			const D3D12_RANGE noRead{0, 0};
			check(uploads_[i]->Map(0, &noRead, &mapped), "Map test input");
			std::memset(mapped, 0, static_cast<std::size_t>(uploads_[i]->GetDesc().Width));
			std::memcpy(mapped, inputs[i].data(), inputs[i].size());
			uploads_[i]->Unmap(0, nullptr);
			gpuInputs.push_back({uploads_[i].Get(), 0, inputs[i].size()});
		}
		check(allocator_->Reset(), "Reset test allocator");
		check(commands_->Reset(allocator_.Get(), nullptr), "Reset test commands");
		BaselineDml::recordGpuRun(definition_, *program_, commands_.Get(), gpuInputs, nullptr, readback_.Get(),
					  0);
		submitAndWait();
		void *mapped = nullptr;
		const D3D12_RANGE range{0, output.size()};
		check(readback_->Map(0, &range, &mapped), "Map test output");
		std::memcpy(output.data(), mapped, output.size());
		const D3D12_RANGE noWrite{0, 0};
		readback_->Unmap(0, &noWrite);
	}

private:
	auto buffer(std::uint64_t bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
		-> Microsoft::WRL::ComPtr<ID3D12Resource>
	{
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = type;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = std::max<std::uint64_t>(bytes, 4);
		desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> result;
		check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
						       IID_PPV_ARGS(&result)),
		      "Create test buffer");
		return result;
	}

	void submitAndWait()
	{
		check(commands_->Close(), "Close test commands");
		ID3D12CommandList *lists[]{commands_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		const auto value = ++fenceValue_;
		const HRESULT signal = queue_->Signal(fence_.Get(), value);
		if (FAILED(signal) && FAILED(queue_->Signal(fence_.Get(), value))) {
			// With no completion marker, a live device cannot safely release submitted resources.
			if (SUCCEEDED(device_->GetDeviceRemovedReason()))
				std::terminate();
			check(signal, "Signal test completion");
		}
		HRESULT waitStatus = fence_->SetEventOnCompletion(value, event_.get());
		for (;;) {
			const auto completed = fence_->GetCompletedValue();
			if (completed == std::numeric_limits<std::uint64_t>::max())
				throw std::runtime_error("Test GPU device removed");
			if (completed >= value)
				break;
			check(device_->GetDeviceRemovedReason(), "Test GPU completion");
			if (SUCCEEDED(waitStatus)) {
				if (WaitForSingleObject(event_.get(), 1000) == WAIT_FAILED)
					waitStatus = HRESULT_FROM_WIN32(GetLastError());
			} else {
				Sleep(1); // Drain submitted work before reporting an event setup/wait failure.
			}
		}
		check(signal, "Signal test completion");
		check(waitStatus, "Wait for test completion");
		check(dmlDevice_->GetDeviceRemovedReason(), "Test DirectML completion");
	}

	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<IDMLDevice> dmlDevice_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	std::uint64_t fenceValue_ = 0;
	std::unique_ptr<void, decltype(&CloseHandle)> event_{CreateEventW(nullptr, FALSE, FALSE, nullptr),
							     &CloseHandle};
	const BaselineDml::DmlProgramDefinition definition_;
	std::unique_ptr<BaselineDml::DmlProgram> program_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uploads_;
	Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
};

ProgramTestSession::ProgramTestSession(std::span<const BaselineDml::Definition::Node> nodes,
				       const BaselineDml::Definition::ModelParams &params,
				       std::span<const std::byte> weights)
	: nodes_(nodes.begin(), nodes.end()),
	  weights_(weights.begin(), weights.end()),
	  impl_(std::make_unique<Impl>(nodes_, params, weights_))
{
}

ProgramTestSession::ProgramTestSession(std::span<const BaselineDml::Definition::TensorSpec> specs,
				       std::span<const BaselineDml::Definition::Node> nodes,
				       std::span<const std::pair<std::int32_t, std::int32_t>> barriers,
				       std::int32_t outputIndex, std::span<const std::byte> weights)
{
	nodes_.assign(nodes.begin(), nodes.end());
	weights_.assign(weights.begin(), weights.end());
	tables_ = BaselineDml::Definition::makeModelParamsStorage(specs, nodes_, barriers, outputIndex, weights.size());
	impl_ = std::make_unique<Impl>(nodes_, tables_.params(), weights_);
}

ProgramTestSession::~ProgramTestSession() noexcept = default;

auto ProgramTestSession::device() const noexcept -> ID3D12Device *
{
	return impl_->device();
}

auto ProgramTestSession::inputInfo(std::size_t index) const noexcept -> BaselineDml::TensorInfo
{
	return impl_->inputInfo(index);
}

auto ProgramTestSession::outputInfo() const noexcept -> BaselineDml::TensorInfo
{
	return impl_->outputInfo();
}

void ProgramTestSession::run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output)
{
	impl_->run(inputs, output);
}

} // namespace DmlProgramTest
