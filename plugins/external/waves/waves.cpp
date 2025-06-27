#include "PluginManager.h"
#include "VTableInterpose.h"
#include "MiscUtils.h"

#include "df/enabler.h"
#include "df/event_handlerst.h"
#include "df/flow_info.h"
#include "df/graphic.h"
#include "df/renderer.h"
#include "df/renderer_2d.h"
#include "df/graphic_viewportst.h"
#include "df/map_block.h"
#include "df/ocean_wave.h"
#include "df/ocean_wave_maker.h"
#include "df/world.h"
#include "df/coord2d.h"

#include "modules/Maps.h"
#include "modules/Gui.h"

#include <list>
#include <algorithm>

DFHACK_PLUGIN("waves");

struct WaveVec {
    int x;
    int y;
};

struct WaveInfo {
    int id;
    int32_t texpos;
};

int target_time = 18;
int target_fps = 50;
int shore_dist = 20;  // waves always start this far out apparently.
int max_cycle = target_time * target_fps - 1;
int cycle = 0;
int max_noise = 5;

using WaveGrid = std::vector<std::vector<std::vector<WaveInfo>>>;

// int32_t **** ocean_waves_animation;
std::unordered_map<int, WaveGrid> grid_map;
std::vector<std::vector<std::vector<std::vector<WaveInfo>>>> ocean_waves_animation; // time, x, y
std::vector<int> owm_xmin;
std::vector<int> owm_xmax;
std::vector<int> owm_ymin;
std::vector<int> owm_ymax;
std::vector<int> coast_xmin;
std::vector<int> coast_xmax;
std::vector<int> coast_ymin;
std::vector<int> coast_ymax;
std::vector<WaveVec> waves_direction;

namespace std {
    template <>
    struct hash<df::coord2d> {
        std::size_t operator()(const df::coord2d& c) const noexcept {
            return std::hash<int32_t>()(c.x) ^ (std::hash<int32_t>()(c.y) << 1);
        }
    };
}

