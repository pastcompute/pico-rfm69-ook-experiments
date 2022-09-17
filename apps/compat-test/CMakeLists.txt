add_executable(compat main.cpp)

target_include_directories(compat PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../lib
)

target_link_libraries(compat arduino-compat)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(compat)
