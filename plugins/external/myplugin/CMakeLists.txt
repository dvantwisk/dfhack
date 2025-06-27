project(myplugin)
set(PROJECT_SRCS
    myplugin.cpp
)

dfhack_plugin(myplugin ${PROJECT_SRCS} LINK_LIBRARIES lua)

