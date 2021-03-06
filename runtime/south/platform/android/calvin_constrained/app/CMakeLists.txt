cmake_minimum_required(VERSION 3.4.1)

cmake_policy(SET CMP0003 OLD)
cmake_policy(SET CMP0026 OLD)

include_directories("../../../../../..")

add_library(
             calvin_constrained

             SHARED

             ../../../../../../main.c
             ../../../../../../cc_api.c
             ../../../../../../runtime/north/cc_common.c
             ../../../../../../runtime/north/scheduler/np_scheduler/cc_scheduler.c
             ../../../../../../runtime/north/cc_node.c
             ../../../../../../runtime/north/cc_proto.c
             ../../../../../../runtime/north/cc_transport.c
             ../../../../../../runtime/south/transport/fcm/cc_transport_fcm.c
             ../../../../../../runtime/south/transport/socket/cc_transport_socket.c
             ../../../../../../runtime/north/cc_tunnel.c
             ../../../../../../runtime/north/cc_link.c
             ../../../../../../runtime/north/cc_actor.c
             ../../../../../../runtime/north/cc_port.c
             ../../../../../../runtime/north/cc_fifo.c
             ../../../../../../runtime/north/cc_token.c
             ../../../../../../runtime/north/coder/cc_coder_msgpuck.c
             ../../../../../../msgpuck/msgpuck.c
             ../../../../../../runtime/north/cc_actor_store.c
             ../../../../../../actors/sensor/cc_actor_accelerometer.c
             ../../../../../../actors/sensor/cc_actor_gyroscope.c
             ../../../../../../runtime/south/platform/android/cc_platform_android.c
             ../../../../../../runtime/south/platform/android/cc_jni_api.c
             ../../../../../../calvinsys/cc_calvinsys.c
             ../../../../../../calvinsys/common/cc_calvinsys_timer.c
             ../../../../../../calvinsys/common/cc_calvinsys_attribute.c
             ../../../../../../runtime/south/platform/android/calvinsys/cc_accelerometer.c
             ../../../../../../runtime/south/platform/android/calvinsys/cc_gyroscope.c
             ../../../../../../jsmn/jsmn.c
  )

find_library(
            log-lib
            log
)

target_link_libraries(
             calvin_constrained
             #micropython
             ${log-lib}
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -landroid")
