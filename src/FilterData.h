/*
 * SPDX-FileCopyrightText: Copyright (C) 2021 Roy Shilkrot roy.shil@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FILTERDATA_H
#define FILTERDATA_H

#include <obs-module.h>

#include <atomic>
#include <memory>

#include "models/Model.h"
#include "ort-utils/ORTModelData.h"

/**
  * @brief The filter_data struct
  *
  * This struct is used to store the base data needed for ORT filters.
  *
*/
struct filter_data : public ORTModelData, public std::enable_shared_from_this<filter_data> {
	std::string useGPU;
	uint32_t numThreads;
	std::string modelSelection;
	std::unique_ptr<Model> model;

	obs_source_t *source;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;

	cv::Mat inputBGRA;

	std::atomic<bool> isDisabled{false};

	std::mutex inputBGRALock;
	std::mutex outputLock;

#if _WIN32
	std::wstring modelFilepath;
#else
	std::string modelFilepath;
#endif
};

#endif /* FILTERDATA_H */
