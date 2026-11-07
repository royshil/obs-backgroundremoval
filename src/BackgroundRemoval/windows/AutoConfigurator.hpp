// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef PLUGIN_PRIVATE
#define PLUGIN_PRIVATE private
#endif

#include <QDialog>
#include <QElapsedTimer>
#include <obs.hpp>
#include <optional>

class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QTimer;

namespace BackgroundRemoval {

class FilterContext;

class AutoConfigurator final : public QDialog {
public:
	explicit AutoConfigurator(obs_source_t *source, QWidget *parent = nullptr);
	~AutoConfigurator() override;

protected:
	void done(int result) override;

	PLUGIN_PRIVATE : void setupUi();
	void startMeasurement();
	void updateMeasurement();
	void stopMeasurement();
	void applyValue(double value);
	void applySettings();

	OBSSource source_;
	FilterContext *context_ = nullptr; // Kept alive by source_'s OBS reference.
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

} // namespace BackgroundRemoval
