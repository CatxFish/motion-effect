/*
*	motion-filter, an OBS-Studio filter plugin for animating sources using
*	transform manipulation on the scene.
*	Copyright(C) <2018>  <CatxFish>
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; either version 2 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License along
*	with this program; if not, write to the Free Software Foundation, Inc.,
*	51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.
*/

#pragma once

#include <obs-module.h>

obs_sceneitem_t *get_item(obs_source_t* context,const char* name);
obs_sceneitem_t* get_item_by_id(obs_source_t* context,int64_t id);
int64_t get_item_id(obs_source_t* context, const char* name);

bool cal_size(obs_sceneitem_t* item, float sx, float sy, int* width, 
	int* height);

bool cal_scale(obs_sceneitem_t* item, float* sx, float*sy, int width, 
	int height);

void set_item_scale(obs_sceneitem_t* item, int width, int height);

obs_hotkey_id register_hotkey(obs_source_t *context, obs_source_t *scene,
	const char *name, const char *text, obs_hotkey_func func, void *data);

void save_hotkey_config(obs_hotkey_id id, obs_data_t *settings,
	const char *name);