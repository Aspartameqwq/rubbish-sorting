cmake_minimum_required(VERSION 3.22)
project(rubbish_sorting_host_tests C)

enable_testing()

add_executable(firmware_host_tests
    test_main.c
    stubs/fake_hal.c
    ../../BSP/Src/servo.c
    ../../BSP/Src/tb6600.c
    ../../BSP/Src/tb6600_timing.c
    ../../Motion/Src/stepper.c
    ../../Motion/Src/stepper_profile.c
    ../../Protocol/Src/protocol.c
)

target_include_directories(firmware_host_tests PRIVATE
    stubs
    ../../App/Inc
    ../../BSP/Inc
    ../../Config
    ../../Motion/Inc
    ../../Protocol/Inc
)

target_compile_features(firmware_host_tests PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(firmware_host_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME firmware_host_tests COMMAND firmware_host_tests)
