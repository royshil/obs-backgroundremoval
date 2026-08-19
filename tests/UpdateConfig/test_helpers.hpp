// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

namespace TestHelpers {

class HttpServer final {
public:
	explicit HttpServer(std::string body, int statusCode = 200);
	~HttpServer() noexcept;

	HttpServer(const HttpServer &) = delete;
	HttpServer &operator=(const HttpServer &) = delete;
	HttpServer(HttpServer &&) = delete;
	HttpServer &operator=(HttpServer &&) = delete;

	[[nodiscard]] std::string url() const;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace TestHelpers
