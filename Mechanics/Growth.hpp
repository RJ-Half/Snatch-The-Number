#pragma once

#include "Platform/Arena.hpp"
#include "Object/Snake.hpp"

namespace growth {

inline int addCredit(Snake& s, int credit) {
    if (credit <= 0) return 0;

    int left = cfg::MaxHeadValue - s.headValue();
    if (left <= 0) return 0;

    int accepted = credit < left ? credit : left;
    s.setPending(s.pending() + accepted);
    return accepted;
}

inline bool growsThisTick(const Snake& s) {
    return s.pending() > 0;
}

inline void advance(Snake& s, Vec2 newHead, bool grow) {
    if (grow && s.pending() > 0) {
        s.growTo(newHead);
        s.setPending(s.pending() - 1);
    } else {
        s.moveTo(newHead);
    }
}

}