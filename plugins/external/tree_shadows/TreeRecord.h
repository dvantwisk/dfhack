#ifndef TREE_RECORD_H
#define TREE_RECORD_H

#include <unordered_map>
#include <list>
#include <utility>
#include <iostream>

#include "modules/Textures.h"

using namespace DFHack;

struct TreeRecord {

    df::coord pos;
    int dim_x, dim_y;
    int local_trunk_height;
    int height;

    TexposHandle shadow_map[490];

    TreeRecord() {
        pos = df::coord(-3000,-3000,-3000);
        dim_x = 7;
        dim_y = 7;
        height = 10;
        local_trunk_height = 2;
        for (int z = 0; z < height; ++z) {
            for (int x = 0; x < dim_x; ++x) {
                for (int y = 0; y < dim_y; ++y) {
                   shadow_map[z * dim_x * dim_y + x * dim_y + y] = 0;
                }
            }
        }
    }
};

#endif // TREE_RECORD_H
