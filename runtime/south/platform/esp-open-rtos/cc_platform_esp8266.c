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
#include <unistd.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sys.h>
#include <string.h>
#include <stdarg.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <dhcpserver.h>
#include <spiffs.h>
#include <esp_spiffs.h>
#include <espressif/esp_system.h>
#include "cc_api.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/cc_node.h"
#include "runtime/south/transport/socket/cc_transport_socket.h"
#include "runtime/north/coder/cc_coder.h"
#include "calvinsys/cc_calvinsys_ds18b20.h"
#include "calvinsys/cc_calvinsys_yl69.h"
#include "calvinsys/cc_calvinsys_gpio.h"

#define CC_ESP_WIFI_CONFIG_FILE	"wifi.msgpack"
#define CC_ESP_BUFFER_SIZE			1024
#define CC_ESP_AP_SSID					"calvin-esp"
#define CC_ESP_AP_PSK						"calvin-esp"
#define CC_ESP_WIFI_STATUS_PIN	2
#define CC_ESP_RESET_PIN				4

// Capabilities

cc_calvinsys_temperature_state_t temp_state = {5};
cc_calvinsys_gpio_state_t light_state = {0, CC_GPIO_OUT};

cc_calvinsys_capability_t capabilities[] = {
	{cc_calvinsys_ds18b20_open, NULL, NULL, &temp_state, false, "io.temperature"},
	{cc_calvinsys_gpio_open, NULL, NULL, &light_state, false, "io.light"},
	{cc_calvinsys_yl69_open, NULL, NULL, NULL, false, "io.soil_moisture"}
};

// End of capabilities

#ifdef CC_PYTHON_ENABLED
// TODO: Workaround to solve link error with missing function
#include <math.h>
double __ieee754_remainder(double x, double y) {
	return x - y * floor(x/y);
}
#endif

static cc_result_t cc_platform_esp_write_calvin_config(char *attributes, uint32_t attributes_len, char *proxy_uris, uint32_t proxy_uris_len)
{
	int start = 0, read_pos = 0, nbr_uris = 1;
	char buffer[CC_ESP_BUFFER_SIZE], id[CC_UUID_BUFFER_SIZE];
	char *tmp = buffer;
	cc_result_t result = CC_FAIL;

	cc_gen_uuid(id, NULL);

	tmp = cc_coder_encode_map(tmp, 3);
	{
		tmp = cc_coder_encode_kv_str(tmp, "id", id, strnlen(id, CC_UUID_BUFFER_SIZE));
		tmp = cc_coder_encode_kv_str(tmp, "attributes", attributes, attributes_len);

		while (read_pos < proxy_uris_len) {
			if (proxy_uris[read_pos] == ' ' || read_pos == proxy_uris_len)
				nbr_uris++;
			read_pos++;
		}
		tmp = cc_coder_encode_kv_array(tmp, "proxy_uris", nbr_uris);
		read_pos = 0;
		while (read_pos <= proxy_uris_len) {
			if (proxy_uris[read_pos] == ' ' || read_pos == proxy_uris_len) {
				tmp = cc_coder_encode_str(tmp, proxy_uris + start, read_pos - start);
				start = read_pos + 1;
			}
			read_pos++;
		}
	}

	result = cc_platform_file_write(CC_CONFIG_FILE, buffer, tmp - buffer);
	if (result == CC_SUCCESS)
		cc_log("Config written to '%s'", CC_CONFIG_FILE);

	return result;
}

static cc_result_t cc_platform_esp_write_wifi_config(char *ssid, uint32_t ssid_len, char *password, uint32_t password_len)
{
	char buffer[CC_ESP_BUFFER_SIZE];
	char *tmp = buffer;
	cc_result_t result = CC_FAIL;

	tmp = cc_coder_encode_map(tmp, 2);
	{
		tmp = cc_coder_encode_kv_str(tmp, "ssid", ssid, ssid_len);
		tmp = cc_coder_encode_kv_str(tmp, "password", password, password_len);
	}

	result = cc_platform_file_write(CC_ESP_WIFI_CONFIG_FILE, buffer, tmp - buffer);
	if (result == CC_SUCCESS)
		cc_log("WiFi config written to '%s'", CC_ESP_WIFI_CONFIG_FILE);

	return result;
}

