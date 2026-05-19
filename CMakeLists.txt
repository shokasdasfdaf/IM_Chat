cmake_minimum_required(VERSION 3.16)
project(IM_Chat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets Sql)

# --- Common (static library) ---
add_library(common STATIC
    src/common/protocol.h
    src/common/protocol.cpp
)
target_include_directories(common PUBLIC src/common)
target_link_libraries(common PUBLIC Qt6::Core Qt6::Network)

# --- Server (console) ---
add_executable(server
    src/server/main.cpp
    src/server/chatserver.h
    src/server/chatserver.cpp
    src/server/clienthandler.h
    src/server/clienthandler.cpp
)
target_link_libraries(server PRIVATE common Qt6::Network Qt6::Sql)

# --- Client (GUI) ---
add_executable(client
    src/client/main.cpp
    src/client/chatclient.h
    src/client/chatclient.cpp
    src/client/ui/mainwindow.h
    src/client/ui/mainwindow.cpp
    src/client/ui/loginwidget.h
    src/client/ui/loginwidget.cpp
)
target_link_libraries(client PRIVATE common Qt6::Network Qt6::Widgets)
