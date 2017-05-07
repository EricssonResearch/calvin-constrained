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
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include <string.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sockets.h"
#include "spiffs.h"
#include "esp_spiffs.h"
#include "espressif/esp_system.h"
#include "ssid_config.h"
#include "../../../../cc_api.h"
#include "../../../../runtime/north/cc_common.h"
#include "../../../../runtime/north/cc_node.h"
#include "../../transport/socket/cc_transport_socket.h"

#define CALVIN_RUNTIME_STATE_FILE "cc_state.conf"

void platform_init(void)
{
	uart_set_baud(0, 115200);

	log("----------------------------------------");
	log("SDK version:%s", sdk_system_get_sdk_version());
	log("%20s: %u b", "Heap size", sdk_system_get_free_heap_size());
	log("%20s: 0x%08x", "Flash ID", sdk_spi_flash_get_id());
	log("%20s: %u Mbytes", "Flash size", sdk_flashchip.chip_size / 1024 / 1024);
	log("----------------------------------------");

#ifdef USE_PERSISTENT_STORAGE
	esp_spiffs_init();
	if (esp_spiffs_mount() != SPIFFS_OK) {
    log_error("Failed to mount SPIFFS");

		SPIFFS_unmount(&fs);
		if (SPIFFS_format(&fs) == SPIFFS_OK)
			log("Format complete");
		else
			log_error("Format failed");

		if (esp_spiffs_mount() != SPIFFS_OK)
			log_error("Failed to mount SPIFFS");
	}
#endif

	// WIFI_SSID and WIFI_PASS should be defined in ssid_config.h
	struct sdk_station_config config = {
		.ssid = WIFI_SSID,
		.password = WIFI_PASS,
	};

	/* required to call wifi_set_opmode before station_set_config */
	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
}

void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

result_t platform_create(struct node_t* node)
{
	node->platform = NULL;
	return CC_RESULT_SUCCESS;
}

result_t platform_create_calvinsys(struct node_t *node)
{
	// TODO: Create calvinsys objects
	node->calvinsys = NULL;
	return CC_RESULT_SUCCESS;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

void *platform_mem_calloc(size_t nitems, size_t size)
{
	void *ptr = NULL;
	if (platform_mem_alloc(&ptr, nitems * size) != CC_RESULT_SUCCESS)
		return NULL;

	memset(ptr, 0, nitems * size);
	return ptr;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}

bool platform_evt_wait(struct node_t *node, uint32_t timeout_seconds)
{
	fd_set fds;
	int fd = 0;
	struct timeval tv, *tv_ref = NULL;

	if (timeout_seconds > 0) {
		tv.tv_sec = timeout_seconds;
		tv.tv_usec = 0;
		tv_ref = &tv;
	}

	FD_ZERO(&fds);

	if (node->transport_client != NULL && (node->transport_client->state == TRANSPORT_PENDING || node->transport_client->state == TRANSPORT_ENABLED)) {
		FD_SET(((transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, tv_ref);

		if (FD_ISSET(fd, &fds)) {
			if (transport_handle_data(node, node->transport_client, node_handle_message) != CC_RESULT_SUCCESS) {
				log_error("Failed to read data from transport");
				node->transport_client->state = TRANSPORT_DISCONNECTED;
			}
			return true;
		}
	} else {
		if (timeout_seconds > 0)
			vTaskDelay((timeout_seconds * 1000) / portTICK_PERIOD_MS);
	}

	return false;
}

result_t platform_stop(struct node_t* node)
{
	return CC_RESULT_SUCCESS;
}

result_t platform_node_started(struct node_t* node)
{
	return CC_RESULT_SUCCESS;
}

#ifdef USE_PERSISTENT_STORAGE
void platform_write_node_state(struct node_t* node, char *buffer, size_t size)
{
  spiffs_file fd = SPIFFS_open(&fs, CALVIN_RUNTIME_STATE_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	int res = 0;

	res = SPIFFS_write(&fs, fd, buffer, size);
	if (res != size)
		log_error("Failed to write runtime state, status '%d'", res);

	SPIFFS_close(&fs, fd);
}

result_t platform_read_node_state(struct node_t* node, char buffer[], size_t size)
{
	spiffs_file fd = SPIFFS_open(&fs, CALVIN_RUNTIME_STATE_FILE, SPIFFS_RDONLY, 0);
	if (fd < 0) {
		log_error("Error opening file");
		return CC_RESULT_FAIL;
	}

	SPIFFS_read(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	return CC_RESULT_SUCCESS;
}
#endif

#ifdef CC_PLATFORM_SLEEP
void platform_sleep(node_t *node)
{
	log("Enterring system deep sleep for '%d' seconds", CC_SLEEP_TIME);
	sdk_system_deep_sleep(CC_SLEEP_TIME * 1000 * 1000);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	log("----------- Should not come here --------------");
}
#endif

void calvin_task(void *pvParameters)
{
	node_t *node = NULL;

	platform_init();

	while (1) {
		while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			log("Waiting for connection to AP");
		}

		log("Connected to AP\n");

		if (api_runtime_init(&node, "esp8266", CALVIN_CB_URIS, NULL) == CC_RESULT_SUCCESS)
			api_runtime_start(node);

		api_runtime_stop(node);
	}
}

void user_init(void)
{
	// TODO: Set proper stack size and prio
	xTaskCreate(&calvin_task, "calvin_task", 2048, NULL, 2, NULL);
}
