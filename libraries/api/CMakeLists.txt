include_directories( "${CMAKE_SOURCE_DIR}/libraries/fc/include" )

file(GLOB HEADERS "include/bts/api/*.hpp")

add_executable( bts_api_generator 
                bts_api_generator.cpp )
target_include_directories( bts_api_generator
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" 
                                    "${CMAKE_SOURCE_DIR}/libraries/utilities/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/db/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/blockchain/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/wallet/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/net/include"
                                    "${CMAKE_SOURCE_DIR}/libraries/client/include" )
target_link_libraries( bts_api_generator fc bts_utilities leveldb )


set(json_description_files "${CMAKE_CURRENT_SOURCE_DIR}/blockchain_api.json"
                           "${CMAKE_CURRENT_SOURCE_DIR}/wallet_api.json"
                           "${CMAKE_CURRENT_SOURCE_DIR}/network_api.json"
                           "${CMAKE_CURRENT_SOURCE_DIR}/general_api.json")
source_group("JSON Files" FILES ${json_description_files})

set(generated_api_files "${CMAKE_CURRENT_BINARY_DIR}/include/bts/api/common_api.hpp")
set(rpc_stubs_output_dir "${CMAKE_BINARY_DIR}/libraries/rpc_stubs")
set(generated_rpc_stubs_files "${rpc_stubs_output_dir}/common_api_server.cpp" 
                             "${rpc_stubs_output_dir}/common_api_client.cpp" 
                             "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_server.hpp" 
                             "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_client.hpp"
                             "${rpc_stubs_output_dir}/include/bts/rpc_stubs/common_api_overrides.ipp")

add_custom_command(OUTPUT ${generated_api_files} ${generated_rpc_stubs_files}
                   COMMAND bts_api_generator "--api-classname=common_api" 
                                             "--api-interface-output-dir=${CMAKE_CURRENT_BINARY_DIR}"
                                             "--rpc-stub-output-dir=${rpc_stubs_output_dir}" ${json_description_files}
                   DEPENDS bts_api_generator ${json_description_files} )

add_library(bts_api STATIC ${HEADERS} "conversion_functions.cpp" ${json_description_files} ${generated_api_files})
target_include_directories(bts_api
                           PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" 
                                  "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(bts_rpc_stubs ${generated_rpc_stubs_files})
target_link_libraries( bts_rpc_stubs 
                       PRIVATE fc
                       PUBLIC bts_api bts_wallet bts_net bts_blockchain)
target_include_directories(bts_rpc_stubs
                           PUBLIC "${rpc_stubs_output_dir}/include")