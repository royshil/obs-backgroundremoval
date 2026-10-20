// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <QPointer>

class QDialog;

struct obs_source;
using obs_source_t = struct obs_source;

namespace BackgroundRemoval {

class MotionMeasurement;

class AutoMotionConfigurator final {
public:
	explicit AutoMotionConfigurator(obs_source_t *source) noexcept : source_(source) {}
	~AutoMotionConfigurator();
	AutoMotionConfigurator(const AutoMotionConfigurator &) = delete;
	auto operator=(const AutoMotionConfigurator &) -> AutoMotionConfigurator & = delete;

	auto show(std::shared_ptr<void> owner = {}) -> bool;
	auto startMeasurement() -> std::shared_ptr<MotionMeasurement>;
	void stopMeasurement() noexcept;
	void recordMotion(double motion) noexcept;

private:
	obs_source_t *const source_;
	std::atomic<std::shared_ptr<MotionMeasurement>> measurement_;
	QPointer<QDialog> dialog_;
};

} // namespace BackgroundRemoval