bool operator==(const df::coord2d& lhs, const df::coord2d& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

auto wm = df::global::world->event.ocean_wave_makers;

std::vector<bool> start_waves(wm.size(), false);

std::vector<int> anim_cycle(wm.size() + 1, max_cycle + 2);
std::vector<int> anim_trigger(wm.size() + 1, true);

std::unordered_map<df::coord2d, int> wave_maker_table;
std::unordered_map<df::coord2d, int> ocean_wave_maker_table;
std::unordered_map<df::coord2d, int> ocean_coastline_table;
std::unordered_map<int, std::vector<int>> ocean_wave_noise;
std::unordered_map<int, std::vector<int>> ocean_wave_vis_noise;
std::unordered_map<int, std::vector<std::vector<int>>> coast_grids;

std::vector<std::list<int>> wave_timers(wm.size());
std::vector<std::list<int>> wave_vis_noises(wm.size());
std::vector<std::list<int>> wave_noises(wm.size());

std::vector<std::vector<int>> vis_noise(wm.size());
std::vector<std::vector<int>> wave_noise(wm.size());

using namespace DFHack;
using namespace df;

df::coord cached_viewport = df::coord(0,0,0);

// Interpose wrapper for df::renderer
struct RendererInterpose : public df::renderer_2d {
    typedef renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, update_viewport_tile,
        (graphic_viewportst* vp, int32_t x, int32_t y)) {

        int32_t wx = cached_viewport.x + x;
        int32_t wy = cached_viewport.y + y;
        int32_t wz = cached_viewport.z;

        for(int i = 0; i < owm_xmin.size(); ++i) {
                    //std::cerr << "In update animation loop before i = " << i << "   cycle = " << cycle << "\n";
                    //std::cerr << "In update animation loop At start i = " << i << "   anim_cycle[i] = " << anim_cycle[i] << "\n";
            for (auto t = wave_timers[i].begin(); t != wave_timers[i].end();) {
                int anim_frame = *t;
            //if (anim_frame < max_cycle) {
            if (anim_frame < max_cycle) {
                    //std::cerr << "In update animation loop passed 1 i = " << i << "   cycle = " << cycle << "\n";

                if (wx >= owm_xmin[i] && wy >= owm_ymin[i] && wx < owm_xmax[i] && wy < owm_ymax[i]) {
                    //std::cerr << "In update animation loop passed i = " << i << "   anim_cycle[i] = " << anim_cycle[i] << "\n";
                    //std::cerr << "In update animation loop passed 2 i = " << i << "   cycle = " << cycle << "\n";
                    int32_t x_adj = wx - owm_xmin[i];   // align grid_map to viewport
                    int32_t y_adj = wy - owm_ymin[i];   // align grid_map to viewport
                    //std::cerr << "Checking for draw at x = " << x << "   y = " << y << "\n";
                    //std::cerr << "Checking for draw at x_adj = " << x_adj << "   y_adj = " << y_adj << "\n";
                    //std::cerr << "Checking for draw at wx = " << wx << "   wy = " << wy << "\n";
                    //std::cerr << "Checking for draw at owm_xmin[i] = " << owm_xmin[i] << "   owm_ymin[i] = " << owm_ymin[i] << "\n";
                    //std::cerr << "Checking for draw at owm_xmax[i] = " << owm_xmax[i] << "   owm_ymax[i] = " << owm_ymax[i] << "\n";
                    WaveInfo wi = WaveInfo(-1,-1);
                    if (grid_map[i][anim_frame][x_adj][y_adj].texpos >= 0) {
                        wi = grid_map[i][anim_frame][x_adj][y_adj];
                    }
                    if (false) {
                    for (int j = max_noise; j > 0; --j) {
                        //std::cerr << "In noise Loop start j = " << j << "\n";
                        if (anim_frame + j < max_cycle) { // check if wave in prev time exists
                            //std::cerr << "In noise Loop j = " << j << "\n";
                            int current_id = grid_map[i][anim_frame][x_adj][y_adj].id;
                            int temp_id = grid_map[i][anim_frame + j][x_adj][y_adj].id;
                            int search_id = -1;
                            if (temp_id >= 0) search_id = temp_id;
                            else if (current_id >= 0) search_id = current_id;
                            //std::cerr << "Searching lag at cycle = " << anim_frame << " at x = " << x << " y = " << y << "\n";
                            if (search_id > 0) {
                                //std::cerr << "Checking lag at cycle = " << anim_frame << " at x = " << x << " y = " << y << "\n";
                                WaveInfo search_wnoise = grid_map[i][anim_frame - ocean_wave_noise[i][search_id]][x_adj][y_adj];
                                bool id_match = temp_id == search_wnoise.id;
                                if (id_match) {
                                    wi = search_wnoise;
                                    //std::cerr << "Found lag at cycle = " << anim_cycle[i] << " at x = " << x << " y = " << y << "\n";
                                    break;
                                }
                            }
                        }
                    }
                    }
                    //std::cerr << "Check Texpos: x = " << x << "   y = " << y << "\n";
                    if (wi.texpos >= 0) {
                        //std::cerr << "Drawing at anim cycle: " << anim_cycle[i] << " at x = " << x << " and y = " << y << "\n";
                        //vp->screentexpos_designation[x * vp->dim_y + y] = 1;
                        if (true) {
                        //std::cerr << "Drawing wave at x = " << x << "   y = " << y << "\n";
                        if (ocean_wave_vis_noise[i][wi.id] < 1) {
                            vp->screentexpos_designation[x * vp->dim_y + y] = 221;
                        //std::cerr << "Drawing wave after 1 x = " << x << "   y = " << y << "\n";

                        }
                        else if (ocean_wave_vis_noise[i][wi.id] < 500) {
                            vp->screentexpos_designation[x * vp->dim_y + y] = 1;
                            //ocean_wave_vis_noise[i][wi.id]--;
                        //std::cerr << "Drawing wave after 2 x = " << x << "   y = " << y << "\n";

                        }
                        else if (ocean_wave_vis_noise[i][wi.id] < 700) {
                            vp->screentexpos_designation[x * vp->dim_y + y] = 9;
                            //ocean_wave_vis_noise[i][wi.id]--;
                        //std::cerr << "Drawing wave after 3 x = " << x << "   y = " << y << "\n";

                        }
                        else if (ocean_wave_vis_noise[i][wi.id] < 900) {
                            vp->screentexpos_designation[x * vp->dim_y + y] = 249;
                            //ocean_wave_vis_noise[i][wi.id]--;
                        //std::cerr << "Drawing wave after 4 x = " << x << "   y = " << y << "\n";

                        }
                        }
                    }
                }
                }
                break;
            }
            if (false) {
            if (wx >= coast_xmin[i] && wy >= coast_ymin[i] && wx < coast_xmax[i] && wy < coast_ymax[i]) {
                vp->screentexpos_signpost[x * vp->dim_y + y] = 94;
                df::map_block* block = Maps::getTileBlock(wx, wy, wz);
                for (auto* flow : block->flows) {
                    if (flow && flow->pos.x == wx && flow->pos.y == wy && flow->pos.z == wz) {
                        if (flow->type) {
                            std::cerr << "Found flow at x:" << wx << " and y:" << wy << "\n";
                            vp->screentexpos_designation[x * vp->dim_y + y] = 689;
                            std::cerr << "After found flow at x:" << wx << " and y:" << wy << "\n";

                            break;
                        }
                    }
                }
                //df::flow_info* temp_flow = &block->flows[wx&15][wy&15];
                //if (temp_flow && temp_flow->type == 11) {
                //    vp->screentexpos_designation[x * vp->dim_y + y] = 689;
                //}
            }
            }
        }

        if (false) {
        auto it3 = wave_maker_table.find(df::coord2d(wx, wy));
        if (it3 != wave_maker_table.end()) {
            vp->screentexpos_signpost[x * vp->dim_y + y] = it3->second;
            //std::cerr << "Found wave at x: " << x << "   y: " << y << "\n";
        }
        auto it = ocean_wave_maker_table.find(df::coord2d(wx, wy));
        if (it != ocean_wave_maker_table.end()) {
            vp->screentexpos_high_flow[x * vp->dim_y + y] = it->second;
            //std::cerr << "Found wave at x: " << x << "   y: " << y << "\n";
        }
        auto it2 = ocean_coastline_table.find(df::coord2d(wx, wy));
        if (it2 != ocean_coastline_table.end()) {
            vp->screentexpos_high_flow[x * vp->dim_y + y] = it2->second;
            //std::cerr << "Found wave at x: " << x << "   y: " << y << "\n";
        }
        }

        INTERPOSE_NEXT(update_viewport_tile)(vp, x, y);
    }
};

