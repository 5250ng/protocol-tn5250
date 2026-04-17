find_package(OpenSSL REQUIRED)

# Simple test framework - just uses assert
add_executable(test_ibmrseed test_ibmrseed.cpp)
target_link_libraries(test_ibmrseed PRIVATE tn5250 OpenSSL::Crypto)
add_test(NAME test_ibmrseed COMMAND test_ibmrseed)

add_executable(test_decoder test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE tn5250)
add_test(NAME test_decoder COMMAND test_decoder)

add_executable(test_order_sf test_order_sf.cpp)
target_link_libraries(test_order_sf PRIVATE tn5250)
add_test(NAME test_order_sf COMMAND test_order_sf)

add_executable(test_order_soh test_order_soh.cpp)
target_link_libraries(test_order_soh PRIVATE tn5250)
add_test(NAME test_order_soh COMMAND test_order_soh)

add_executable(test_orders test_orders.cpp)
target_link_libraries(test_orders PRIVATE tn5250)
add_test(NAME test_orders COMMAND test_orders)
