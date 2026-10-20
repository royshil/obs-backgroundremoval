// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AutoMotionConfigurator.hpp"
#include "ModuleText.hpp"
#include "MotionMeasurement.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSlider>
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
} // namespace

class AutoMotionDialog final : public QDialog {
public:
	AutoMotionDialog(AutoMotionConfigurator &configurator, obs_source_t *source, QWidget *parent);
	~AutoMotionDialog() override;

protected:
	void done(int result) override;

private:
	void setupUi();
	void startMeasurement();
	void updateMeasurement();
	void stopMeasurement();
	void applyValue(double value);
	void applySettings();
	OBSSource source_;
	AutoMotionConfigurator &configurator_;
	std::shared_ptr<MotionMeasurement> measurement_;
	std::optional<double> recommendation_;
	QElapsedTimer elapsed_;
	QTimer *const timer_;
	QLabel *const instructions_;
	QLabel *const status_;
	QProgressBar *const progress_;
	QPushButton *const start_;
	QSlider *const suppressionSlider_;
	QDoubleSpinBox *const suppressionValue_;
	QDialogButtonBox *const buttonBox_;
	QPushButton *const apply_;
	QPushButton *const ok_;
};

AutoMotionDialog::AutoMotionDialog(AutoMotionConfigurator &configurator, obs_source_t *source, QWidget *parent)
	: QDialog(parent),
	  source_(source),
	  configurator_(configurator),
	  timer_(new QTimer(this)),
	  instructions_(new QLabel(this)),
	  status_(new QLabel(this)),
	  progress_(new QProgressBar(this)),
	  start_(new QPushButton(this)),
	  suppressionSlider_(new QSlider(Qt::Horizontal, this)),
	  suppressionValue_(new QDoubleSpinBox(this)),
	  buttonBox_(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
					  this)),
	  apply_(buttonBox_->button(QDialogButtonBox::Apply)),
	  ok_(buttonBox_->button(QDialogButtonBox::Ok))
{
	setupUi();
	if (!source_) {
		start_->setEnabled(false);
		status_->setText(tNoMotionInput());
	}
}

void AutoMotionDialog::setupUi()
{
	setWindowTitle(tAutoMotionTitle());
	setObjectName("autoMotionDialog");
	setMinimumWidth(440);
	auto *layout = new QVBoxLayout(this);
	instructions_->setText(tAutoMotionInstructions());
	instructions_->setWordWrap(true);
	status_->setWordWrap(true);
	start_->setText(tStartMotionMeasurement());
	progress_->setRange(0, kMeasurementMs);
	progress_->setValue(0);
	layout->addWidget(instructions_);
	layout->addWidget(start_);
	layout->addWidget(progress_);
	layout->addWidget(status_);
	suppressionSlider_->setRange(-5000, 5000);
	suppressionSlider_->setSingleStep(1);
	suppressionSlider_->setPageStep(100);
	suppressionSlider_->setAccessibleName(tMotionSuppression());
	suppressionValue_->setRange(-50.0, 50.0);
	suppressionValue_->setDecimals(2);
	suppressionValue_->setSingleStep(0.01);
	suppressionValue_->setAccessibleName(tMotionSuppression());
	suppressionSlider_->setEnabled(false);
	suppressionValue_->setEnabled(false);
	auto *adjustmentLayout = new QHBoxLayout;
	adjustmentLayout->addWidget(suppressionSlider_, 1);
	adjustmentLayout->addWidget(suppressionValue_);
	layout->addLayout(adjustmentLayout);
	apply_->setEnabled(false);
	ok_->setEnabled(false);
	layout->addWidget(buttonBox_);
	connect(suppressionSlider_, &QSlider::valueChanged, this,
		[this](int value) { suppressionValue_->setValue(value / 100.0); });
	connect(suppressionValue_, &QDoubleSpinBox::valueChanged, this,
		[this](double value) { suppressionSlider_->setValue(qRound(value * 100.0)); });
	connect(apply_, &QPushButton::clicked, this, &AutoMotionDialog::applySettings);
	connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(start_, &QPushButton::clicked, this, [this] { startMeasurement(); });
	connect(timer_, &QTimer::timeout, this, [this] { updateMeasurement(); });
	timer_->setInterval(50);
}

AutoMotionDialog::~AutoMotionDialog()
{
	stopMeasurement();
}

void AutoMotionDialog::startMeasurement()
{
	suppressionSlider_->setEnabled(false);
	suppressionValue_->setEnabled(false);
	apply_->setEnabled(false);
	ok_->setEnabled(false);
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
	suppressionValue_->setValue(*recommendation_);
	suppressionSlider_->setEnabled(true);
	suppressionValue_->setEnabled(true);
	apply_->setEnabled(true);
	ok_->setEnabled(true);
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
	obs_source_update_properties(source_);
}

void AutoMotionDialog::applySettings()
{
	if (!recommendation_)
		return;
	applyValue(suppressionValue_->value());
}

void AutoMotionDialog::done(int result)
{
	stopMeasurement();
	if (result == QDialog::Accepted) {
		if (!recommendation_)
			return;
		applySettings();
	}
	QDialog::done(result);
}

AutoMotionConfigurator::~AutoMotionConfigurator()
{
	stopMeasurement();
}

auto AutoMotionConfigurator::show(std::shared_ptr<void> owner) -> bool
{
	if (dialog_) {
		dialog_->raise();
		dialog_->activateWindow();
		return false;
	}
	auto *dialog = new AutoMotionDialog(*this, source_, QApplication::activeWindow());
	dialog_ = dialog;
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	// Keep the embedded configurator's owner alive until the dialog is destroyed.
	QObject::connect(dialog, &QObject::destroyed, [owner = std::move(owner)] {});
	dialog->show();
	return false;
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
