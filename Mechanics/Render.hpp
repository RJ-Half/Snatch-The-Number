#pragma once
#ifndef NCURSES_NOMACROS
#define NCURSES_NOMACROS
#endif
#include <ncurses.h>
#include <unistd.h>
#include "Platform/Arena.hpp"
#include "Object/Snake.hpp"
#include "Object/Food.hpp"
#include "Platform/Menu.hpp"
namespace cfg
{
    constexpr int TickMs = 100;
    constexpr int SliceMs = 5;
}

inline unsigned int processSeed()
{
    unsigned int s = static_cast<unsigned int>(getpid()) * 2654435761u;
    if (s == 0u)
        s = 0x9E3779B9u;
    for (int i = 0; i < 8; ++i)
    {
        s ^= (s << 13);
        s ^= (s >> 17);
        s ^= (s << 5);
    }
    if (s == 0u)
        s = 0x9E3779B9u;
    return s;
}

class TickTimer
{
private:
    int elapsed_ = 0;

public:
    void sleepSlice()
    {
        napms(cfg::SliceMs);
        elapsed_ += cfg::SliceMs;
    }
    bool ready()
    {
        if (elapsed_ < cfg::TickMs)
            return false;
        elapsed_ = 0;
        return true;
    }
    void reset() { elapsed_ = 0; }
};

class Terminal
{
private:
    bool active_ = false;

public:
    Terminal()
    {
        if (initscr() == nullptr)
            return;
        active_ = true;
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        nodelay(stdscr, TRUE);
    }
    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;
    ~Terminal()
    {
        if (active_)
        {
            endwin();
        }
    }
    bool ok() const
    {
        return active_;
    }
};

enum class Tone
{
    P1Normal = 0,
    P1Dark,
    P2Normal,
    P2Dark,
    Food1to3,
    Food4to6,
    Food7to8,
    Food9
};
namespace detail
{
    inline Tone snakeTone(int player, bool pending)
    {
        if (player == 0)
            return pending ? Tone::P1Dark : Tone::P1Normal;
        if (player == 1)
            return pending ? Tone::P2Dark : Tone::P2Normal;
        return Tone::P1Normal;
    }
    inline Tone foodTone(int value)
    {
        if (value <= 3)
            return Tone::Food1to3;
        if (value <= 6)
            return Tone::Food4to6;
        if (value <= 8)
            return Tone::Food7to8;
        return Tone::Food9;
    }
}

class Colors
{
private:
    enum class Mode
    {
        None,
        Basic8,
        Rich256
    };
    Mode mode_ = Mode::None;

public:
    void init()
    {
        if (!has_colors())
        {
            mode_ = Mode::None;
            return;
        }
        start_color();
        if (COLORS >= 256)
        {
            mode_ = Mode::Rich256;
            const short fg[8] = {39, 27, 118, 34, 226, 208, 135, 196};
            for (int i = 0; i < 8; ++i)
            {
                init_pair(static_cast<short>(i + 1), fg[i], COLOR_BLACK);
            }
        }
        else
        {
            mode_ = Mode::Basic8;
            const short fg[8] = {COLOR_CYAN, COLOR_BLUE, COLOR_GREEN,
                                 COLOR_GREEN, COLOR_YELLOW, COLOR_YELLOW,
                                 COLOR_MAGENTA, COLOR_RED};
            for (int i = 0; i < 8; ++i)
            {
                init_pair(static_cast<short>(i + 1), fg[i], COLOR_BLACK);
            }
        }
    }
    attr_t attr(Tone tone) const
    {
        const short pair = static_cast<short>(static_cast<int>(tone) + 1);
        if (mode_ == Mode::Rich256)
        {
            if (tone == Tone::Food9)
                return COLOR_PAIR(pair) | A_BOLD;
            return COLOR_PAIR(pair);
        }
        if (mode_ == Mode::Basic8)
        {
            if (tone == Tone::P1Normal || tone == Tone::P2Normal)
                return COLOR_PAIR(pair) | A_BOLD;
            if (tone == Tone::Food1to3 || tone == Tone::Food9)
                return COLOR_PAIR(pair) | A_BOLD;
            return COLOR_PAIR(pair);
        }
        switch (tone)
        {
        case Tone::P1Normal:
            return A_BOLD;
        case Tone::P1Dark:
            return A_NORMAL;
        case Tone::P2Normal:
            return A_BOLD | A_UNDERLINE;
        case Tone::P2Dark:
            return A_UNDERLINE;
        case Tone::Food1to3:
            return A_NORMAL;
        case Tone::Food4to6:
            return A_BOLD;
        case Tone::Food7to8:
            return A_UNDERLINE;
        case Tone::Food9:
            return A_REVERSE;
        default:
            return A_NORMAL;
        }
    }
};

