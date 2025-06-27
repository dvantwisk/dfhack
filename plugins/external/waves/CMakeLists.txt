project(waves)
set(PROJECT_SRCS
    waves.cpp
)

dfhack_plugin(waves ${PROJECT_SRCS} LINK_LIBRARIES lua)