void cc_platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

cc_result_t cc_platform_create(struct cc_node_t *node)
{
	node->platform = NULL;
	return CC_SUCCESS;
}

cc_result_t cc_platform_add_capabilities(cc_calvinsys_t *calvinsys)
{
	return cc_calvinsys_add_capabilities(calvinsys, sizeof(capabilities) / sizeof(cc_calvinsys_capability_t), capabilities);
}

cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' bytes", (unsigned long)size);
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

cc_platform_evt_wait_status_t cc_platform_evt_wait(struct cc_node_t *node, uint32_t timeout_seconds)
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

	if (node->transport_client != NULL && (node->transport_client->state == CC_TRANSPORT_PENDING || node->transport_client->state == CC_TRANSPORT_ENABLED)) {
		FD_SET(((cc_transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((cc_transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, tv_ref);

		if (FD_ISSET(fd, &fds)) {
			if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS) {
				cc_log_error("Failed to read data from transport");
				node->transport_client->state = CC_TRANSPORT_DISCONNECTED;
				return CC_PLATFORM_EVT_WAIT_FAIL;
			}
			return CC_PLATFORM_EVT_WAIT_DATA_READ;
		}
	} else {
		if (timeout_seconds > 0)
			vTaskDelay((timeout_seconds * 1000) / portTICK_PERIOD_MS);
	}

	return CC_PLATFORM_EVT_WAIT_TIMEOUT;
}

cc_result_t cc_platform_stop(struct cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_stat_t cc_platform_file_stat(const char *path)
{
	int res = 0;
	spiffs_stat s;

	res = SPIFFS_stat(&fs, path, &s);
	if (res < 0)
		return CC_STAT_NO_EXIST;

	return CC_STAT_FILE;
}

cc_result_t cc_platform_file_write(const char *path, char *buffer, size_t size)
{
	cc_result_t result = CC_SUCCESS;
	spiffs_file fd;
	int res = 0;

	fd = SPIFFS_open(&fs, path, SPIFFS_CREAT | SPIFFS_RDWR, 0);
	if (fd < 0) {
		cc_log_error("Failed to open '%s'", path);
		return CC_FAIL;
	}

	res = SPIFFS_write(&fs, fd, buffer, size);
	if (res != size) {
		cc_log_error("Failed to write '%s' status '%d'", path, res);
		result = CC_FAIL;
	}
	SPIFFS_close(&fs, fd);

	return result;
}

cc_result_t cc_platform_file_read(const char *path, char **buffer, size_t *size)
{
	size_t read = 0;
	spiffs_file fd;
	spiffs_stat s;

 	fd = SPIFFS_open(&fs, path, SPIFFS_RDONLY, 0);
	if (fd < 0) {
		cc_log_error("Failed to open '%s'", path);
		return CC_FAIL;
	}

	SPIFFS_stat(&fs, path, &s);
	if (cc_platform_mem_alloc((void **)buffer, s.size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	read = SPIFFS_read(&fs, fd, *buffer, s.size);
	SPIFFS_close(&fs, fd);

	if (read != s.size) {
		cc_log_error("Failed to read '%s'", path);
		cc_platform_mem_free(*buffer);
		return CC_FAIL;
	}
	*size = s.size;

	return CC_SUCCESS;
}

#ifdef CC_DEEPSLEEP_ENABLED
void cc_platform_deepsleep(uint32_t time)
{
	sdk_system_deep_sleep(time * 1000 * 1000);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
}
#endif

void cc_platform_init(void)
{
}

uint32_t cc_platform_get_time()
{
	return sdk_system_get_time() / 1000000;
}

static cc_result_t cc_platform_esp_get_config(void)
{
	int sockfd = 0, newsockfd = 0, clilen = 0, len = 0;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[CC_ESP_BUFFER_SIZE];
	char *attributes = NULL, *uri = NULL, *ssid = NULL, *password = NULL;
	size_t len_attributes = 0, len_uri = 0, len_ssid = 0, len_password = 0;
	struct ip_info ap_ip;
	ip_addr_t first_client_ip;
	struct sdk_softap_config ap_config = {
		.ssid = CC_ESP_AP_SSID,
		.ssid_hidden = 0,
		.channel = 3,
		.ssid_len = strlen(CC_ESP_AP_SSID),
		.authmode = AUTH_WPA_WPA2_PSK,
		.password = CC_ESP_AP_PSK,
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
		return CC_FAIL;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(80);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		dhcpserver_stop();
		cc_log_error("Failed to bind socket");
		return CC_FAIL;
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

		bzero(buffer, CC_ESP_BUFFER_SIZE);
		len = read(newsockfd, buffer, CC_ESP_BUFFER_SIZE);
		if (len < 0) {
			cc_log_error("Failed to read data");
			close(newsockfd);
			continue;
		}

		cc_log("Data received, %d bytes", len);

		if (cc_get_json_dict_value(buffer, len, (char *)"attributes", 10, &attributes, &len_attributes) != CC_SUCCESS) {
			cc_log_error("No attribute 'attributes'");
			close(newsockfd);
			continue;
		}

		if (len_attributes > CC_MAX_ATTRIBUTES_LEN) {
			cc_log_error("Attributes to big");
			close(newsockfd);
			continue;
		}

		if (cc_get_json_string_value(buffer, len, (char *)"proxy_uris", 10, &uri, &len_uri) != CC_SUCCESS) {
			cc_log_error("No attribute 'proxy_uris'");
			close(newsockfd);
			continue;
		}

		if (cc_get_json_string_value(buffer, len, (char *)"ssid", 4, &ssid, &len_ssid) != CC_SUCCESS) {
			cc_log_error("No attribute 'ssid'");
			close(newsockfd);
			continue;
		}

		if (cc_get_json_string_value(buffer, len, (char *)"password", 8, &password, &len_password) != CC_SUCCESS) {
			cc_log_error("No attribute 'password'");
			close(newsockfd);
			continue;
		}

		if (cc_platform_esp_write_calvin_config(attributes, len_attributes, uri, len_uri) == CC_SUCCESS) {
			if (cc_platform_esp_write_wifi_config(ssid, len_ssid, password, len_password) == CC_SUCCESS) {
				write(newsockfd, "HTTP/1.0 200 OK\r\n", 17);
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				close(newsockfd);
				dhcpserver_stop();
				cc_log("Config data written");
				return CC_SUCCESS;
			} else
				cc_log_error("Failed to write WiFi config");
		} else
			cc_log_error("Failed to write runtime config");

		write(newsockfd, "HTTP/1.0 500 Internal Server Error\r\n", 36);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		close(newsockfd);
	}
}

static cc_result_t cc_platform_esp_start_station_mode(void)
{
	char *buffer = NULL, *ssid = NULL, *password = NULL;
	uint32_t ssid_len = 0, password_len = 0;
	struct sdk_station_config config;
	uint8_t status = 0, retries = 0;
	size_t size;

	if (cc_platform_file_read(CC_ESP_WIFI_CONFIG_FILE, &buffer, &size) != CC_SUCCESS) {
		cc_log_error("Failed to read %s", CC_ESP_WIFI_CONFIG_FILE);
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(buffer, "ssid", &ssid, &ssid_len) != CC_SUCCESS) {
		cc_log_error("Failed to read 'ssid' from config");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	if (cc_coder_decode_string_from_map(buffer, "password", &password, &password_len) != CC_SUCCESS) {
		cc_log_error("Failed to read 'password' from config");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	cc_platform_mem_free(buffer);

	strncpy((char *)config.ssid, ssid, ssid_len);
	config.ssid[ssid_len] = '\0';
	strncpy((char *)config.password, password, password_len);
	config.password[password_len] = '\0';

	cc_log("Starting in station mode with AP '%s'", config.ssid);

	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	sdk_wifi_station_connect();

	retries = 10;
	while (retries--) {
		cc_log("Waiting for connection to AP (retries %d)", retries);
		status = sdk_wifi_station_get_connect_status();
		if (status == STATION_GOT_IP) {
			cc_log("Connected to AP");
			return CC_SUCCESS;
		} else if (status == STATION_WRONG_PASSWORD) {
			cc_log("Wrong password");
			return CC_FAIL;
		} else if (status == STATION_NO_AP_FOUND)
			cc_log("AP not found");
		else if (status == STATION_CONNECT_FAIL)
			cc_log("Connection failed");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	return CC_FAIL;
}

void calvin_task(void *pvParameters)
{
	cc_node_t *node = NULL;
	spiffs_stat s;
	bool startAP = false;
	cc_result_t result = CC_SUCCESS;
	uint32_t total, used;

	uart_set_baud(0, 115200);

	cc_log("----------------------------------------");
	cc_log("SDK version:%s", sdk_system_get_sdk_version());
	cc_log("%20s: %u b", "Heap size", sdk_system_get_free_heap_size());
	cc_log("%20s: 0x%08x", "Flash ID", sdk_spi_flash_get_id());
	cc_log("%20s: %u Mbytes", "Flash size", sdk_flashchip.chip_size / 1024 / 1024);
	cc_log("----------------------------------------");

	// Set led to indicate wifi status
	sdk_wifi_status_led_install(CC_ESP_WIFI_STATUS_PIN, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	// reset config pin
	gpio_enable(CC_ESP_RESET_PIN, GPIO_INPUT);

	esp_spiffs_init();

	if (esp_spiffs_mount() != SPIFFS_OK) {
		SPIFFS_unmount(&fs);
		if (SPIFFS_format(&fs) == SPIFFS_OK)
			cc_log("Filesystem formatted");
		else {
			cc_log_error("Failed to format filesystem %i", SPIFFS_errno(&fs));
			result = CC_FAIL;
		}

		if (result == CC_SUCCESS && esp_spiffs_mount() != SPIFFS_OK) {
			cc_log_error("Failed to mount filesystem");
			result = CC_FAIL;
		}
	}

	if (result == CC_SUCCESS) {
		if (gpio_read(CC_ESP_RESET_PIN) == 1) {
			cc_log("Forcing AP");
			startAP = true;
		}

		if (SPIFFS_stat(&fs, CC_ESP_WIFI_CONFIG_FILE, &s) != SPIFFS_OK) {
			cc_log("No WiFi config found");
			startAP = true;
		}

		if (startAP)
			result = cc_platform_esp_get_config();
	}

	if (result != CC_SUCCESS) {
		cc_log("Removing config files");
		SPIFFS_remove(&fs, CC_ESP_WIFI_CONFIG_FILE);
		SPIFFS_remove(&fs, CC_CONFIG_FILE);
	} else {
		SPIFFS_info(&fs, &total, &used);
		cc_log("SPIFFS size: %d bytes, used: %d bytes", total, used);
		result = cc_platform_esp_start_station_mode();
		if (result == CC_SUCCESS) {
			if (cc_api_runtime_init(&node, NULL, NULL, NULL) == CC_SUCCESS)
				cc_api_runtime_start(node);
		}
	}

	cc_log("System restart");
	sdk_system_restart();
}

void user_init(void)
{
	// TODO: Set proper stack size and prio
	xTaskCreate(&calvin_task, "calvin_task", 2048, NULL, 2, NULL);
}
