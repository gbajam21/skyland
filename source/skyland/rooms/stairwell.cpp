#include "stairwell.hpp"
#include "platform/platform.hpp"
#include "skyland/tile.hpp"


namespace skyland {


Stairwell::Stairwell(Island* parent, const Vec2<u8>& position)
    : Room(parent, name(), size(), position, Health(60))
{
}


void Stairwell::update(Platform& pfrm, App& app, Microseconds delta)
{
    Room::update(pfrm, app, delta);
}


void Stairwell::render_interior(Platform& pfrm, Layer layer)
{
    pfrm.set_tile(layer, position().x, position().y, InteriorTile::ladder_top);
    pfrm.set_tile(
        layer, position().x, position().y + 1, InteriorTile::ladder_mid_2);
    pfrm.set_tile(
        layer, position().x, position().y + 2, InteriorTile::ladder_mid);
    pfrm.set_tile(
        layer, position().x, position().y + 3, InteriorTile::ladder_base);
}


void Stairwell::render_exterior(Platform& pfrm, Layer layer)
{
    pfrm.set_tile(layer, position().x, position().y, Tile::wall_window_1);
    pfrm.set_tile(
        layer, position().x, position().y + 1, Tile::wall_window_middle_2);
    pfrm.set_tile(
        layer, position().x, position().y + 2, Tile::wall_window_middle_1);
    pfrm.set_tile(layer, position().x, position().y + 3, Tile::wall_window_2);
}



void Stairwell::plot_walkable_zones(bool matrix[16][16])
{
    // All tiles in a stairwell are walkable, that's kind of the point.
    for (int y = 0; y < size().y; ++y) {
        matrix[position().x][position().y + y] = true;
    }
}



} // namespace skyland
