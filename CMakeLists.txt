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
    tn5250/utils/endianness/big_endian.cpp
    tn5250/utils/endianness/little_endian.cpp
    tn5250/utils/binary/binary.cpp
    tn5250/utils/hex/hex.cpp
    # codepage
    tn5250/codepage/codepage.cpp
    # telnet
    tn5250/telnet/commands.cpp
    tn5250/telnet/options.cpp
    tn5250/telnet/client.cpp
    # devices
    tn5250/devices/devices.cpp
    # client
    tn5250/client/ibmrseed.cpp
    tn5250/client/decoder.cpp
    tn5250/client/protocol_handler.cpp
    # message
    tn5250/message/message.cpp
    tn5250/message/operation_code.cpp
    tn5250/message/header/header.cpp
    tn5250/message/header/record_type.cpp
    tn5250/message/command/command.cpp
    tn5250/message/command/command_code.cpp
    tn5250/message/command/commands/command_cs_clear_screen.cpp
    tn5250/message/command/commands/command_wtd_write_to_display.cpp
    tn5250/message/command/commands/command_rmf_read_mdt_fields.cpp
    tn5250/message/command/order/order.cpp
    tn5250/message/command/order/order_code.cpp
    tn5250/message/command/order/orders/order_ic_insert_cursor.cpp
    tn5250/message/command/order/orders/order_ra_repeat_to_address.cpp
    tn5250/message/command/order/orders/order_sba_set_buffer_address.cpp
    tn5250/message/command/order/orders/order_sf_start_field.cpp
    tn5250/message/command/order/orders/order_soh_start_of_header.cpp
)

add_library(tn5250 STATIC ${TN5250_SOURCES})
add_library(tn5250::tn5250 ALIAS tn5250)

target_include_directories(tn5250
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(tn5250 PUBLIC OpenSSL::Crypto)

if(TN5250_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
