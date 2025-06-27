project(extra_building_graphics)
set(PROJECT_SRCS
    extra_building_graphics.cpp
)

dfhack_plugin(extra_building_graphics ${PROJECT_SRCS} LINK_LIBRARIES lua)