class Renderer
{
private:
    Terminal term_;
    Colors colors_;
    ArenaWindow win_;
    void drawSnake(WINDOW *w, const Snake &snake, attr_t base) const
    {
        for (int i = snake.length() - 1; i >= 0; --i)
        {
            const Vec2 p = snake.segment(i);
            attr_t at = base;
            if (i == 0)
            {
                at |= A_REVERSE;
            }
            wattron(w, at);
            if (i == 0 && snake.isDead())
            {
                mvwprintw(w, p.y, cfg::CellWidth * p.x, "XX");
            }
            else
            {
                mvwprintw(w, p.y, cfg::CellWidth * p.x, "%2d", snake.valueAt(i));
            }
            wattroff(w, at);
        }
    }

public:
    Renderer()
    {
        if (term_.ok())
        {
            colors_.init();
        }
    }
    bool ok() const { return term_.ok(); }
    bool open(Arena &arena)
    {
        return term_.ok() && win_.open(arena);
    }
    bool fits() const { return win_.fits(); }
    int needCols() const { return win_.needCols(); }
    int needRows() const { return win_.needRows(); }
    bool rebuild() { return win_.rebuild(); }
    void drawTooSmall(bool started)
    {
        werase(stdscr);
        mvprintw(0, 0, "Terminal terlalu kecil");
        mvprintw(1, 0, "Butuh %dx%d",
                 needCols(), needRows());
        mvprintw(2, 0, "Ada   %dx%d", COLS, LINES);
        if (started)
        {
            mvprintw(3, 0, "Permainan dijeda");
        }
        wnoutrefresh(stdscr);
        doupdate();
    }
    void drawGame(const Snake (&snakes)[PlayerCount], const FoodField &food, bool paused, bool fullClear)
    {
        const Outcome outcome = outcomeOf(snakes);
        if (fullClear)
        {
            werase(stdscr);
            clearok(stdscr, TRUE);
            wnoutrefresh(stdscr);
        }
        werase(win_.hud());
        werase(win_.frame());
        werase(win_.field());
        box(win_.frame(), 0, 0);
        int attrs[PlayerCount];
        for (int i = 0; i < PlayerCount; ++i)
        {
            attrs[i] = static_cast<int>(colors_.attr(
                detail::snakeTone(i, snakes[i].pending() > 0)));
        }
        menu::drawStatus(win_.hud(), snakes, attrs);
        menu::drawCommands(win_.hud());

        for (int i = 0; i < food.count(); ++i)
        {
            const Food &f = food.get(i);
            const attr_t at = colors_.attr(detail::foodTone(f.value)) | A_BOLD;
            wattron(win_.field(), at);
            mvwprintw(win_.field(), f.pos.y, cfg::CellWidth * f.pos.x, "*%d", f.value);
            wattroff(win_.field(), at);
        }
        for (int i = 0; i < PlayerCount; ++i)
        {
            if (snakes[i].isDead())
            {
                drawSnake(win_.field(), snakes[i], static_cast<attr_t>(attrs[i]));
            }
        }
        for (int i = 0; i < PlayerCount; ++i)
        {
            if (!snakes[i].isDead())
            {
                drawSnake(win_.field(), snakes[i], static_cast<attr_t>(attrs[i]));
            }
        }
        if (outcome != Outcome::None)
        {
            menu::drawResult(win_.frame(), snakes, outcome);
        }
        else if (paused)
        {
            menu::drawPaused(win_.frame());
        }
        touchwin(win_.hud());
        touchwin(win_.frame());
        wnoutrefresh(win_.hud());
        wnoutrefresh(win_.frame());
        wnoutrefresh(win_.field());
        doupdate();
    }
};