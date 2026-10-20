// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <memory>
#include <optional>
#include <obs.hpp>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace BackgroundRemoval {

class IRenderingPipeline;
class MotionMeasurement;

class AutoMotionConfigurator final : public QDialog {
public:
	AutoMotionConfigurator(obs_source_t *source, std::shared_ptr<IRenderingPipeline> pipeline,
			       QWidget *parent = nullptr);
	~AutoMotionConfigurator() override;
	static auto show(obs_source_t *source, std::shared_ptr<IRenderingPipeline> pipeline) -> bool;

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
	std::shared_ptr<IRenderingPipeline> pipeline_;
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

} // namespace BackgroundRemoval
