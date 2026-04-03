# ============================================================================
# CMakeLists fragment for MS5611 driver — SRAD 2026 Flight Computer
#
# Copy this into your project's CMakeLists.txt or include() it.
# Adjust the paths to match your directory structure.
# ============================================================================

# --- MS5611 driver library ---------------------------------------------------
add_library(ms5611
    ${CMAKE_CURRENT_LIST_DIR}/driver_ms5611.c
    ${CMAKE_CURRENT_LIST_DIR}/ms5611_interface_rp2040.c
)

target_include_directories(ms5611 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ms5611 PUBLIC
    pico_stdlib
    hardware_i2c
    hardware_gpio
)

# --- Link to your main target ------------------------------------------------
# In your top-level CMakeLists.txt, after defining your executable, add:
#
#   target_link_libraries(your_main_target PRIVATE ms5611)
#
# That's it — the PUBLIC include dirs and libs propagate automatically.
