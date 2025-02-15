#pragma once

#include "entity.hpp"
#include "entity/character/basicCharacter.hpp"
#include "health.hpp"
#include "number/numeric.hpp"
#include "platform/layer.hpp"
#include "power.hpp"
#include "scene.hpp"
#include <memory>



class Platform;



namespace skyland {



class App;
class Entity;
class Island;
class RoomMeta;
class Cannonball;
class BasicCharacter;



struct Conditions {
    enum Value : u32 {
        none = 0,
        workshop_required = (1 << 0),
        not_constructible = (1 << 1),
    };
};



class Room {
public:
    virtual ~Room()
    {
    }


    using Icon = u16;



    Room(Island* parent,
         const char* name,
         const Vec2<u8>& size,
         const Vec2<u8>& position,
         Health health);


    virtual bool add_occupant(EntityRef<BasicCharacter> entity)
    {
        characters_.push(std::move(entity));
        return true;
    }


    virtual void render_interior(Platform& pfrm, Layer layer) = 0;
    virtual void render_exterior(Platform& pfrm, Layer layer) = 0;


    void set_injured(Platform& pfrm);


    virtual void update(Platform& pfrm, App&, Microseconds delta);


    Island* other_island(App&);


    virtual bool has_roof()
    {
        return true;
    }


    virtual bool disallow_chimney()
    {
        return false;
    }


    virtual bool has_chimney()
    {
        return false;
    }


    const Vec2<u8>& position() const
    {
        return position_;
    }


    Vec2<u8>& __position()
    {
        return position_;
    }


    const Vec2<u8>& size() const
    {
        return size_;
    }


    virtual ScenePtr<Scene> select(Platform& pfrm, App&);


    virtual void set_target(const Vec2<u8>& target)
    {
    }


    virtual void unset_target()
    {
    }


    void on_collision(Platform& pfrm, App& app, Entity& entity);


    void apply_damage(Platform&, App&, Health damage);


    void plunder(Platform&, App&, Health damage);


    virtual void plot_walkable_zones(bool matrix[16][16]);


    RoomMeta* metaclass()
    {
        return metaclass_;
    }


    Vec2<Float> origin() const;


    Vec2<Float> center() const;


    Island* parent() const
    {
        return parent_;
    }


    EntityList<BasicCharacter>& characters()
    {
        return characters_;
    }


    Health health() const
    {
        return health_;
    }


    static Power consumes_power()
    {
        return 10;
    }


    static Conditions::Value conditions()
    {
        return Conditions::none;
    }


    static Icon icon()
    {
        return 450;
    }


    static Icon unsel_icon()
    {
        return 450;
    }


    Health max_health() const
    {
        return max_health_;
    }


    void heal(Health amount)
    {
        const Health new_health = health_ + amount;
        health_ = std::min(max_health_, new_health);
    }


    void set_health(Health amount)
    {
        health_ = amount;
    }


    virtual bool description_visible();


private:
    Island* parent_;
    RoomMeta* metaclass_;
    EntityList<BasicCharacter> characters_;
    Vec2<u8> size_;
    Vec2<u8> position_;
    Health health_;
    Health max_health_;
    Microseconds injured_timer_;
};



template <typename T> using RoomPtr = std::unique_ptr<T, void (*)(Room*)>;



} // namespace skyland