struct RenderInterpose : public df::renderer_2d {
    typedef renderer_2d interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, render, ()) {
        cached_viewport = Gui::getViewportPos();
        INTERPOSE_NEXT(render)();
    }
};

void initOceanWaveMaker() {
    
    std::cerr << "At Declaring min max.\n";
    for (int i = 0; i < wm.size(); ++i) {
        df::ocean_wave_maker * temp_ovm = wm[i];
        int xmin = 100000;
        int xmax = 0;
        int ymin = 100000;
        int ymax = 0;
        for (int j = 0; j < temp_ovm->wave_origin.size(); ++j) {
            int xma;
            int xmi;
            int yma;
            int ymi;
            if (temp_ovm->wave_origin.x[j] > temp_ovm->coastline.x[j]){
                xma = temp_ovm->wave_origin.x[j];
                xmi = temp_ovm->coastline.x[j];
            }
            else {
                xma = temp_ovm->coastline.x[j];
                xmi = temp_ovm->wave_origin.x[j];
            }
            if (temp_ovm->wave_origin.y[j] > temp_ovm->coastline.y[j]){
                yma = temp_ovm->wave_origin.y[j];
                ymi = temp_ovm->coastline.y[j];
            }
            else {
                yma = temp_ovm->coastline.y[j];
                ymi = temp_ovm->wave_origin.y[j];
            }
            if (xmin > xmi) xmin = xmi;
            if (xmax < xma) xmax = xma;
            if (ymin > ymi) ymin = ymi;
            if (ymax < yma) ymax = yma;
        }
        owm_xmin.push_back(xmin);
        owm_xmax.push_back(xmax);
        owm_ymin.push_back(ymin);
        owm_ymax.push_back(ymax);
    }

    int coast_expand = 10;
if(true) {
    for (int i = 0; i < wm.size(); ++i) {
        df::ocean_wave_maker * temp_owm = wm[i];

        int xmin = 100000;
        int ymin = 100000;
        int xmax = 0;
        int ymax = 0;

        for (int j = 0; j < temp_owm->coastline.x.size(); ++j) {
            int tempx = temp_owm->coastline.x[j];
            int tempy = temp_owm->coastline.y[j];

            if (tempx < xmin) xmin = tempx;
            if (tempx > xmax) xmax = tempx;
            if (tempy < ymin) ymin = tempy;
            if (tempy > ymax) ymax = tempy;
        }

        xmin = xmin - coast_expand;
        ymin = ymin - coast_expand;
        xmax = xmax + coast_expand;
        ymax = ymax + coast_expand;

        int32_t mx;
        int32_t my;
        int32_t mz;

        Maps::getTileSize(mx, my, mz);

        if (xmin < 0) xmin = 0;
        if (ymin < 0) ymin = 0;
        if (xmax > mx) xmax = 0;
        if (xmin > my) ymax = 0;

        coast_xmin.push_back(xmin);
        coast_ymin.push_back(ymin);
        coast_xmax.push_back(xmax);
        coast_ymax.push_back(ymax);
    }
}

    std::cerr << "Got Coordinates:\n";
    for (int i = 0; i < wm.size(); ++i) {
        std::cerr << "Wave maker i:" << i << " is at (" << owm_xmin[i] << "," << owm_ymin[i] << ") and (" << owm_xmax[i] << "," << owm_ymax[i] << ") with lengths xlen = " << (owm_xmax[i] - owm_xmin[i]) << " and ylen = " << (owm_ymax[i] - owm_ymin[i]) << "\n";
    }

    std::cerr << "At Declaring wave directions.\n";
    for (int i = 0; i < wm.size(); ++i) {
        int x_comp = wm[i]->wave_origin[0].x - wm[i]->coastline[0].x;
        int y_comp = wm[i]->wave_origin[0].y - wm[i]->coastline[0].y;

        if (x_comp == 0) {
            if (y_comp > 0) {
                waves_direction.push_back(WaveVec(0,-1));
            }
            else {
                waves_direction.push_back(WaveVec(0,1));
            }
        }
        else if (y_comp == 0) {
            if (x_comp > 0) {
                waves_direction.push_back(WaveVec(-1,0));
            }
            else {
                waves_direction.push_back(WaveVec(1,0));
            }
        }
        else {
            if (x_comp > 0 && y_comp > 0) {
                waves_direction.push_back(WaveVec(-1,-1));
            }
            else if (x_comp > 0 && y_comp < 0) {
                waves_direction.push_back(WaveVec(-1,1));
            }
            else if (x_comp < 0 && y_comp > 0) {
                waves_direction.push_back(WaveVec(1,-1));
            }
            else {
                waves_direction.push_back(WaveVec(1,1));
            }
        }
    }

    std::cerr << "Got directions:\n";
    for (int i = 0; i < wm.size(); ++i) {
        std::cerr << "Wave direction at i:" << i << " is (" << waves_direction[i].x << "," << waves_direction[i].y << ")\n";
    }

    std::cerr << "At Declaring standard coastal grid.\n";
    for (int i = 0; i < wm.size(); ++i) {
        int x1 = owm_xmin[i];
        int y1 = owm_ymin[i];

        int x2 = owm_xmax[i];
        int y2 = owm_ymax[i];

        int xlen = x2 - x1;
        int ylen = y2 - y1;

        std::cerr << "At standard coastal grid: finished declaring standard dims. xlen = " << xlen << "   ylen = " << ylen << "\n";
        std::cerr << "At standard coastal grid: finished declaring standard dims. x1 = " << x1 << " y1 = " << y1 << "   x2 = " << x2 << " y2 = " << y2 << "\n";

        std::vector<std::vector<int>> coast(xlen + 1, std::vector<int>(ylen + 1, -1));

        //std::cerr << "At standard coastal grid: created vector.\n";

        df::ocean_wave_maker * temp_ovm = df::global::world->event.ocean_wave_makers[i];
        //std::cerr << "At standard coastal grid: ocean wave makers.\n";

        for (int b = 0; b < temp_ovm->wave_origin.size(); ++b) {
            //std::cerr << "At standard coastal grid: Start loop " << b << "  wave_origin x" << temp_ovm->wave_origin.x[b] << "  wave_origin y " << temp_ovm->wave_origin.y[b] << "  -x1: " << temp_ovm->wave_origin.x[b] - x1 << "  -y1: " << temp_ovm->wave_origin.y[b] - y1 << "\n";
            coast[temp_ovm->wave_origin.x[b] - x1][temp_ovm->wave_origin.y[b] - y1] = -2;
            //std::cerr << "At standard coastal grid: Start loop " << b << "  coastline x" << temp_ovm->coastline.x[b] << "  coastline y " << temp_ovm->coastline.y[b] << "  -x1: " << temp_ovm->coastline.x[b] - x1 << "  -y1: " << temp_ovm->coastline.y[b] - y1 << "\n";
            coast[temp_ovm->coastline.x[b] - x1][temp_ovm->coastline.y[b] - y1] = -3;
            //std::cerr << "At standard coastal grid: after find coastlines " << b << "\n";
        }
        coast_grids[i] = coast;
    }

    std::cerr << "After standard coastal grid.\n";

    int total_time = target_fps * target_time;
    int time_per_tile = total_time / shore_dist;

    std::cerr << "At Declaring animations.\n";

    for (int i = 0; i < wm.size(); ++i) {
        std::cerr << "In Animation i = " << i << "\n";
        int x1 = owm_xmin[i];
        int y1 = owm_ymin[i];

        int x2 = owm_xmax[i];
        int y2 = owm_ymax[i];

        std::vector<int> temp_noise(wm[i]->wave_origin.x.size(), 0);
        std::vector<int> temp_vis_noise(wm[i]->wave_origin.x.size(), 0);

        ocean_wave_noise[i] = temp_noise;
        ocean_wave_vis_noise[i] = temp_vis_noise;
        WaveVec wv = waves_direction[i];

        int xlen = x2 - x1;
        int ylen = y2 - y1;

        WaveGrid g(total_time + 1, std::vector<std::vector<WaveInfo>>(xlen + 1, std::vector<WaveInfo>(ylen + 1, WaveInfo(-1, -1))));

        df::ocean_wave_maker * temp_ovm = df::global::world->event.ocean_wave_makers[i];

        std::vector<int> stopped(temp_ovm->wave_origin.size() + 1, 0);

        std::cerr << "In Animation before loops i = " << i << "   WaveVec.x = " << wv.x << "   WaveVec.y = " << wv.y << "\n";

        for (int a = 0; a < total_time; ++a) {
        //std::cerr << "In Animation frame loops a = " << a << "\n";

            for (int b = 0; b < temp_ovm->wave_origin.size(); ++b) {
            //std::cerr << "In Animation wave loops b = " << b << "\n";

                    if (stopped[b]) continue;
                    int wx = temp_ovm->wave_origin.x[b] - x1;
                    int wy = temp_ovm->wave_origin.y[b] - y1;

                    int multi = a/time_per_tile;
                    int x_new = wx + wv.x * multi;
                    int y_new = wy + wv.y * multi;

                    //std::cerr << "In Animation After declarations = " << b << "  wx = " << wx << "  wy " << wy <<"\n";
                    //std::cerr << "In Animation After declarations = " << b << "  multi = " << multi << "  x_new = " << x_new << "  y_new = " << y_new << "\n";

                    if (coast_grids[i][x_new][y_new] == -3){
                        stopped[b] = 1;
                        continue;
                    }

                    //std::cerr << "In Animation After stopped declarations\n";

                    if (coast_grids[i][x_new][y_new] > -3) {
                        g[a][x_new][y_new].id = b;
                        g[a][x_new][y_new].texpos = 1;
                    }

                    //std::cerr << "In Animation After add grid\n";

                
            }
        }
        grid_map[i] = g;
        stopped.clear();
    }    
    
    std::cerr << "At Declaring visual coastal guides.\n";

    for (int i = 0; i < df::global::world->event.ocean_wave_makers.size(); ++i) {
        df::ocean_wave_maker * temp_ovm = df::global::world->event.ocean_wave_makers[i];
        wave_maker_table[df::coord2d(temp_ovm->pos.x, temp_ovm->pos.y)] = 778;
        for (int j = 0; j < temp_ovm->wave_origin.size(); ++j) {
            ocean_wave_maker_table[df::coord2d(temp_ovm->wave_origin.x[j], temp_ovm->wave_origin.y[j])] = i+3;
            ocean_coastline_table[df::coord2d(temp_ovm->coastline.x[j], temp_ovm->coastline.y[j])] = i+3;
        }
    }
}

