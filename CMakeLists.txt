cmake_minimum_required(VERSION 3.22)
project(nats_logger C)

set(CMAKE_C_STANDARD 17)

add_executable(nats_logger main.c libs/jsmn-stream/jsmn_stream.c)

target_link_libraries(nats_logger nats)

install(TARGETS nats_logger DESTINATION bin)
