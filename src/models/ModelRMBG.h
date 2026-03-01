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

#ifndef MODELRMBG_H
#define MODELRMBG_H

#include "Model.h"

class ModelRMBG : public ModelBCHW {
public:
	ModelRMBG(/* args */) {}
	~ModelRMBG() {}

	bool populateInputOutputShapes(const std::unique_ptr<Ort::Session> &session,
				       std::vector<std::vector<int64_t>> &inputDims,
				       std::vector<std::vector<int64_t>> &outputDims)
	{
		ModelBCHW::populateInputOutputShapes(session, inputDims, outputDims);

		// fix the output width and height to the input width and height
		outputDims[0].at(2) = inputDims[0].at(2);
		outputDims[0].at(3) = inputDims[0].at(3);

		return true;
	}
};

#endif // MODELRMBG_H
