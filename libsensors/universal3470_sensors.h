/*
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <poll.h>
#include <linux/input.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#ifndef _UNIVERSAL3470_SENSORS_H_
#define _UNIVERSAL3470_SENSORS_H_

#define UNIVERSAL3470_SENSORS_NEEDED_API		(1 << 0)

/* conversion of orientation data to degree units */
#define CONVERT_O                   (1.0f/1000.0f)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (CONVERT_O)

#define LSG                         (1000.0f)
#define RANGE_A                     (4*GRAVITY_EARTH)
#define RESOLUTION_A                (GRAVITY_EARTH / LSG / 20)

/* For k2hh */
#define EVENT_TYPE_ACCEL_X          REL_X
#define EVENT_TYPE_ACCEL_Y          REL_Y
#define EVENT_TYPE_ACCEL_Z          REL_Z

struct universal3470_sensors_device;

struct universal3470_sensors_handlers {
	char *name;
	int handle;

	int (*init)(struct universal3470_sensors_handlers *handlers,
		struct universal3470_sensors_device *device);
	int (*deinit)(struct universal3470_sensors_handlers *handlers);
	int (*activate)(struct universal3470_sensors_handlers *handlers);
	int (*deactivate)(struct universal3470_sensors_handlers *handlers);
	int (*set_delay)(struct universal3470_sensors_handlers *handlers,
		int64_t delay);
	int (*get_data)(struct universal3470_sensors_handlers *handlers,
		struct sensors_event_t *event);

	int activated;
	int needed;
	int poll_fd;

	void *data;
};

struct universal3470_sensors_device {
	sensors_poll_device_1_t  device;

	struct universal3470_sensors_handlers **handlers;
	int handlers_count;

	struct pollfd *poll_fds;
	int poll_fds_count;
};

extern struct universal3470_sensors_handlers *universal3470_sensors_handlers[];
extern int universal3470_sensors_handlers_count;

int universal3470_sensors_activate(struct sensors_poll_device_t *dev, int handle,
	int enabled);
int universal3470_sensors_set_delay(struct sensors_poll_device_t *dev, int handle,
	int64_t ns);
int universal3470_sensors_poll(struct sensors_poll_device_t *dev,
	struct sensors_event_t* data, int count);

/*
 * Input
 */

void input_event_set(struct input_event *event, int type, int code, int value);
int64_t timestamp(struct timeval *time);
int64_t input_timestamp(struct input_event *event);
int uinput_rel_create(const char *name);
void uinput_destroy(int uinput_fd);
int input_open(char *name);
int sysfs_path_prefix(char *name, char *path_prefix);
int64_t sysfs_value_read(char *path);
int sysfs_value_write(char *path, int64_t value);
int sysfs_string_read(char *path, char *buffer, size_t length);
int sysfs_string_write(char *path, char *buffer, size_t length);

extern struct universal3470_sensors_handlers k2hh_acceleration;

#endif
