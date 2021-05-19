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

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <linux/ioctl.h>
#include <linux/input.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "universal3470_sensors"
#include <utils/Log.h>

#include "universal3470_sensors.h"

struct k2hh_acceleration_data {
	char path_enable[PATH_MAX];
	char path_delay[PATH_MAX];

	sensors_vec_t acceleration;
};

int k2hh_acceleration_init(struct universal3470_sensors_handlers *handlers,
	struct universal3470_sensors_device *device)
{
	struct k2hh_acceleration_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int input_fd = -1;
	int rc;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct k2hh_acceleration_data *) calloc(1, sizeof(struct k2hh_acceleration_data));

	input_fd = input_open("accelerometer_sensor");
	if (input_fd < 0) {
		ALOGE("%s: Unable to open input", __func__);
		goto error;
	}

	rc = sysfs_path_prefix("accelerometer_sensor", (char *) &path);
	if (rc < 0 || path[0] == '\0') {
		ALOGE("%s: Unable to open sysfs", __func__);
		goto error;
	}

	snprintf(data->path_enable, PATH_MAX, "%s/enable", path);
	snprintf(data->path_delay, PATH_MAX, "%s/poll_delay", path);

	handlers->poll_fd = input_fd;
	handlers->data = (void *) data;

	return 0;

error:
	if (data != NULL)
		free(data);

	if (input_fd >= 0)
		close(input_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int k2hh_acceleration_deinit(struct universal3470_sensors_handlers *handlers)
{
	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL)
		return -EINVAL;

	if (handlers->poll_fd >= 0)
		close(handlers->poll_fd);
	handlers->poll_fd = -1;

	if (handlers->data != NULL)
		free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int k2hh_acceleration_activate(struct universal3470_sensors_handlers *handlers)
{
	struct k2hh_acceleration_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct k2hh_acceleration_data *) handlers->data;

	rc = sysfs_value_write(data->path_enable, 1);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int k2hh_acceleration_deactivate(struct universal3470_sensors_handlers *handlers)
{
	struct k2hh_acceleration_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct k2hh_acceleration_data *) handlers->data;

	rc = sysfs_value_write(data->path_enable, 0);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int k2hh_acceleration_set_delay(struct universal3470_sensors_handlers *handlers, int64_t delay)
{
	struct k2hh_acceleration_data *data;
	int rc;

	ALOGD("%s(%p, %" PRId64 ")", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct k2hh_acceleration_data *) handlers->data;

	rc = sysfs_value_write(data->path_delay, delay);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	return 0;
}

float k2hh_acceleration_convert(int value)
{
	return (float) (value) * RESOLUTION_A;
}

extern int mFlushed;

int k2hh_acceleration_get_data(struct universal3470_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct k2hh_acceleration_data *data;
	struct input_event input_event;
	int input_fd;
	int rc;
	int sensorId = SENSOR_TYPE_ACCELEROMETER;

//	ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	if (mFlushed & (1 << sensorId)) { /* Send flush META_DATA_FLUSH_COMPLETE immediately */
		sensors_event_t sensor_event;
		memset(&sensor_event, 0, sizeof(sensor_event));
		sensor_event.version = META_DATA_VERSION;
		sensor_event.type = SENSOR_TYPE_META_DATA;
		sensor_event.meta_data.sensor = sensorId;
		sensor_event.meta_data.what = 0;
		*event++ = sensor_event;
		mFlushed &= ~(0x01 << sensorId);
		ALOGD("AkmSensor: %s Flushed sensorId: %d", __func__, sensorId);
	}

	data = (struct k2hh_acceleration_data *) handlers->data;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -EINVAL;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->acceleration.x = data->acceleration.x;
	event->acceleration.y = data->acceleration.y;
	event->acceleration.z = data->acceleration.z;

	event->acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_REL) {
			switch (input_event.code) {
				case EVENT_TYPE_ACCEL_X:
					event->acceleration.x = k2hh_acceleration_convert(input_event.value);
					break;
				case EVENT_TYPE_ACCEL_Y:
					event->acceleration.y = k2hh_acceleration_convert(input_event.value);
					break;
				case EVENT_TYPE_ACCEL_Z:
					event->acceleration.z = k2hh_acceleration_convert(input_event.value);
					break;
				default:
					continue;
			}
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT)
				event->timestamp = input_timestamp(&input_event);
		}
	} while (input_event.type != EV_SYN);

	data->acceleration.x = event->acceleration.x;
	data->acceleration.y = event->acceleration.y;
	data->acceleration.z = event->acceleration.z;

	return 0;
}

struct universal3470_sensors_handlers k2hh_acceleration = {
	.name = "K2HH Acceleration",
	.handle = SENSOR_TYPE_ACCELEROMETER,
	.init = k2hh_acceleration_init,
	.deinit = k2hh_acceleration_deinit,
	.activate = k2hh_acceleration_activate,
	.deactivate = k2hh_acceleration_deactivate,
	.set_delay = k2hh_acceleration_set_delay,
	.get_data = k2hh_acceleration_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
