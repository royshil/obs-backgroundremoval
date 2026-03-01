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

#ifndef OBS_CONFIG_UTILS_H
#define OBS_CONFIG_UTILS_H

enum {
	OBS_BGREMOVAL_CONFIG_SUCCESS = 0,
	OBS_BGREMOVAL_CONFIG_FAIL = 1,
};

/**
 * Get a boolean flasg from the module configuration file.
 *
 * @param name The name of the config item.
 * @param returnValue The value of the config item.
 * @param defaultValue The default value of the config item.
 * @return OBS_BGREMOVAL_CONFIG_SUCCESS if the config item was found,
 * OBS_BGREMOVAL_CONFIG_FAIL otherwise.
 */
int getFlagFromConfig(const char *name, bool *returnValue, bool defaultValue);

/**
 * Set a boolean flag in the module configuration file.
 *
 * @param name The name of the config item.
 * @param value The value of the config item.
 * @return OBS_BGREMOVAL_CONFIG_SUCCESS if the config item was found,
 * OBS_BGREMOVAL_CONFIG_FAIL otherwise.
 */
int setFlagInConfig(const char *name, const bool value);

#endif /* OBS_CONFIG_UTILS_H */
