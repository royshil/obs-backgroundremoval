// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AutoConfigurator.hpp"

#include <algorithm>
#include <optional>

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

#include <util/platform.h>

#include "FilterContext.hpp"
#include "ModuleText.hpp"
#include "MotionMeasurement.hpp"

namespace BackgroundRemoval {
namespace {
constexpr int kMeasurementMs = 3000;
} // namespace

AutoConfigurator::AutoConfigurator(obs_source_t *source, QWidget *parent)
	: QDialog(parent),
	  source_(source),
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
	setAttribute(Qt::WA_DeleteOnClose);
	if (source_ && obs_source_get_type(source_) == OBS_SOURCE_TYPE_FILTER) {
		auto *context = static_cast<std::shared_ptr<FilterContext> *>(obs_obj_get_data(source_));
		if (context && *context)
			context_ = context->get();
	}
	setupUi();
	if (!context_) {
		start_->setEnabled(false);
		status_->setText(tNoMotionInput());
	}
}

void AutoConfigurator::setupUi()
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
	connect(apply_, &QPushButton::clicked, this, &AutoConfigurator::applySettings);
	connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(start_, &QPushButton::clicked, this, [this] { startMeasurement(); });
	connect(timer_, &QTimer::timeout, this, [this] { updateMeasurement(); });
	timer_->setInterval(50);
}

AutoConfigurator::~AutoConfigurator()
{
	stopMeasurement();
}

void AutoConfigurator::startMeasurement()
{
	suppressionSlider_->setEnabled(false);
	suppressionValue_->setEnabled(false);
	apply_->setEnabled(false);
	ok_->setEnabled(false);
	recommendation_.reset();
	stopMeasurement();
	context_->motionMeasurement_.requestStart();
	progress_->setValue(0);
	status_->setText(tMeasuringMotion());
	start_->setEnabled(false);
	elapsed_.start();
	timer_->start();
}

void AutoConfigurator::updateMeasurement()
{
	progress_->setValue(static_cast<int>(std::min<qint64>(elapsed_.elapsed(), kMeasurementMs)));
	if (elapsed_.elapsed() < kMeasurementMs)
		return;
	if (!context_->motionMeasurement_.readable()) {
		// An inactive source may not tick. Cancel without reading unpublished data.
		if (elapsed_.elapsed() < kMeasurementMs + 1500)
			return;
		stopMeasurement();
		start_->setEnabled(true);
		status_->setText(tNoMotionInput());
		return;
	}
	timer_->stop();
	recommendation_ = context_->motionMeasurement_.motionSuppression();
	start_->setText(tMeasureMotionAgain());
	start_->setEnabled(true);
	if (!recommendation_) {
		status_->setText(tNoMotionInput());
		return;
	}
	status_->setText(QString::fromUtf8(tMotionMeasurementResult())
				 .arg(*recommendation_, 0, 'f', 2)
				 .arg(static_cast<qulonglong>(context_->motionMeasurement_.samples().size())));
	suppressionValue_->setValue(*recommendation_);
	suppressionSlider_->setEnabled(true);
	suppressionValue_->setEnabled(true);
	apply_->setEnabled(true);
	ok_->setEnabled(true);
}

void AutoConfigurator::stopMeasurement()
{
	timer_->stop();
	if (context_)
		context_->motionMeasurement_.requestCancel();
}

void AutoConfigurator::applyValue(double value)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_double(settings, "v2_MotionSuppression", value);
	obs_source_update(source_, settings);
	obs_source_update_properties(source_);
}

void AutoConfigurator::applySettings()
{
	if (!recommendation_)
		return;
	applyValue(suppressionValue_->value());
}

void AutoConfigurator::done(int result)
{
	stopMeasurement();
	if (result == QDialog::Accepted) {
		if (!recommendation_)
			return;
		applySettings();
	}
	QDialog::done(result);
}

} // namespace BackgroundRemoval
