#pragma once

#include "Mechanics/Collision.hpp"
#include "Mechanics/Growth.hpp"
#include "Object/Snake.hpp"
#include "Platform/Arena.hpp"

namespace snatch {

struct Steal {
    int index = -1;  
    int value = 0;   

    bool active() const { return index >= 0; }
    bool wipesVictim() const { return index == 0; }  
};


inline int targetIndex(const Snake& foe, Vec2 cell, bool foeGrows) {
    int index = foe.indexOf(cell);
    return collision::isSolid(foe, index, foeGrows) ? index : -1;
}


inline bool succeeds(const Snake& attacker, const Snake& foe, int index) {
    return attacker.headValue() > foe.valueAt(index);
}

inline Steal aim(const Snake& foe, int index) { return {index, foe.valueAt(index)}; }


inline void cut(Snake& victim, const Steal& steal) { victim.truncate(steal.index); }


inline void reward(Snake& attacker, const Steal& steal) {
    if (steal.active()) growth::addCredit(attacker, steal.value);
}

}  