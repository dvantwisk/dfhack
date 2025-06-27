project(tree_shadows)
set(PROJECT_SRCS
    tree_shadows.cpp
)

dfhack_plugin(tree_shadows ${PROJECT_SRCS} LINK_LIBRARIES lua)
