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

#ifndef ORT_SESSION_UTILS_H
#define ORT_SESSION_UTILS_H

#include <opencv2/core/types.hpp>

#include "FilterData.h"

#define OBS_BGREMOVAL_ORT_SESSION_ERROR_FILE_NOT_FOUND 1
#define OBS_BGREMOVAL_ORT_SESSION_ERROR_INVALID_MODEL 2
#define OBS_BGREMOVAL_ORT_SESSION_ERROR_INVALID_INPUT_OUTPUT 3
#define OBS_BGREMOVAL_ORT_SESSION_ERROR_STARTUP 5
#define OBS_BGREMOVAL_ORT_SESSION_SUCCESS 0

int createOrtSession(filter_data *tf);

bool runFilterModelInference(filter_data *tf, const cv::Mat &imageBGRA, cv::Mat &output);

#endif /* ORT_SESSION_UTILS_H */
