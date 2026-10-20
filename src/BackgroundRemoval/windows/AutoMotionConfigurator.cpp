// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AutoMotionConfigurator.hpp"
#include "ModuleText.hpp"
#include "MotionMeasurement.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <obs.hpp>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>
#include <util/platform.h>
#include <utility>

namespace BackgroundRemoval {
namespace {
constexpr int kMeasurementMs = 3000;
}

class AutoMotionDialog final : public QDialog {
public:
	AutoMotionDialog(AutoMotionConfigurator &configurator, obs_source_t *source, QWidget *parent);
	~AutoMotionDialog() override;

protected:
	void done(int result) override;

private:
	void startMeasurement();
	void updateMeasurement();
	void stopMeasurement();
	void setPreview(bool enabled);
	void applyValue(double value);
	void restoreValue();
	OBSSource source_;
	AutoMotionConfigurator &configurator_;
	std::shared_ptr<MotionMeasurement> measurement_;
	std::optional<double> recommendation_;
	double originalValue_ = 0.0;
	bool originalHasUserValue_ = false;
	bool previewApplied_ = false;
	QElapsedTimer elapsed_;
	QTimer *timer_;
	QLabel *status_;
	QProgressBar *progress_;
	QPushButton *start_;
	QPushButton *apply_;
	QCheckBox *preview_;
};

AutoMotionDialog::AutoMotionDialog(AutoMotionConfigurator &configurator, obs_source_t *source, QWidget *parent)
	: QDialog(parent),
	  source_(source),
	  configurator_(configurator),
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
	if (!source_) {
		start_->setEnabled(false);
		status_->setText(tNoMotionInput());
	}
}

AutoMotionDialog::~AutoMotionDialog()
{
	stopMeasurement();
	if (previewApplied_)
		restoreValue();
}

void AutoMotionDialog::startMeasurement()
{
	preview_->setChecked(false);
	preview_->setEnabled(false);
	apply_->setEnabled(false);
	recommendation_.reset();
	stopMeasurement();
	measurement_ = configurator_.startMeasurement();
	progress_->setValue(0);
	status_->setText(tMeasuringMotion());
	start_->setEnabled(false);
	elapsed_.start();
	timer_->start();
}

void AutoMotionDialog::updateMeasurement()
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

void AutoMotionDialog::stopMeasurement()
{
	timer_->stop();
	if (measurement_) {
		measurement_->stop();
		configurator_.stopMeasurement();
	}
}

void AutoMotionDialog::applyValue(double value)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_double(settings, "v2_MotionSuppression", value);
	obs_source_update(source_, settings);
}

void AutoMotionDialog::restoreValue()
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

void AutoMotionDialog::setPreview(bool enabled)
{
	if (enabled && recommendation_) {
		applyValue(*recommendation_);
		previewApplied_ = true;
	} else if (previewApplied_) {
		restoreValue();
	}
}

void AutoMotionDialog::done(int result)
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

AutoMotionConfigurator::~AutoMotionConfigurator()
{
	stopMeasurement();
}

auto AutoMotionConfigurator::show() -> bool
{
	AutoMotionDialog dialog(*this, source_, QApplication::activeWindow());
	dialog.exec();
	return true;
}

auto AutoMotionConfigurator::startMeasurement() -> std::shared_ptr<MotionMeasurement>
{
	stopMeasurement();
	auto measurement =
		std::make_shared<MotionMeasurement>(os_gettime_ns(), std::uint64_t(kMeasurementMs) * 1000000);
	measurement_.store(measurement, std::memory_order_release);
	return measurement;
}

void AutoMotionConfigurator::stopMeasurement() noexcept
{
	if (const auto measurement = measurement_.exchange(nullptr, std::memory_order_acq_rel))
		measurement->stop();
}

void AutoMotionConfigurator::recordMotion(double motion) noexcept
{
	if (const auto measurement = measurement_.load(std::memory_order_acquire))
		measurement->record(os_gettime_ns(), motion);
}

} // namespace BackgroundRemoval
