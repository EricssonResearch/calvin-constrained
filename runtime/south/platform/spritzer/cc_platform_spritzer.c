#include <ril/lte_if.h>
#include <ril/lte_if_system.h>
#include <errno.h>
#include "net/lwip/net_lwip/lwip/sockets.h"
#include <nuttx/board.h>
#include <arch/cxd56xx/pm.h>
#include <arch/board/board.h>
#include <nuttx/config.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include "../../../../cc_api.h"
#include "../cc_platform.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../north/cc_transport.h"
#include "../../../north/cc_node.h"
#include "../../../north/cc_common.h"
#include "../../../north/coder/cc_coder.h"
#include "../../../../calvinsys/cc_calvinsys.h"
#include "calvinsys/cc_calvinsys_temp_sensor.h"

static LTECommand_System response_sys;
static int com_status = 0;
static int com_id = 0;
static LTECommand command;
static LTECommand_System command_sys;

static int cc_platform_spritzer_wait_recv(void)
{
  int count;

  /* Wait receive */
  for (count=0; count<500; count++)
  {
    if (com_status != 1)
    {
      return com_status;
    }
    usleep(10000);
  }

  /* Timeout */
  return 0;
}

static int cc_platform_spritzer_send_recv_sys(LTECommand_System* cmd)
{
  /* Status init */
  com_status = 1;

  /* Send and return code check */
  cc_log_debug("LT_SendSystem : %04X(%d)\n", cmd->header.code, cmd->header.id);
  LT_SendSystem(cmd);
  if (cmd->header.result < 0)
  {
    cc_log_debug("LT_SendSystem error : %d\n", cmd->header.result);
    return cmd->header.result;
  }

  /* Wait receive */
  return cc_platform_spritzer_wait_recv();
}

static void cc_platform_spritzer_app_callback_sys(void* out)
{
  LTECommand_System *data = (LTECommand_System *)out;

  com_status = data->header.result;
  cc_log_debug("Response[%04X][%d][%d][%04X]\n", data->header.code, data->header.id, data->header.result, data->header.type);

  switch (data->header.type)
  {
    case LTE_STRUCT_TYPE_MESSAGE:
      cc_log_debug("data:%s\n", data->message.text.data);
      break;
    case LTE_STRUCT_TYPE_ERROR:
      cc_log_debug("[ERROR]:%s(code:%d)\n", data->message.error.group, data->message.error.code);
      break;
    case LTE_STRUCT_TYPE_PIN_INFO:
      cc_log_debug("status:%s\n", data->message.pin.status);
      break;
    case LTE_STRUCT_TYPE_STATE:
      cc_log_debug("state:%d\n", data->message.state.state);
      break;
    case LTE_STRUCT_TYPE_POWER_STATE:
      cc_log_debug("power state:%d\n", data->message.pwrstate.state);
      break;
    case LTE_STRUCT_TYPE_WAKELOCK_STATE:
      cc_log_debug("wakelock state:%d\n", data->message.lockstate.state);
      break;
    case LTE_STRUCT_TYPE_EVENT:
      cc_log_debug("event:%s\nvalue:%d\nparam:%s\n", data->message.event.event, data->message.event.value, data->message.event.param);
      break;
    case LTE_STRUCT_TYPE_GPSNOTIFY:
      cc_log_debug("handle:%d\nnotify:%d\nlocation:%d\nid:%s\nname:%s\nplane:%d\n", data->message.gpsnotify.handle, data->message.gpsnotify.notify, data->message.gpsnotify.location, data->message.gpsnotify.id, data->message.gpsnotify.name, data->message.gpsnotify.plane);
      break;
    case LTE_STRUCT_TYPE_GPSAUTO:
      cc_log_debug("mode:%d\n", data->message.gpsauto.mode);
      break;
    default:
      if (data->header.result == 0)
        cc_log_debug("success\n");
      else
        cc_log_debug("fail\n");
      break;
  }
}

