set(APP_SOURCES main.cpp)

add_executable(${APP_NAME} ${APP_SOURCES})

target_link_libraries(${APP_NAME} ${LIB_NAME})

