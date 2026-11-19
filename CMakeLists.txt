cmake_minimum_required(VERSION 3.21)
project(WebhookSender LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(WebhookSender WIN32 main.cpp)
target_link_libraries(WebhookSender PRIVATE Qt6::Widgets Qt6::Network)
