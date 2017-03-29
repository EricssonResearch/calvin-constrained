cmake_minimum_required(VERSION 3.4.1)

cmake_policy(SET CMP0003 OLD)
cmake_policy(SET CMP0026 OLD)

add_library(
             calvin_constrained

             SHARED

             ../../../../main.c
             ../../../../platform/android/platform_android.c
             ../../../../platform/android/jni_api.c
             ../../../../platform/android/calvinsys_android.c
             ../../../../main.c
             ../../../../api.c
             ../../../../common.c
             ../../../../node.c
             ../../../../proto.c
             ../../../../transport.c
             ../../../../transport/fcm/transport_fcm.c
             ../../../../transport/socket/transport_socket.c
             ../../../../tunnel.c
             ../../../../link.c
             ../../../../actor.c
             ../../../../port.c
             ../../../../fifo.c
             ../../../../token.c
             ../../../../msgpack_helper.c
             ../../../../msgpuck/msgpuck.c
             ../../../../actors/actor_identity.c
             ../../../../actors/actor_gpioreader.c
             ../../../../actors/actor_gpiowriter.c
             ../../../../actors/actor_temperature.c
             ../../../../actors/actor_mpy.c
             # ../../../../libmpy/calvin_mpy_port.c
             # ../../../../libmpy/calvin_mpy_gpiohandler.c
             # ../../../../libmpy/calvin_mpy_environmental.c
)

find_library(
            log-lib
            log
)

target_link_libraries(
             calvin_constrained
             micropython
             ${log-lib}
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -landroid")
