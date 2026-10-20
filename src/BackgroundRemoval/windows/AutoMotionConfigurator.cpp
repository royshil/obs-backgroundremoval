// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AutoMotionConfigurator.hpp"
#include "IRenderingPipeline.hpp"
#include "ModuleText.hpp"
#include "MotionMeasurement.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <util/platform.h>
#include <utility>

namespace BackgroundRemoval {
namespace {
constexpr int kMeasurementMs = 3000;
}

AutoMotionConfigurator::AutoMotionConfigurator(obs_source_t *source, std::shared_ptr<IRenderingPipeline> pipeline,
					       QWidget *parent)
	: QDialog(parent),
	  source_(source),
	  pipeline_(std::move(pipeline)),
	  timer_(new QTimer(this)),
	  status_(new QLabel(this)),
	  progress_(new QProgressBar(this)),
	  start_(new QPushButton(tStartMotionMeasurement(), this)),
	  preview_(new QCheckBox(tMotionPreview(), this))
{
	setWindowTitle(tAutoMotionTitle());
	setMinimumWidth(440);
	auto *layout = new QVBoxLayout(this);
	auto *instructions = new QLabel(tAutoMotionInstructions(), this);
	instructions->setWordWrap(true);
	status_->setWordWrap(true);
	progress_->setRange(0, kMeasurementMs);
	progress_->setValue(0);
	preview_->setEnabled(false);
	layout->addWidget(instructions);
	layout->addWidget(start_);
	layout->addWidget(progress_);
	layout->addWidget(status_);
	layout->addWidget(preview_);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
	apply_ = buttons->button(QDialogButtonBox::Apply);
	apply_->setEnabled(false);
	layout->addWidget(buttons);
	connect(apply_, &QPushButton::clicked, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(start_, &QPushButton::clicked, this, [this] { startMeasurement(); });
	connect(preview_, &QCheckBox::toggled, this, [this](bool enabled) { setPreview(enabled); });
	connect(timer_, &QTimer::timeout, this, [this] { updateMeasurement(); });
	timer_->setInterval(50);
	if (source_) {
		OBSDataAutoRelease settings = obs_source_get_settings(source_);
		originalValue_ = obs_data_get_double(settings, "v2_MotionSuppression");
		originalHasUserValue_ = obs_data_has_user_value(settings, "v2_MotionSuppression");
	}
	if (!source_ || !pipeline_) {
		start_->setEnabled(false);
		status_->setText(tNoMotionInput());
	}
}

AutoMotionConfigurator::~AutoMotionConfigurator()
{
	stopMeasurement();
	if (previewApplied_)
		restoreValue();
}

auto AutoMotionConfigurator::show(obs_source_t *source, std::shared_ptr<IRenderingPipeline> pipeline) -> bool
{
	AutoMotionConfigurator dialog(source, std::move(pipeline), QApplication::activeWindow());
	dialog.exec();
	// The standard OBS button handler refreshes the property view after this returns.
	return true;
}

void AutoMotionConfigurator::startMeasurement()
{
	preview_->setChecked(false);
	preview_->setEnabled(false);
	apply_->setEnabled(false);
	recommendation_.reset();
	stopMeasurement();
	measurement_ = std::make_shared<MotionMeasurement>(os_gettime_ns(), std::uint64_t(kMeasurementMs) * 1000000);
	pipeline_->setMotionMeasurement(measurement_);
	progress_->setValue(0);
	status_->setText(tMeasuringMotion());
	start_->setEnabled(false);
	elapsed_.start();
	timer_->start();
}

void AutoMotionConfigurator::updateMeasurement()
{
	progress_->setValue(static_cast<int>(std::min<qint64>(elapsed_.elapsed(), kMeasurementMs)));
	if (elapsed_.elapsed() < kMeasurementMs)
		return;
	stopMeasurement();
	recommendation_ = measurement_->motionSuppression();
	start_->setText(tMeasureMotionAgain());
	start_->setEnabled(true);
	if (!recommendation_) {
		status_->setText(tNoMotionInput());
		return;
	}
	status_->setText(QString::fromUtf8(tMotionMeasurementResult())
				 .arg(*recommendation_, 0, 'f', 2)
				 .arg(static_cast<qulonglong>(measurement_->sampleCount())));
	preview_->setEnabled(true);
	apply_->setEnabled(true);
}

void AutoMotionConfigurator::stopMeasurement()
{
	timer_->stop();
	if (measurement_) {
		measurement_->stop();
		pipeline_->setMotionMeasurement(nullptr);
	}
}

void AutoMotionConfigurator::applyValue(double value)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_double(settings, "v2_MotionSuppression", value);
	obs_source_update(source_, settings);
}

void AutoMotionConfigurator::restoreValue()
{
	if (originalHasUserValue_) {
		applyValue(originalValue_);
	} else {
		OBSDataAutoRelease settings = obs_source_get_settings(source_);
		obs_data_unset_user_value(settings, "v2_MotionSuppression");
		obs_source_update(source_, nullptr);
	}
	previewApplied_ = false;
}

void AutoMotionConfigurator::setPreview(bool enabled)
{
	if (enabled && recommendation_) {
		applyValue(*recommendation_);
		previewApplied_ = true;
	} else if (previewApplied_) {
		restoreValue();
	}
}

void AutoMotionConfigurator::done(int result)
{
	stopMeasurement();
	if (result == QDialog::Accepted) {
		if (!recommendation_)
			return;
		applyValue(*recommendation_);
		previewApplied_ = false;
	} else if (previewApplied_) {
		restoreValue();
	}
	QDialog::done(result);
}

} // namespace BackgroundRemoval
