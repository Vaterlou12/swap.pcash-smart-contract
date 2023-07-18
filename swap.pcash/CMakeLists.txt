cmake_minimum_required( VERSION 3.5 )

project(swap.pcash)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

if(${BUILD_TESTS})
    add_definitions(-DDEBUG)
endif()

if(${PREPROD})
    add_definitions(-DPREPROD)
endif()

include_directories(
tables
include
)

add_contract(swap.pcash swap.pcash
swap.pcash.cpp
)
