set(RADIOHEAD_PATH ${CMAKE_CURRENT_LIST_DIR}/RadioHead)
message("RADIOHEAD_PATH=${RADIOHEAD_PATH}")
add_library(external-lib-radiohead INTERFACE)
target_sources(external-lib-radiohead INTERFACE
    "${RADIOHEAD_PATH}/RHGenericDriver.cpp"
    "${RADIOHEAD_PATH}/RHGenericDriver.h"
    "${RADIOHEAD_PATH}/RHSPIDriver.cpp"
    "${RADIOHEAD_PATH}/RHSPIDriver.h"
    "${RADIOHEAD_PATH}/RHGenericSPI.cpp"
    "${RADIOHEAD_PATH}/RHGenericSPI.h"
    "${RADIOHEAD_PATH}/RHSoftwareSPI.cpp"
    "${RADIOHEAD_PATH}/RHSoftwareSPI.h"
    "${RADIOHEAD_PATH}/RHHardwareSPI.cpp"
    "${RADIOHEAD_PATH}/RHHardwareSPI.h"
    "${RADIOHEAD_PATH}/RHCRC.cpp"
    "${RADIOHEAD_PATH}/RHCRC.h"
    "${RADIOHEAD_PATH}/RH_ASK.cpp"
    "${RADIOHEAD_PATH}/RH_ASK.h"
    "${RADIOHEAD_PATH}/RH_RF69.cpp"
    "${RADIOHEAD_PATH}/RH_RF69.h"
)
target_include_directories(external-lib-radiohead INTERFACE "${RADIOHEAD_PATH}")
