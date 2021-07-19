cmake_minimum_required(VERSION 3.10)
project(otus8 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(Boost_USE_STATIC_LIBS ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")

add_compile_options(-Wall -Wextra -pedantic -Werror)

find_package(Boost COMPONENTS unit_test_framework program_options filesystem regex REQUIRED)
find_package(Threads REQUIRED)

add_library(bulk_lib bulk.cpp bulk.h response_handler.cpp response_handler.h)

add_library(async async.cpp async.h)
target_link_libraries(async bulk_lib Threads::Threads)

add_executable(otus8 main.cpp)
set_target_properties(otus8 PROPERTIES
        COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
        )
target_link_libraries(otus8 ${Boost_LIBRARIES} bulk_lib async)

add_executable(test_bulk test_bulk.cpp)
set_target_properties(test_bulk PROPERTIES
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
target_link_libraries(test_bulk bulk_lib ${Boost_LIBRARIES})

add_executable(test_async test_async.cpp)
set_target_properties(test_async PROPERTIES
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
target_link_libraries(test_async async ${Boost_LIBRARIES})

enable_testing()
add_test(test_bulk test_bulk)
add_test(test_async test_async)


install(TARGETS otus8 RUNTIME DESTINATION bin)
set(CPACK_GENERATOR DEB)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEB_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${otus8_VERSION})
set(CPACK_PACKAGE_CONTACT buevichd@yandex-team.ru)
include(CPack)
