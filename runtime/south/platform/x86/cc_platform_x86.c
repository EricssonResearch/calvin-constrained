/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cc_config.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/south/transport/socket/cc_transport_socket.h"
#include "runtime/north/cc_transport.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/cc_common.h"
#include "calvinsys/cc_calvinsys.h"
#include "runtime/north/coder/cc_coder.h"
#ifdef CC_USE_LIBCOAP_CLIENT
#include "calvinsys/cc_libcoap_client.h"
#endif

void cc_platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

void cc_platform_early_init(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec + getpid());
}

cc_result_t cc_platform_late_init(cc_node_t *node, const char *args)
{
	cc_result_t result = CC_SUCCESS;

#ifdef CC_USE_LIBCOAP_CLIENT
	if (args != NULL)
		result = cc_libcoap_create(node->calvinsys, args);
#endif

	return result;
}

cc_result_t cc_platform_stop(cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_platform_evt_wait_status_t cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
{
	int transport_fd = 0, res = 0, max_fd = -1, i = 0;
	struct timeval tv, *tv_ref = NULL;
	cc_calvinsys_t *sys = node->calvinsys;

	if (timeout_seconds > 0) {
		tv.tv_sec = timeout_seconds;
		tv.tv_usec = 0;
		tv_ref = &tv;
	}

	FD_ZERO(&node->fds);

	if (node->transport_client != NULL && (node->transport_client->state == CC_TRANSPORT_PENDING || node->transport_client->state == CC_TRANSPORT_ENABLED)) {
		transport_fd = ((cc_transport_socket_client_t *)node->transport_client->client_state)->fd;
		max_fd = transport_fd;
		FD_SET(transport_fd, &node->fds);
	}

	for (i = 0; i < CC_CALVINSYS_MAX_FDS; i++) {
		if (sys->fds[i] != -1) {
			FD_SET(sys->fds[i], &node->fds);
			if (sys->fds[i] > max_fd)
				max_fd = sys->fds[i];
		}
	}

	if (max_fd >= 0) {
		res = select(max_fd + 1, &node->fds, NULL, NULL, tv_ref);
		if (res < 0) {
			cc_log_error("select failed");
			return CC_PLATFORM_EVT_WAIT_FAIL;
		} else if (res == 0)
			cc_log_debug("Timeout waiting for data");
		else {
			if (FD_ISSET(transport_fd, &node->fds)) {
				if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS) {
					cc_log_error("Failed to handle received data");
					return CC_PLATFORM_EVT_WAIT_FAIL;
				}
			}
			return CC_PLATFORM_EVT_WAIT_DATA_READ;
		}
	} else
		select(0, NULL, NULL, NULL, tv_ref);

	return CC_PLATFORM_EVT_WAIT_TIMEOUT;
}

cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void *cc_platform_mem_calloc(size_t nitems, size_t size)
{
	void *ptr = NULL;

	if (cc_platform_mem_alloc(&ptr, nitems * size) != CC_SUCCESS)
		return NULL;

	memset(ptr, 0, nitems * size);
	return ptr;
}

void cc_platform_mem_free(void *buffer)
{
	free(buffer);
}

uint32_t cc_platform_get_time(void)
{
	struct timeval value;

	gettimeofday(&value, NULL);

	return value.tv_sec;
}

#if CC_USE_SLEEP
void cc_platform_deepsleep(uint32_t time_in_us)
{
	exit(0);
}
#endif

#if CC_USE_STORAGE
cc_stat_t cc_platform_file_stat(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) == 0) {
		if (S_ISDIR(statbuf.st_mode))
			return CC_STAT_DIR;
		if (S_ISREG(statbuf.st_mode))
			return CC_STAT_FILE;
	}

	return CC_STAT_NO_EXIST;
}

cc_result_t cc_platform_file_read(const char *path, char **buffer, size_t *len)
{
	FILE *fp = NULL;
	size_t read = 0;

	fp = fopen(path, "r+");
	if (fp == NULL) {
		cc_log_error("Failed to open '%s'", path);
		return CC_FAIL;
	}

	fseek(fp, 0, SEEK_END);
	*len = ftell(fp);

	if (cc_platform_mem_alloc((void **)buffer, *len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(*buffer, 0, *len);

	rewind(fp);
	read = fread(*buffer, 1, *len, fp);
	fclose(fp);

	if (read != *len) {
		cc_platform_mem_free(*buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_platform_create_dirs(const char *path)
{
	int i = 1, len = strlen(path);
	char *tmp = NULL;

	while (i < len) {
		if (path[i] == '/') {
			if (cc_platform_mem_alloc((void **)&tmp, i + 1) != CC_SUCCESS) {
				cc_log_error("Failed to allocate memory");
				return CC_FAIL;
			}
			strncpy(tmp, path, i);
			tmp[i] = '\0';
			if (cc_platform_file_stat(tmp) == CC_STAT_NO_EXIST) {
				if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
					cc_log_error("Failed to create '%s'", tmp);
					cc_platform_mem_free(tmp);
					return CC_FAIL;
				}
			}
			cc_platform_mem_free(tmp);
		}
		i++;
	}
	return CC_SUCCESS;
}

cc_result_t cc_platform_file_write(const char *path, char *buffer, size_t size)
{
	FILE *fp = NULL;
	int len = 0;

	if (cc_platform_create_dirs(path) != CC_SUCCESS)
		return CC_FAIL;

	fp = fopen(path, "w+");
	if (fp == NULL)
		return CC_FAIL;

	len = fwrite(buffer, 1, size, fp);
	if (len != size) {
		cc_log_error("Failed to write node config");
		fclose(fp);
		return CC_FAIL;
	}

	fclose(fp);

	return CC_SUCCESS;
}

cc_result_t cc_platform_file_del(const char *path)
{
	if (unlink(path) < 0)
		return CC_FAIL;

	return CC_SUCCESS;
}
#endif
