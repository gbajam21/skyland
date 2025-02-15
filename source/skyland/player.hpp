#pragma once


#include "number/numeric.hpp"



class Platform;



namespace skyland {



class Room;
class App;



using MissileAmmo = short int;



class Player {
public:
    virtual ~Player()
    {
    }


    virtual void update(Platform&, App&, Microseconds delta)
    {
    }


    virtual void on_room_damaged(Platform& pfrm, App& app, Room& room)
    {
    }


    virtual void on_room_destroyed(Platform& pfrm, App& app, Room& room)
    {
        ++rooms_lost_;
    }


    int rooms_lost_ = 0;
    int rooms_built_ = 0;


    MissileAmmo& missile_ammo()
    {
        return missile_ammo_;
    }


private:
    MissileAmmo missile_ammo_;
};



} // namespace skyland
