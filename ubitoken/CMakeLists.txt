add_contract(ubitoken ubitoken ${CMAKE_CURRENT_SOURCE_DIR}/src/ubitoken.cpp)
target_include_directories(ubitoken.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(ubitoken.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
