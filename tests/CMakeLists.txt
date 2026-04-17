find_package(OpenSSL REQUIRED)

# Simple test framework - just uses assert
add_executable(test_ibmrseed test_ibmrseed.cpp)
target_link_libraries(test_ibmrseed PRIVATE tn5250 OpenSSL::Crypto)
add_test(NAME test_ibmrseed COMMAND test_ibmrseed)

add_executable(test_decoder test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE tn5250)
add_test(NAME test_decoder COMMAND test_decoder)

add_executable(test_message test_message.cpp)
target_link_libraries(test_message PRIVATE tn5250)
add_test(NAME test_message COMMAND test_message)
