set(OOKDECODER_PATH ${CMAKE_CURRENT_LIST_DIR}/ookDecoder)
message("OOKDECODER_PATH=${OOKDECODER_PATH}")
add_library(external-lib-ookdecoder INTERFACE)
target_sources(external-lib-ookdecoder INTERFACE
"${OOKDECODER_PATH}/DecodeOOK.h"
"${OOKDECODER_PATH}/OregonDecoderV1.h"
"${OOKDECODER_PATH}/OregonDecoderV2.h"
"${OOKDECODER_PATH}/OregonDecoderV3.h"
)
target_include_directories(external-lib-ookdecoder INTERFACE "${OOKDECODER_PATH}")
