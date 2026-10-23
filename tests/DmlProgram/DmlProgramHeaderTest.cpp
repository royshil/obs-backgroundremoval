// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

// This translation unit intentionally includes neither model definitions nor SDK headers.
#include <BaselineDml/DmlProgram.hpp>

#include <type_traits>
#include <utility>

static_assert(std::is_nothrow_destructible_v<BaselineDml::DmlProgram>);
static_assert(std::is_nothrow_move_constructible_v<BaselineDml::DmlProgram>);
static_assert(!std::is_copy_constructible_v<BaselineDml::DmlProgram>);

void checkDmlProgramHeaderIsolation()
{
	BaselineDml::DmlProgram owner;
	auto moved = std::move(owner);
}
