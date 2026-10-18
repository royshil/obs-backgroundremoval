// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <stdexcept>

namespace GpuAccessValidation {
inline bool enabled()
{
	return std::getenv("DML_TEST_GPU_VALIDATION") != nullptr;
}
inline void enable()
{
	if (!enabled())
		return;
	Microsoft::WRL::ComPtr<ID3D12Debug1> validation;
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&validation))))
		throw std::runtime_error("D3D12 GPU validation is unavailable");
	validation->EnableDebugLayer();
	if (std::strcmp(std::getenv("DML_TEST_GPU_VALIDATION"), "cpu") != 0) {
		validation->SetEnableGPUBasedValidation(TRUE);
		validation->SetEnableSynchronizedCommandQueueValidation(TRUE);
	}
}
inline void checkMessages(ID3D12Device *device)
{
	if (!enabled())
		return;
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
	REQUIRE(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&queue))));
	const auto count = queue->GetNumStoredMessagesAllowedByRetrievalFilter();
	std::cout << "GPU_VALIDATION,messages=" << count
		  << ",discarded=" << queue->GetNumMessagesDiscardedByMessageCountLimit() << '\n';
	CHECK(queue->GetNumMessagesDiscardedByMessageCountLimit() == 0);
	for (UINT64 i = 0; i < count; ++i) {
		SIZE_T size = 0;
		REQUIRE(SUCCEEDED(queue->GetMessage(i, nullptr, &size)));
		std::vector<unsigned char> bytes(size);
		auto *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
		REQUIRE(SUCCEEDED(queue->GetMessage(i, message, &size)));
		if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
			std::cout << "GPU_MESSAGE," << message->ID << ',' << message->pDescription << '\n';
		INFO(message->pDescription);
		CHECK(message->Severity != D3D12_MESSAGE_SEVERITY_CORRUPTION);
		CHECK(message->Severity != D3D12_MESSAGE_SEVERITY_ERROR);
	}
}
} // namespace GpuAccessValidation
