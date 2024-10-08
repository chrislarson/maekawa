set(LIB_SOURCES Networking.cpp Logger.cpp CDistributedMutex.cpp)
set(LIB_HEADERS Networking.h Logger.h CDistributedMutex.h)
set(LIB_INCLUDES "./")

add_library(${LIB_NAME} STATIC ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(${LIB_NAME} PUBLIC ${LIB_INCLUDES})

