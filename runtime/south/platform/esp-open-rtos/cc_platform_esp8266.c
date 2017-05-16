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
#include "dhcpserver.h"
#include "lwip/sockets.h"
#include "spiffs.h"
#include "esp_spiffs.h"
#include "espressif/esp_system.h"
#include "../../../../cc_api.h"
#include "../../../../runtime/north/cc_common.h"
#include "../../../../runtime/north/cc_node.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../../runtime/north/cc_msgpack_helper.c"
#include "calvinsys/cc_calvinsys_ds18b20.h"

#define CALVIN_ESP_RUNTIME_STATE_FILE		"cc_state.conf"
#define CALVIN_ESP_WIFI_CONFIG_FILE		"cc_wifi.conf"
#define CALVIN_ESP_WIFI_CONFIG_BUFFER_SIZE	200
#define CALVIN_ESP_WIFI_RECV_CONFIG_BUFFER_SIZE	600
#define CALVIN_ESP_AP_SSID			"calvin-esp"
#define CALVIN_ESP_AP_PSK			"calvin-esp"

static result_t platform_esp_start_station_mode()
{
	spiffs_file fd;
	char buffer[CALVIN_ESP_WIFI_CONFIG_BUFFER_SIZE], *ssid = NULL, *password = NULL;
	uint32_t ssid_len = 0, password_len = 0;
	struct sdk_station_config config;

	fd = SPIFFS_open(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, SPIFFS_RDONLY, 0);
	if (fd < 0) {
		log_error("Failed to open config file");
		return CC_RESULT_FAIL;
	}

	if (SPIFFS_read(&fs, fd, buffer, CALVIN_ESP_WIFI_CONFIG_BUFFER_SIZE) < 0) {
		log_error("Failed to read file");
		SPIFFS_close(&fs, fd);
		return CC_RESULT_FAIL;
	}

	SPIFFS_close(&fs, fd);

	if (decode_string_from_map(buffer, "ssid", &ssid, &ssid_len) != CC_RESULT_SUCCESS) {
		log_error("Failed to read 'ssid' from config");
		return CC_RESULT_FAIL;
	}

	if (decode_string_from_map(buffer, "password", &password, &password_len) != CC_RESULT_SUCCESS) {
		log_error("Failed to read 'password' from config");
		return CC_RESULT_FAIL;
	}

	strncpy((char *)config.ssid, ssid, ssid_len);
	config.ssid[ssid_len] = '\0';
	strncpy((char *)config.password, password, password_len);
	config.password[password_len] = '\0';

	log("Starting in station mode with AP '%s'", config.ssid);

	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);

	return CC_RESULT_SUCCESS;
}

