// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

// The public header is self-contained; construction/destruction also require the SDK inheritance declarations.

#include <BaselineDml/DmlProgram.hpp>

#include <type_traits>
#include <utility>

#include <d3d12.h>

#ifndef DML_TARGET_VERSION
#define DML_TARGET_VERSION 0x1000
#elif DML_TARGET_VERSION != 0x1000
#error "DML_TARGET_VERSION is not 0x1000"
#endif
#include <DirectML.h>

static_assert(std::is_nothrow_destructible_v<BaselineDml::DmlProgram>);
static_assert(std::is_nothrow_move_constructible_v<BaselineDml::DmlProgram>);
static_assert(std::is_nothrow_move_assignable_v<BaselineDml::DmlProgram>);
static_assert(!std::is_copy_constructible_v<BaselineDml::DmlProgram>);

void checkDmlProgramHeaderIsolation()
{
	auto owner = std::make_unique<BaselineDml::DmlProgram>();
	auto moved = std::move(owner);
}