static void cc_platform_spritzer_lte_init(void)
{
  while (1)
  {
    /* Delay for link detection */
    sleep(3);

    /* Initialize task */
    command.debug_level = LTE_DEBUG_INFO;
    LT_Start(&command);
    if (command.result == 0)
    {
      break;
    }
    cc_log("LT_Start() error : %d\n", command.result);
  }

  cc_log("LT_Start() done\n");

  /* Set callback */
  LT_SetCallbackSystem(&cc_platform_spritzer_app_callback_sys, &response_sys);
}

void cc_platform_print(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  va_end(args);
}

cc_result_t cc_platform_stop(cc_node_t *node)
{
  return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
  return CC_SUCCESS;
}

cc_result_t cc_platform_create_calvinsys(cc_calvinsys_t **calvinsys)
{
  if (cc_calvinsys_temp_sensor_create(calvinsys, "io.temperature") != CC_SUCCESS) {
    cc_log_error("Failed to create 'io.temperature'");
    return CC_FAIL;
  }

  return CC_SUCCESS;
}

void cc_platform_init(void)
{
  srand(time(NULL));

  board_power_control(POWER_LTE, true);

  cc_platform_spritzer_lte_init();

  /* Send system connect command */
  cc_log("LT_Send() : LTE_COMMAND_SYSTEM_CONNECT\n");
  command_sys.header.code = LTE_COMMAND_SYSTEM_CONNECT;
  command_sys.header.id = com_id++;
  command_sys.header.type = LTE_STRUCT_TYPE_CONFIG;
  command_sys.message.config.IMSregist = 0;
  cc_platform_spritzer_send_recv_sys(&command_sys);
}

cc_result_t cc_platform_create(cc_node_t *node)
{
  return CC_SUCCESS;
}

bool cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
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

    NT_Select(fd + 1, &fds, NULL, NULL, tv_ref);

    if (FD_ISSET(fd, &fds)) {
      if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS) {
        cc_log_error("Failed to read data from transport");
        node->transport_client->state = CC_TRANSPORT_DISCONNECTED;
      }
      return true;
    }
  } else
    NT_Select(0, NULL, NULL, NULL, tv_ref);

  return false;
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

uint32_t cc_platform_get_seconds(cc_node_t *node)
{
  struct timeval value;

  gettimeofday(&value, NULL);

  return value.tv_sec;
}

#ifdef CC_DEEPSLEEP_ENABLED
void cc_platform_deepsleep(cc_node_t *node, uint32_t time)
{
  // TODO: Set RTC alarm and enter PM_SLEEP_DEEP
  // and calvin should be started at OS boot
  cc_log("TODO: Implement sleep");
}
#endif

#ifdef CC_STORAGE_ENABLED
void cc_platform_write_node_state(cc_node_t *node, char *buffer, size_t size)
{
  FILE *fp = NULL;
  int len = 0;

  fp = fopen(CC_CONFIG_FILE, "w+");
  if (fp != NULL) {
    len = fwrite(buffer, 1, size, fp);
    if (len != size)
      cc_log_error("Failed to write node config");
    else
      cc_log_debug("Wrote runtime state '%d' bytes", len);
    fclose(fp);
  } else
    cc_log("Failed to open %s for writing", CC_CONFIG_FILE);
}

cc_result_t cc_platform_read_node_state(cc_node_t *node, char buffer[], size_t size)
{
  FILE *fp = NULL;

  fp = fopen(CC_CONFIG_FILE, "r+");
  if (fp != NULL) {
    fread(buffer, 1, size, fp);
    fclose(fp);
    return CC_SUCCESS;
  }

  return CC_FAIL;
}
#endif

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int calvin_main(int argc, char *argv[])
#endif
{
  char *attr = NULL, *proxy_uris = NULL;
  cc_node_t *node = NULL;

  // workaround due to get_opt/get_opt_long not working
  if (argc == 1) {
    printf("Started without arguments\n");
  } else if (argc == 3) {
    printf("Started with arguments\n");
    proxy_uris = argv[1];
    attr = argv[2];
  } else {
    printf("Usage: calvin_c proxy_uris attributes\n");
    return EXIT_FAILURE;
  }

  if (cc_api_runtime_init(&node, attr, proxy_uris, "./") != CC_SUCCESS)
    return EXIT_FAILURE;

  if (cc_api_runtime_start(node) != CC_SUCCESS)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
