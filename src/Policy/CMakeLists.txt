#* Liam Ashdown
#* Copyright (C) 2019
#*
#* This program is free software: you can redistribute it and/or modify
#* it under the terms of the GNU General Public License as published by
#* the Free Software Foundation, either version 3 of the License, or
#* (at your option) any later version.
#*
#* This program is distributed in the hope that it will be useful,
#* but WITHOUT ANY WARRANTY; without even the implied warranty of
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#* GNU General Public License for more details.
#*
#* You should have received a copy of the GNU General Public License
#* along with this program.  If not, see <http://www.gnu.org/licenses/>.
#*

# Executable Name
set(EXECUTABLE_NAME Policy)

# Include Directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Server)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Server/Opcodes)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Server/Opcodes/Packets)
include_directories(${CMAKE_SOURCE_DIR}/src/Engine)

file(GLOB_RECURSE SOURCE_LIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp" "*.hpp")

foreach(SOURCE IN LISTS SOURCE_LIST)
    get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${SOURCE_PATH}")
    source_group("${source_path_msvc}" FILES "${SOURCE}")
endforeach()

# Add Executable
add_executable(${EXECUTABLE_NAME} ${SOURCE_LIST})

# External Link Libaries
target_link_libraries(${EXECUTABLE_NAME} 
  PRIVATE ${OPENSSL_LIBRARIES}
  PRIVATE ${Boost_LIBRARIES}
  PRIVATE ${MYSQL_LIBRARY}
  Engine
)

# External Link Includes
target_include_directories(${EXECUTABLE_NAME}
  PRIVATE ${Boost_INCLUDE_DIRS}
  PRIVATE ${OPENSSL_INCLUDE_DIR}
  PRIVATE ${MYSQL_INCLUDE_DIR}
)

# Define OutDir to SOURCE/bin/(platform)_(configuaration) folder.
set_target_properties(${EXECUTABLE_NAME} PROPERTIES PROJECT_LABEL "Policy")