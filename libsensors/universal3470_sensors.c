/*
 * Copyright (C) 2021 Peter Schelchshorn (html6405) <peter.schelchshorn@gmail.com>
 *
 * This program is based on the work of Paul Kocialkowski <contact@paulk.fr>
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
#include <poll.h>
#include <sys/select.h>
#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "universal3470_sensors"
#include <utils/Log.h>

#include "universal3470_sensors.h"

/*
 * Sensors list
 */

struct sensor_t universal3470_sensors[] = {
       { "K2HH Acceleration Sensor", "STMicroelectronics", 1, SENSOR_TYPE_ACCELEROMETER,
               SENSOR_TYPE_ACCELEROMETER, RANGE_A, RESOLUTION_A, 0.20f, 10000, 0, 0, SENSOR_STRING_TYPE_ACCELEROMETER, 0, 0,
               SENSOR_FLAG_ON_CHANGE_MODE, {}, },
};

int universal3470_sensors_count = sizeof(universal3470_sensors) / sizeof(struct sensor_t);

struct universal3470_sensors_handlers *universal3470_sensors_handlers[] = {
	&k2hh_acceleration,
};

int universal3470_sensors_handlers_count = sizeof(universal3470_sensors_handlers) /
	sizeof(struct universal3470_sensors_handlers *);


int universal3470_sensors_activate(struct sensors_poll_device_t *dev, int handle,
	int enabled)
{
	struct universal3470_sensors_device *device;
	int i;

	ALOGD("%s(%p, %d, %d)", __func__, dev, handle, enabled);

	if (dev == NULL)
		return -EINVAL;

	device = (struct universal3470_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0)
		return -EINVAL;

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == handle) {
			if (enabled && device->handlers[i]->activate != NULL) {
				device->handlers[i]->needed |= UNIVERSAL3470_SENSORS_NEEDED_API;
				if (device->handlers[i]->needed == UNIVERSAL3470_SENSORS_NEEDED_API)
					return device->handlers[i]->activate(device->handlers[i]);
				else
					return 0;
			} else if (!enabled && device->handlers[i]->deactivate != NULL) {
				device->handlers[i]->needed &= ~UNIVERSAL3470_SENSORS_NEEDED_API;
				if (device->handlers[i]->needed == 0)
					return device->handlers[i]->deactivate(device->handlers[i]);
				else
					return 0;
			}
		}
	}

	return -1;
}

int universal3470_sensors_set_delay(struct sensors_poll_device_t *dev, int handle,
	int64_t ns)
{
	struct universal3470_sensors_device *device;
	int i;

	ALOGD("%s(%p, %d, %" PRId64 ")", __func__, dev, handle, ns);

	if (dev == NULL)
		return -EINVAL;

	device = (struct universal3470_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0)
		return -EINVAL;

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == handle && device->handlers[i]->set_delay != NULL)
			return device->handlers[i]->set_delay(device->handlers[i], ns);
	}

	return 0;
}

int mFlushed;

static int universal3470_sensors_batch(struct sensors_poll_device_1 *dev, int handle,
        int flags, int64_t period_ns, int64_t timeout) {
	(void)flags;
	(void)timeout;
	universal3470_sensors_set_delay((struct sensors_poll_device_t *)dev, handle, period_ns);
	return 0;
}

static int universal3470_sensors_flush(struct sensors_poll_device_1 *dev, int handle) {
	(void)dev;
	mFlushed |= (1 << handle);
	ALOGD("%s: handle: %d", __func__, handle);
	return 0;
}

int universal3470_sensors_poll(struct sensors_poll_device_t *dev,
	struct sensors_event_t* data, int count)
{
	struct universal3470_sensors_device *device;
	int i, j;
	int c, n;
	int poll_rc, rc;

//	ALOGD("%s(%p, %p, %d)", __func__, dev, data, count);

	if (dev == NULL)
		return -EINVAL;

	device = (struct universal3470_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0 ||
		device->poll_fds == NULL || device->poll_fds_count <= 0)
		return -EINVAL;

	n = 0;