static result_t platform_esp_write_calvin_config(char *name, uint32_t name_len, char *uri, uint32_t uri_len)
{
	spiffs_file fd;
	int res = 0;
	char buffer[CALVIN_ESP_WIFI_CONFIG_BUFFER_SIZE];
	char *tmp = buffer;
	size_t size = 0;
	char id[UUID_BUFFER_SIZE];

	fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	if (fd < 0) {
		log_error("Failed to open config file");
		return CC_RESULT_FAIL;
	}

	gen_uuid(id, NULL);

	tmp = mp_encode_map(tmp, 3);
	{
		tmp = encode_str(&tmp, "id", id, strlen(id));
		tmp = encode_str(&tmp, "name", name, name_len);
		tmp = encode_str(&tmp, "proxy_uri", uri, uri_len);
	}

	size = tmp - buffer;
	res = SPIFFS_write(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	if (res != size) {
		log_error("Failed to write runtime config, status '%d'", res);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

static result_t platform_esp_write_wifi_config(char *ssid, uint32_t ssid_len, char *password, uint32_t password_len)
{
	spiffs_file fd;
	int res = 0;
	char buffer[CALVIN_ESP_WIFI_CONFIG_BUFFER_SIZE];
	char *tmp = buffer;
	size_t size = 0;

	fd = SPIFFS_open(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	if (fd < 0) {
		log_error("Failed to open config file");
		return CC_RESULT_FAIL;
	}

	tmp = mp_encode_map(tmp, 2);
	{
		tmp = encode_str(&tmp, "ssid", ssid, ssid_len);
		tmp = encode_str(&tmp, "password", password, password_len);
	}

	size = tmp - buffer;
	res = SPIFFS_write(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	if (res != size) {
		log_error("Failed to write runtime wifi config, status '%d'", res);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

static result_t platform_esp_start_ap_mode()
{
	int sockfd = 0, newsockfd = 0, clilen = 0, len = 0;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[CALVIN_ESP_WIFI_RECV_CONFIG_BUFFER_SIZE];
	char *name = NULL, *uri = NULL, *ssid = NULL, *password = NULL;
	struct ip_info ap_ip;
	struct sdk_softap_config ap_config = {
		.ssid = CALVIN_ESP_AP_SSID,
		.ssid_hidden = 0,
		.channel = 3,
		.ssid_len = strlen(CALVIN_ESP_AP_SSID),
		.authmode = AUTH_WPA_WPA2_PSK,
		.password = CALVIN_ESP_AP_PSK,
		.max_connection = 3,
		.beacon_interval = 100,
	};

	log("Starting in AP mode, listening for config on '172.16.0.1:5003'");

	sdk_wifi_set_opmode(SOFTAP_MODE);

	IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
	IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
	IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);

	sdk_wifi_set_ip_info(1, &ap_ip);
	sdk_wifi_softap_set_config(&ap_config);

	ip_addr_t first_client_ip;
	IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
	dhcpserver_start(&first_client_ip, 4);

	// Wait for configuration data from client
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		log_error("Failed to open socket");
		return CC_RESULT_FAIL;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(5003);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		log_error("Failed to bind socket");
		return CC_RESULT_FAIL;
	}

	listen(sockfd, 1);
	clilen = sizeof(cli_addr);

	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
	if (newsockfd < 0) {
		log_error("Failed to accept client");
		return CC_RESULT_FAIL;
	}

	bzero(buffer, CALVIN_ESP_WIFI_RECV_CONFIG_BUFFER_SIZE);
	len = read(newsockfd, buffer, CALVIN_ESP_WIFI_RECV_CONFIG_BUFFER_SIZE);
	if (len < 0) {
		log_error("Failed to read data");
		close(newsockfd);
		return CC_RESULT_FAIL;
	}

	name = strstr(buffer, "\"name\": \"");
	if (name == NULL) {
		log_error("Failed to parse 'name' from configuration data");
		close(newsockfd);
		return CC_RESULT_FAIL;
	}
	name = name + 9;

	uri = strstr(buffer, "\"proxy_uri\": \"");
	if (uri == NULL) {
		log_error("Failed to parse 'uri' from configuration data");
		close(newsockfd);
		return CC_RESULT_FAIL;
	}
	uri = uri + 14;

	ssid = strstr(buffer, "\"ssid\": \"");
	if (ssid == NULL) {
		log_error("Failed to parse 'ssid' from configuration data");
		close(newsockfd);
		return CC_RESULT_FAIL;
	}
	ssid = ssid + 9;

	password = strstr(buffer, "\"password\": \"");
	if (password == NULL) {
		log_error("Failed to parse 'password' from configuration data");
		close(newsockfd);
		return CC_RESULT_FAIL;
	}
 	password = password + 13;

	if (platform_esp_write_calvin_config(name, strstr(name, "\"") - name, uri, strstr(uri, "\"") - uri) == CC_RESULT_SUCCESS) {
		if (platform_esp_write_wifi_config(ssid, strstr(ssid, "\"") - ssid, password, strstr(password, "\"") - password) == CC_RESULT_SUCCESS) {
			write(newsockfd, "HTTP/1.0 200 OK\r\n", strlen("HTTP/1.0 200 OK\r\n"));
			close(newsockfd);
			return CC_RESULT_SUCCESS;
		}
	}

	write(newsockfd, "HTTP/1.0 500 OK\r\n", strlen("HTTP/1.0 500 OK\r\n"));
	close(newsockfd);

	return CC_RESULT_FAIL;
}

void platform_init(void)
{
	spiffs_stat s;

	uart_set_baud(0, 115200);

	log("----------------------------------------");
	log("SDK version:%s", sdk_system_get_sdk_version());
	log("%20s: %u b", "Heap size", sdk_system_get_free_heap_size());
	log("%20s: 0x%08x", "Flash ID", sdk_spi_flash_get_id());
	log("%20s: %u Mbytes", "Flash size", sdk_flashchip.chip_size / 1024 / 1024);
	log("----------------------------------------");

	esp_spiffs_init();
	if (esp_spiffs_mount() != SPIFFS_OK) {
		SPIFFS_unmount(&fs);
		if (SPIFFS_format(&fs) == SPIFFS_OK)
			log("Filesystem formatted");
		else
			log_error("Failed to format filesystem");

		if (esp_spiffs_mount() != SPIFFS_OK)
			log_error("Failed to mount filesystem");
	}

	if (SPIFFS_stat(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, &s) != SPIFFS_OK) {
			if (platform_esp_start_ap_mode() == CC_RESULT_SUCCESS) {
				log("Restarting");
				sdk_system_restart();
				return;
			}
			log("Removing WiFi config");
			SPIFFS_remove(&fs, CALVIN_ESP_WIFI_CONFIG_FILE);
	}

	if (platform_esp_start_station_mode() != CC_RESULT_SUCCESS) {
		SPIFFS_remove(&fs, CALVIN_ESP_WIFI_CONFIG_FILE);
		log("Restarting");
		sdk_system_restart();
	}
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
	return calvinsys_ds18b20_create(node);
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

void platform_write_node_state(struct node_t* node, char *buffer, size_t size)
{
	spiffs_file fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	int res = 0;

	res = SPIFFS_write(&fs, fd, buffer, size);
	if (res != size)
		log_error("Failed to write runtime state, status '%d'", res);

	SPIFFS_close(&fs, fd);
}

result_t platform_read_node_state(struct node_t* node, char buffer[], size_t size)
{
	spiffs_file fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_RDONLY, 0);
	if (fd < 0) {
		log_error("Error opening file");
		return CC_RESULT_FAIL;
	}

	SPIFFS_read(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	return CC_RESULT_SUCCESS;
}

#ifdef CC_DEEPSLEEP_ENABLED
void platform_deepsleep(node_t *node)
{
	log("Enterring system deep sleep for '%d' seconds", CC_SLEEP_TIME);
	sdk_system_deep_sleep(CC_SLEEP_TIME * 1000 * 1000);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	log("----------- Should not come here --------------");
}
#endif

void calvin_task(void *pvParameters)
{
	int retries = 0;
	node_t *node = NULL;

	platform_init();

	while (1) {
		while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			log("Waiting for connection to AP");
			retries++;
			if (retries == 10) {
#ifdef CC_DEEPSLEEP_ENABLED
				platform_deepsleep(NULL);
#else
				sdk_system_restart();
#endif
			}
		}

		log("Connected to AP\n");

		if (api_runtime_init(&node, "esp8266", "ssdp", NULL) == CC_RESULT_SUCCESS)
			api_runtime_start(node);

		api_runtime_stop(node);
	}
}

void user_init(void)
{
	// TODO: Set proper stack size and prio
	xTaskCreate(&calvin_task, "calvin_task", 2048, NULL, 2, NULL);
}
