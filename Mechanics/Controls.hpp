#pragma once

#ifndef NCURSES_NOMACROS
#define NCURSES_NOMACROS
#endif
#include <ncurses.h>

#include "Platform/Arena.hpp"
#include "Object/Snake.hpp"

namespace cfg {

constexpr int InputQueueCap = 64;

}

enum class Command {
    None,
    Turn,
    Pause,
    Restart,
    Quit,
    Resize
};

struct InputEvent {
    Command command = Command::None;
    int player = 0;
    Direction direction = Direction::Right;
};

class InputQueue {
public:
    bool push(const InputEvent& e) {
        if (size_ >= cfg::InputQueueCap) return false;
        items_[size_++] = e;
        return true;
    }

    int size() const { return size_; }
    const InputEvent& at(int i) const { return items_[i]; }
    void clear() { size_ = 0; }

private:
    InputEvent items_[cfg::InputQueueCap];
    int size_ = 0;
};

namespace controls {

inline InputEvent translate(int key) {
    InputEvent e;

    switch (key) {
        case 'w': case 'W': e.command = Command::Turn; e.player = 0; e.direction = Direction::Up;    break;
        case 's': case 'S': e.command = Command::Turn; e.player = 0; e.direction = Direction::Down;  break;
        case 'a': case 'A': e.command = Command::Turn; e.player = 0; e.direction = Direction::Left;  break;
        case 'd': case 'D': e.command = Command::Turn; e.player = 0; e.direction = Direction::Right; break;

        case KEY_UP:    e.command = Command::Turn; e.player = 1; e.direction = Direction::Up;    break;
        case KEY_DOWN:  e.command = Command::Turn; e.player = 1; e.direction = Direction::Down;  break;
        case KEY_LEFT:  e.command = Command::Turn; e.player = 1; e.direction = Direction::Left;  break;
        case KEY_RIGHT: e.command = Command::Turn; e.player = 1; e.direction = Direction::Right; break;

        case 'p': case 'P': e.command = Command::Pause;   break;
        case 'r': case 'R': e.command = Command::Restart; break;
        case 'q': case 'Q': e.command = Command::Quit;    break;

        case KEY_RESIZE:    e.command = Command::Resize;  break;

        default: break;
    }
    return e;
}

inline void poll(InputQueue& queue) {
    queue.clear();
    for (;;) {
        const int key = wgetch(stdscr);
        if (key == ERR) break;

        const InputEvent e = translate(key);
        if (e.command != Command::None) {
            queue.push(e);
        }
    }
}

inline void steer(Snake& snake, Direction d) {
    snake.requestTurn(d);
}

inline void steer(Snake (&snakes)[PlayerCount], const InputEvent& e) {
    if (e.command != Command::Turn) return;
    if (e.player < 0 || e.player >= PlayerCount) return;
    steer(snakes[e.player], e.direction);
}

}