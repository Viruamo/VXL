cmake_minimum_required(VERSION 3.16)

# Chỉ định rõ ràng thư mục chứa các module tự viết
set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vxl_project)
