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
#include "calvinsys/cc_calvinsys_yl69.h"
#include "calvinsys/cc_calvinsys_gpio.h"

#define CALVIN_ESP_RUNTIME_STATE_FILE	"cc_state.conf"
#define CALVIN_ESP_WIFI_CONFIG_FILE	"cc_wifi.conf"
#define CALVIN_ESP_BUFFER_SIZE		1024
#define CALVIN_ESP_AP_SSID		"calvin-esp"
#define CALVIN_ESP_AP_PSK		"calvin-esp"


static result_t platform_esp_write_calvin_config(char *attributes, uint32_t attributes_len, char *proxy_uris, uint32_t proxy_uris_len)
{
	spiffs_file fd;
	int res = 0, start = 0, read_pos = 0, nbr_uris = 1;
	char buffer[CALVIN_ESP_BUFFER_SIZE], id[UUID_BUFFER_SIZE];
	char *tmp = buffer;
	size_t size = 0;

	fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	if (fd < 0) {
		cc_log_error("Failed to open config file");
		return CC_RESULT_FAIL;
	}

	gen_uuid(id, NULL);

	tmp = mp_encode_map(tmp, 3);
	{
		tmp = encode_str(&tmp, "id", id, strlen(id));
		tmp = encode_str(&tmp, "attributes", attributes, attributes_len);

		while (read_pos < proxy_uris_len) {
			if (proxy_uris[read_pos] == ' ' || read_pos == proxy_uris_len)
				nbr_uris++;
			read_pos++;
		}
		tmp = encode_array(&tmp, "proxy_uris", nbr_uris);
		read_pos = 0;
		while (read_pos <= proxy_uris_len) {
			if (proxy_uris[read_pos] == ' ' || read_pos == proxy_uris_len) {
				tmp = mp_encode_str(tmp, proxy_uris + start, read_pos - start);
				start = read_pos + 1;
			}
			read_pos++;
		}
	}

	size = tmp - buffer;
	res = SPIFFS_write(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	if (res != size) {
		cc_log_error("Failed to write runtime config, status '%d'", res);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

static result_t platform_esp_write_wifi_config(char *ssid, uint32_t ssid_len, char *password, uint32_t password_len)
{
	spiffs_file fd;
	int res = 0;
	char buffer[CALVIN_ESP_BUFFER_SIZE];
	char *tmp = buffer;
	size_t size = 0;

	fd = SPIFFS_open(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	if (fd < 0) {
		cc_log_error("Failed to open config file");
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
		cc_log_error("Failed to write runtime wifi config, status '%d'", res);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

static int get_dict_size(char *data, size_t size)
{
	int read_pos = 0, braces = 0;

	while (read_pos < size) {
		if (data[read_pos] == '{')
			braces++;
		if (data[read_pos] == '}')
			braces--;

		read_pos++;

		if (braces == 0)
			break;
	}

	return read_pos;
}

void platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

result_t platform_create(struct node_t *node)
{
	node->platform = NULL;
	return CC_RESULT_SUCCESS;
}

result_t platform_create_calvinsys(calvinsys_t **calvinsys)
{
	calvinsys_handler_t *handler = NULL;
	calvinsys_gpio_state_t *state_light = NULL;

	if (calvinsys_ds18b20_create(calvinsys, "io.temperature") != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to create 'io.temperature'");
		return CC_RESULT_FAIL;
	}

	if (calvinsys_yl69_create(calvinsys, "io.soilmoisture") != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to create 'io.soil_moisture'");
		return CC_RESULT_FAIL;
	}

	handler = calvinsys_gpio_create_handler(calvinsys);
	if (handler == NULL) {
		cc_log_error("Failed to create gpio handler");
		return CC_RESULT_FAIL;
	}

	if (platform_mem_alloc((void **)&state_light, sizeof(calvinsys_gpio_state_t)) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_RESULT_FAIL;
	}

	state_light->pin = 2;
	state_light->direction = CC_GPIO_OUT;

	if (calvinsys_register_capability(*calvinsys, "io.light", handler, state_light) != CC_RESULT_SUCCESS)
		return CC_RESULT_FAIL;

	return CC_RESULT_SUCCESS;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' memory", (unsigned long)size);
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
				cc_log_error("Failed to read data from transport");
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

result_t platform_stop(struct node_t *node)
{
	return CC_RESULT_SUCCESS;
}

result_t platform_node_started(struct node_t *node)
{
	return CC_RESULT_SUCCESS;
}

void platform_write_node_state(struct node_t *node, char *buffer, size_t size)
{
	spiffs_file fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	int res = 0;

	res = SPIFFS_write(&fs, fd, buffer, size);
	if (res != size)
		cc_log_error("Failed to write runtime state, status '%d'", res);

	SPIFFS_close(&fs, fd);
}

result_t platform_read_node_state(struct node_t *node, char buffer[], size_t size)
{
	spiffs_file fd = SPIFFS_open(&fs, CALVIN_ESP_RUNTIME_STATE_FILE, SPIFFS_RDONLY, 0);

	if (fd < 0) {
		cc_log_error("Error opening file");
		return CC_RESULT_FAIL;
	}

	SPIFFS_read(&fs, fd, buffer, size);
	SPIFFS_close(&fs, fd);

	return CC_RESULT_SUCCESS;
}

#ifdef CC_DEEPSLEEP_ENABLED
void platform_deepsleep(node_t *node)
{
	cc_log("Enterring system deep sleep for '%d' seconds", CC_SLEEP_TIME);
	sdk_system_deep_sleep(CC_SLEEP_TIME * 1000 * 1000);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	cc_log("----------- Should not come here --------------");
}
#endif

void platform_init(void)
{
}

static result_t platform_esp_get_config(void)
{
	int sockfd = 0, newsockfd = 0, clilen = 0, len = 0;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[CALVIN_ESP_BUFFER_SIZE];
	char *attributes = NULL, *uri = NULL, *ssid = NULL, *password = NULL;
	size_t len_attributes = 0, len_uri = 0, len_ssid = 0, len_password = 0;
	struct ip_info ap_ip;
	ip_addr_t first_client_ip;
	struct sdk_softap_config ap_config = {
		.ssid = CALVIN_ESP_AP_SSID,
		.ssid_hidden = 0,
		.channel = 3,
		.ssid_len = strlen(CALVIN_ESP_AP_SSID),
		.authmode = AUTH_WPA_WPA2_PSK,
		.password = CALVIN_ESP_AP_PSK,
		.max_connection = 1,
		.beacon_interval = 100,
	};

	sdk_wifi_set_opmode(SOFTAP_MODE);

	IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
	IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
	IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);

	sdk_wifi_set_ip_info(1, &ap_ip);
	sdk_wifi_softap_set_config(&ap_config);

	IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
	dhcpserver_start(&first_client_ip, 4);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		cc_log_error("Failed to open socket");
		return CC_RESULT_FAIL;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(80);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		cc_log_error("Failed to bind socket");
		return CC_RESULT_FAIL;
	}

	listen(sockfd, 1);
	clilen = sizeof(cli_addr);

	while (1) {
		cc_log("Waiting for config on '172.16.0.1:80'");

		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
		if (newsockfd < 0) {
			cc_log_error("Failed to accept client");
			continue;
		}

		bzero(buffer, CALVIN_ESP_BUFFER_SIZE);
		len = read(newsockfd, buffer, CALVIN_ESP_BUFFER_SIZE);
		if (len < 0) {
			cc_log_error("Failed to read data");
			close(newsockfd);
			continue;
		}

		attributes = strstr(buffer, "\"attributes\": ");
		if (attributes == NULL) {
			cc_log_error("Failed to parse 'attributes' from '%s'", buffer);
			close(newsockfd);
			continue;
		}
		attributes = attributes + 14;
		len_attributes = get_dict_size(attributes, len - (buffer - attributes));

		uri = strstr(buffer, "\"proxy_uris\": \"");
		if (uri == NULL) {
			cc_log_error("Failed to parse 'proxy_uris' from '%s'", buffer);
			close(newsockfd);
			continue;
		}
		uri = uri + 15;
		len_uri = strstr(uri, "\"") - uri;

		ssid = strstr(buffer, "\"ssid\": \"");
		if (ssid == NULL) {
			cc_log_error("Failed to parse 'ssid' from '%s'", buffer);
			close(newsockfd);
			continue;
		}
		ssid = ssid + 9;
		len_ssid = strstr(ssid, "\"") - ssid;

		password = strstr(buffer, "\"password\": \"");
		if (password == NULL) {
			cc_log_error("Failed to parse 'password' from configuration data");
			close(newsockfd);
			continue;
		}
		password = password + 13;
		len_password = strstr(password, "\"") - password;

		// TODO: Better attribute parsing needed
		if (platform_esp_write_calvin_config(attributes, len_attributes, uri, len_uri) == CC_RESULT_SUCCESS) {
			if (platform_esp_write_wifi_config(ssid, len_ssid, password, len_password) == CC_RESULT_SUCCESS) {
				cc_log("Config data written");
				write(newsockfd, "HTTP/1.0 200 OK\r\n", strlen("HTTP/1.0 200 OK\r\n"));
				close(newsockfd);
				return CC_RESULT_SUCCESS;
			} else
				cc_log_error("Failed to write WiFi config");
		} else
			cc_log_error("Failed to write runtime config");

		write(newsockfd, "HTTP/1.0 500 OK\r\n", strlen("HTTP/1.0 500 OK\r\n"));
		close(newsockfd);
	}
}

static result_t platform_esp_start_station_mode(void)
{
	spiffs_file fd;
	char buffer[CALVIN_ESP_BUFFER_SIZE], *ssid = NULL, *password = NULL;
	uint32_t ssid_len = 0, password_len = 0;
	struct sdk_station_config config;
	uint8_t status = 0, retries = 0;

	fd = SPIFFS_open(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, SPIFFS_RDONLY, 0);
	if (fd < 0) {
		cc_log_error("Failed to open config file");
		return CC_RESULT_FAIL;
	}

	if (SPIFFS_read(&fs, fd, buffer, CALVIN_ESP_BUFFER_SIZE) < 0) {
		cc_log_error("Failed to read file");
		SPIFFS_close(&fs, fd);
		return CC_RESULT_FAIL;
	}

	SPIFFS_close(&fs, fd);

	if (decode_string_from_map(buffer, "ssid", &ssid, &ssid_len) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to read 'ssid' from config");
		return CC_RESULT_FAIL;
	}

	if (decode_string_from_map(buffer, "password", &password, &password_len) != CC_RESULT_SUCCESS) {
		cc_log_error("Failed to read 'password' from config");
		return CC_RESULT_FAIL;
	}

	strncpy((char *)config.ssid, ssid, ssid_len);
	config.ssid[ssid_len] = '\0';
	strncpy((char *)config.password, password, password_len);
	config.password[password_len] = '\0';

	cc_log("Starting in station mode with AP '%s'", config.ssid);

	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	sdk_wifi_station_connect();

	retries = 20;
	while (retries--) {
		cc_log("Waiting for connection to AP (retries %d)", retries);
		status = sdk_wifi_station_get_connect_status();
		if (status == STATION_GOT_IP) {
			cc_log("Connected to AP");
			return CC_RESULT_SUCCESS;
		} else if (status == STATION_WRONG_PASSWORD) {
			cc_log("Wrong password");
			return CC_RESULT_FAIL;
		} else if (status == STATION_NO_AP_FOUND) {
			cc_log("AP not found");
			return CC_RESULT_FAIL;
		} else if (status == STATION_CONNECT_FAIL) {
			cc_log("Connection failed");
			return CC_RESULT_FAIL;
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	return CC_RESULT_FAIL;
}

void calvin_task(void *pvParameters)
{
	node_t *node = NULL;
	spiffs_stat s;

	uart_set_baud(0, 115200);

	cc_log("----------------------------------------");
	cc_log("SDK version:%s", sdk_system_get_sdk_version());
	cc_log("%20s: %u b", "Heap size", sdk_system_get_free_heap_size());
	cc_log("%20s: 0x%08x", "Flash ID", sdk_spi_flash_get_id());
	cc_log("%20s: %u Mbytes", "Flash size", sdk_flashchip.chip_size / 1024 / 1024);
	cc_log("----------------------------------------");

	// Set led to indicate wifi status.
#ifdef CC_USE_WIFI_STATUS_LED
	sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
#endif

	esp_spiffs_init();
	cc_log("Mounting filesystem");
	if (esp_spiffs_mount() != SPIFFS_OK) {
		SPIFFS_unmount(&fs);
		cc_log("Formatting filesystem");
		if (SPIFFS_format(&fs) == SPIFFS_OK)
			cc_log("Filesystem formatted");
		else {
			cc_log_error("Failed to format filesystem, restarting");
			sdk_system_restart();
			return;
		}

		if (esp_spiffs_mount() != SPIFFS_OK) {
			cc_log_error("Failed to mount filesystem");
			sdk_system_restart();
			return;
		}
	}

	if (SPIFFS_stat(&fs, CALVIN_ESP_WIFI_CONFIG_FILE, &s) != SPIFFS_OK) {
		cc_log("No WiFi config found");
		if (platform_esp_get_config() != CC_RESULT_SUCCESS) {
			cc_log("Failed to get config, restarting");
			SPIFFS_remove(&fs, CALVIN_ESP_WIFI_CONFIG_FILE);
			SPIFFS_remove(&fs, CALVIN_ESP_RUNTIME_STATE_FILE);
			sdk_system_restart();
			return;
		}
	}

	if (platform_esp_start_station_mode() == CC_RESULT_SUCCESS) {
		if (api_runtime_init(&node, NULL, NULL, NULL) == CC_RESULT_SUCCESS)
			api_runtime_start(node);
	} else {
		SPIFFS_remove(&fs, CALVIN_ESP_WIFI_CONFIG_FILE);
		SPIFFS_remove(&fs, CALVIN_ESP_RUNTIME_STATE_FILE);
	}

	cc_log("Restarting");
	sdk_system_restart();
}

void user_init(void)
{
	// TODO: Set proper stack size and prio
	xTaskCreate(&calvin_task, "calvin_task", 2048, NULL, 2, NULL);
}
