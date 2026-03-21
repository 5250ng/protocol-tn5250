cmake_minimum_required(VERSION 3.16)
project(tn5250 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

option(TN5250_BUILD_TESTS "Build protocol-tn5250 tests" ON)

find_package(OpenSSL REQUIRED)

# Collect sources
set(TN5250_SOURCES
    # utils
    src/utils/endianness/big_endian.cpp
    src/utils/endianness/little_endian.cpp
    src/utils/binary/binary.cpp
    src/utils/hex/hex.cpp
    # codepage
    src/codepage/codepage.cpp
    # telnet
    src/telnet/commands.cpp
    src/telnet/options.cpp
    src/telnet/client.cpp
    # devices
    src/devices/devices.cpp
    # client
    src/client/ibmrseed.cpp
    src/client/decoder.cpp
    src/client/protocol_handler.cpp
    # message
    src/message/message.cpp
    src/message/operation_code.cpp
    src/message/header/header.cpp
    src/message/header/record_type.cpp
    src/message/command/command.cpp
    src/message/command/command_code.cpp
    src/message/command/commands/command_cs_clear_screen.cpp
    src/message/command/commands/command_wtd_write_to_display.cpp
    src/message/command/commands/command_rmf_read_mdt_fields.cpp
    src/message/command/order/order.cpp
    src/message/command/order/order_code.cpp
    src/message/command/order/orders/order_ic_insert_cursor.cpp
    src/message/command/order/orders/order_ra_repeat_to_address.cpp
    src/message/command/order/orders/order_sba_set_buffer_address.cpp
    src/message/command/order/orders/order_sf_start_field.cpp
    src/message/command/order/orders/order_soh_start_of_header.cpp
)

add_library(tn5250 STATIC ${TN5250_SOURCES})
add_library(tn5250::tn5250 ALIAS tn5250)

target_include_directories(tn5250
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(tn5250 PUBLIC OpenSSL::Crypto)

if(TN5250_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