// Install interpose
IMPLEMENT_VMETHOD_INTERPOSE(RendererInterpose, update_viewport_tile);

IMPLEMENT_VMETHOD_INTERPOSE(RenderInterpose, render);


DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    
    return CR_OK;
}

static void generate_wave_noise(int i) {
    for (int n = 0; n < wm[i]->wave_origin.x.size(); ++n) {
        ocean_wave_noise[i][n] = random_int(max_noise) - max_noise - 1;
    }
}

static void generate_vis_noise(int i) {
    for (int n = 0; n < wm[i]->wave_origin.x.size(); ++n) {
        ocean_wave_vis_noise[i][n] = random_int(300) + 600;
    }
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    if (true) {
        for (int i = 0; i < wm.size(); ++i) {
            if (start_waves[i]) {
                for (int j = 0; j < wm[i]->wave_origin.x.size(); ++j) {
                    ocean_wave_vis_noise[i][j]--;
                }
            }
            if (anim_trigger[i] && wm[i]->interval == 1) {
                wave_timers[i].push_back(0);
                //std::vector<int> temp_vis(wm[i]->wave_origin.x.size());
                generate_vis_noise(i);
                //wave_vis_noises.pushback(temp_vis);
                //std::vector<int> temp_noise(wm[i]->wave_origin.x.size());
                generate_wave_noise(i);
                //wave_noises.pushback(temp_noise);
                //anim_cycle[i] = 0;
                anim_trigger[i] = false;
                //generate_wave_noise(i);
                //generate_vis_noise(i);
                start_waves[i] = true;
                //for (int k = 0; k < ocean_wave_noise[i].size(); ++k) {
                    //std::cerr << "Showing new ocean noise at i:" << i << " k:" << k << " noise:" << ocean_wave_noise[i][k] << "\n";
                //}
            }
            else if (!anim_trigger[i] && wm[i]->interval > 3) {
                anim_trigger[i] = true;
            }
            for (auto it = wave_timers[i].begin(); it != wave_timers[i].end(); ) {
                ++(*it);
                if (*it > max_cycle + 100) {
                    it = wave_timers[i].erase(it); // remove finished countdown
                } else {
                    ++it;
                }
            }
            
        }
    }
    if (false) {
        for (int i = 0; i < wm.size(); ++i) {
            anim_cycle[i]++;
            if (anim_trigger[i] && wm[i]->interval == 0) {
                anim_cycle[i] = 0;
                anim_trigger[i] = false;
                //generate_wave_noise(i);
                //generate_vis_noise(i);
            }
            else if (!anim_trigger[i] && wm[i]->interval > 3) {
                anim_trigger[i] = true;
            }
        }
    }
    return CR_OK;
}

