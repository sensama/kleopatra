set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)

set(kuniqueservicetest_src kuniqueservicetest.cpp ${CMAKE_SOURCE_DIR}/src/utils/kuniqueservice.cpp)

ecm_qt_declare_logging_category(kuniqueservicetest_src HEADER kleopatra_debug.h IDENTIFIER KLEOPATRA_LOG CATEGORY_NAME org.kde.pim.kleopatra)
add_executable(kuniqueservicetest ${kuniqueservicetest_src})
add_test(NAME kuniqueservicetest COMMAND kuniqueservicetest)
ecm_mark_as_test(kuniqueservicetest)
target_link_libraries(kuniqueservicetest Qt5::Test ${_kleopatra_dbusaddons_libs})