	do {
		poll_rc = poll(device->poll_fds, device->poll_fds_count, n > 0 ? 0 : -1);
		if (poll_rc < 0) {
			ALOGE("%s: poll failed, rc=%d", __func__, poll_rc);
			return 0;
		}

		for (i = 0; i < device->poll_fds_count; i++) {
			if (!(device->poll_fds[i].revents & POLLIN))
				continue;

			for (j = 0; j < device->handlers_count; j++) {
				if (device->handlers[j] == NULL || device->handlers[j]->poll_fd != device->poll_fds[i].fd || device->handlers[j]->get_data == NULL)
					continue;

				rc = device->handlers[j]->get_data(device->handlers[j], &data[n]);
				if (rc < 0) {
					device->poll_fds[i].revents = 0;
					poll_rc = -1;
				} else {
					n++;
					count--;
				}
			}
		}
	} while ((poll_rc > 0 || n < 1) && count > 0);

	return n;
}

/*
 * Interface
 */

int universal3470_sensors_close(hw_device_t *device)
{
	struct universal3470_sensors_device *universal3470_sensors_device;
	int i;

	ALOGD("%s(%p)", __func__, device);

	if (device == NULL)
		return -EINVAL;

	universal3470_sensors_device = (struct universal3470_sensors_device *) device;

	if (universal3470_sensors_device->poll_fds != NULL)
		free(universal3470_sensors_device->poll_fds);

	for (i = 0; i < universal3470_sensors_device->handlers_count; i++) {
		if (universal3470_sensors_device->handlers[i] == NULL || universal3470_sensors_device->handlers[i]->deinit == NULL)
			continue;

		universal3470_sensors_device->handlers[i]->deinit(universal3470_sensors_device->handlers[i]);
	}

	free(device);

	return 0;
}

int universal3470_sensors_open(const struct hw_module_t* module, const char *id,
	struct hw_device_t** device)
{
	struct universal3470_sensors_device *universal3470_sensors_device;
	int p, i;

	ALOGD("%s(%p, %s, %p)", __func__, module, id, device);

	if (module == NULL || device == NULL)
		return -EINVAL;

	universal3470_sensors_device = (struct universal3470_sensors_device *)
		calloc(1, sizeof(struct universal3470_sensors_device));
	universal3470_sensors_device->device.common.tag = HARDWARE_DEVICE_TAG;
	universal3470_sensors_device->device.common.version = SENSORS_DEVICE_API_VERSION_1_3;
	universal3470_sensors_device->device.common.module = (struct hw_module_t *) module;
	universal3470_sensors_device->device.common.close = universal3470_sensors_close;
	universal3470_sensors_device->device.activate = universal3470_sensors_activate;
	universal3470_sensors_device->device.setDelay = universal3470_sensors_set_delay;
	universal3470_sensors_device->device.poll = universal3470_sensors_poll;
	universal3470_sensors_device->device.batch = universal3470_sensors_batch;
	universal3470_sensors_device->device.flush = universal3470_sensors_flush;
	universal3470_sensors_device->handlers = universal3470_sensors_handlers;
	universal3470_sensors_device->handlers_count = universal3470_sensors_handlers_count;
	universal3470_sensors_device->poll_fds = (struct pollfd *)
		calloc(1, universal3470_sensors_handlers_count * sizeof(struct pollfd));

	p = 0;
	for (i = 0; i < universal3470_sensors_handlers_count; i++) {
		if (universal3470_sensors_handlers[i] == NULL || universal3470_sensors_handlers[i]->init == NULL)
			continue;

		universal3470_sensors_handlers[i]->init(universal3470_sensors_handlers[i], universal3470_sensors_device);
		if (universal3470_sensors_handlers[i]->poll_fd >= 0) {
			universal3470_sensors_device->poll_fds[p].fd = universal3470_sensors_handlers[i]->poll_fd;
			universal3470_sensors_device->poll_fds[p].events = POLLIN;
			p++;
		}
	}

	universal3470_sensors_device->poll_fds_count = p;

	*device = &(universal3470_sensors_device->device.common);

	return 0;
}

int universal3470_sensors_get_sensors_list(struct sensors_module_t* module,
	const struct sensor_t **sensors_p)
{
	ALOGD("%s(%p, %p)", __func__, module, sensors_p);

	if (sensors_p == NULL)
		return -EINVAL;

	*sensors_p = universal3470_sensors;
	return universal3470_sensors_count;
}

struct hw_module_methods_t universal3470_sensors_module_methods = {
	.open = universal3470_sensors_open,
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.version_major = 1,
		.version_minor = 0,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "UNIVERSAL3470 Sensors",
		.author = "Peter Schelchshorn",
		.methods = &universal3470_sensors_module_methods,
	},
	.get_sensors_list = universal3470_sensors_get_sensors_list,
};