command_result waves_cmd(color_ostream &out, std::vector<std::string> & args)
{
    bool enable = true;

    if (!args.empty()) {
        if (args[0] == "0" || args[0] == "false" || args[0] == "off" || args[0] == "disable")
            enable = false;
    }

    auto wm = df::global::world->event.ocean_wave_makers;

    if (wm.size() == 0) {
        out.printerr("No wave makers. Doing Nothing.\n");
        return CR_OK;
    }

    if (enable) {
        if (!INTERPOSE_HOOK(RendererInterpose, update_viewport_tile).apply(true)) {
            out.printerr("Failed to interpose update_viewport_tile\n");
            return CR_FAILURE;
        }
        if (!INTERPOSE_HOOK(RenderInterpose, render).apply(true)) {
            out.printerr("Failed to interpose update_viewport_tile\n");
            return CR_FAILURE;
        }
        initOceanWaveMaker();
        out.print("Interpose on update_viewport_tile enabled.\n");
    } else {
        ocean_waves_animation.clear();
        owm_xmin.clear();
        owm_ymin.clear();
        owm_xmax.clear();
        owm_ymax.clear();
        waves_direction.clear();
        wave_maker_table.clear();
        ocean_wave_noise.clear();
        ocean_wave_vis_noise.clear();
        ocean_wave_maker_table.clear();
        ocean_coastline_table.clear();
        grid_map.clear();
        coast_grids.clear();
        wave_timers.clear();
        wave_noises.clear();
        wave_vis_noises.clear();
        INTERPOSE_HOOK(RendererInterpose, update_viewport_tile).apply(false);
        INTERPOSE_HOOK(RenderInterpose, render).apply(false);
        out.print("Interpose on update_viewport_tile disabled.\n");
    }
    return CR_OK;
}

// Plugin load and unload
DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{

commands.push_back(PluginCommand(
    "waves",
    "Intercept tile rendering",
    waves_cmd
));

    return CR_OK;
}
