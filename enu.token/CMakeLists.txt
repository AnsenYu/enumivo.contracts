add_contract(enu.token enu.token ${CMAKE_CURRENT_SOURCE_DIR}/src/enu.token.cpp)
target_include_directories(enu.token.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(enu.token.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
