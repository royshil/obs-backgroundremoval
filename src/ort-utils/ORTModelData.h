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

#ifndef ORTMODELDATA_H
#define ORTMODELDATA_H

#include <onnxruntime_cxx_api.h>

struct ORTModelData {
	std::unique_ptr<Ort::Session> session;
	std::unique_ptr<Ort::Env> env;
	std::vector<Ort::AllocatedStringPtr> inputNames;
	std::vector<Ort::AllocatedStringPtr> outputNames;
	std::vector<Ort::Value> inputTensor;
	std::vector<Ort::Value> outputTensor;
	std::vector<std::vector<int64_t>> inputDims;
	std::vector<std::vector<int64_t>> outputDims;
	std::vector<std::vector<float>> outputTensorValues;
	std::vector<std::vector<float>> inputTensorValues;
};

#endif /* ORTMODELDATA_H */
