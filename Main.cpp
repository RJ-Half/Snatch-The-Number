// Snatch The Number : permainan Snake dua pemain di terminal.
// Build: make   |   g++ -std=c++17 -Wall -Wextra -O2 -I. Main.cpp -o number_snake -lncurses
//
// Main.cpp ini disesuaikan dengan header tim (Object/, Mechanics/, Platform/).
// Catatan antarmuka yang dipakai:
//  - Renderer sudah memiliki Terminal dan ArenaWindow, jadi App tidak membuatnya lagi.
//  - Outcome::None = permainan berjalan; DeathCause: SelfBite, HeadToHead, Wiped.
//  - snatch::Steal tidak punya flag aktif: aktif bila index >= 0 (lihat active()).

#include "Platform/Arena.hpp"
#include "Object/Snake.hpp"
#include "Object/Food.hpp"
#include "Mechanics/Collision.hpp"
#include "Mechanics/Growth.hpp"

// PENYESUAIAN untuk Snatch.hpp: snatch::targetIndex() memanggil
//   collision::isSolid(foe, index, foeGrows)   // urutan: (Snake, int, bool)
// padahal Collision.hpp hanya punya isSolid(Vec2 cell, const Snake&, bool). Tanpa
// overload ini Snatch.hpp tidak bisa dikompilasi. Overload ini harus dideklarasikan
// SEBELUM Snatch.hpp di-include. Aturannya sama dengan isSolid versi Vec2: segmen
// di `index` dianggap padat, kecuali ekor yang benar-benar pergi (tidak tumbuh).
// Bila Snatch.hpp baris 21 sudah diperbaiki menjadi isSolid(cell, foe, foeGrows),
// blok ini boleh dihapus.
 // namespace collision

#include "Mechanics/Snatch.hpp"
#include "Mechanics/Controls.hpp"
#include "Platform/Menu.hpp"
#include "Mechanics/Render.hpp"

using namespace std;  // tidak berpengaruh; hapus bila kompilator mengeluh

struct MovePlan {
    Vec2 next;
    bool grow = false;
    DeathCause death = DeathCause::None;
    snatch::Steal steal;  // index -1 = tidak mencuri
};

struct TickPlan {
    MovePlan move[PlayerCount];
};

// State pertandingan dan urutan aturan tiap tick (dua fase: plan lalu apply).
class Match {
public:
    using SnakeArray = Snake[PlayerCount];

    explicit Match(unsigned int seed) { rng_.seed(seed); }

    void start(const Arena& arena) {
        arena_ = arena;
        const int w = arena_.width;
        const int h = arena_.height;
        snakes_[0].reset(Vec2{w / 4, h / 3}, Direction::Right);
        snakes_[1].reset(Vec2{w - 1 - w / 4, h - 1 - h / 3}, Direction::Left);
        food_.start(arena_, snakes_, rng_);
    }

    void tick() {
        if (isOver()) return;
        resolve();
        const TickPlan p = plan();
        apply(p);
        if (!isOver()) food_.update(arena_, snakes_, rng_);
    }

    bool isOver() const { return outcomeOf(snakes_) != Outcome::None; }
    Outcome outcome() const { return outcomeOf(snakes_); }
    const SnakeArray& snakes() const { return snakes_; }
    Snake& snake(int p) { return snakes_[p]; }
    const FoodField& food() const { return food_; }
    const Arena& arena() const { return arena_; }

private:
    // Arah antre menjadi arah aktif.
    void resolve() {
        for (int p = 0; p < PlayerCount; ++p) snakes_[p].commitTurn();
    }

    // Hanya membaca state: semua keputusan dari kondisi awal tick.
    TickPlan plan() const {
        TickPlan tp;
        Vec2 head[PlayerCount];
        for (int p = 0; p < PlayerCount; ++p) {
            head[p] = snakes_[p].head();
            tp.move[p].grow = growth::growsThisTick(snakes_[p]);
            tp.move[p].next = stepFrom(head[p], snakes_[p].heading());
        }
        const bool same = collision::sameCell(tp.move[0].next, tp.move[1].next);
        const bool swap = collision::swapped(tp.move[0].next, tp.move[1].next, head[0], head[1]);

        if (!swap)
            for (int p = 0; p < PlayerCount; ++p) judgeCell(p, tp.move[p]);

        if (same || swap) {
            for (int p = 0; p < PlayerCount; ++p) {
                if (collision::losesClash(snakes_[p].headValue(),
                                          snakes_[otherPlayer(p)].headValue()) &&
                    tp.move[p].death == DeathCause::None)
                    tp.move[p].death = DeathCause::HeadToHead;
            }
        }

        // Mengenai kepala lawan = seluruh tubuh lawan dicuri.
        for (int p = 0; p < PlayerCount; ++p) {
            const MovePlan& mv = tp.move[p];
            const int o = otherPlayer(p);
            if (mv.death == DeathCause::None && mv.steal.active() && mv.steal.wipesVictim() &&
                tp.move[o].death == DeathCause::None)
                tp.move[o].death = DeathCause::Wiped;
        }

        // Serangan pemain yang kalah dibatalkan.
        for (int p = 0; p < PlayerCount; ++p)
            if (tp.move[p].death != DeathCause::None) tp.move[p].steal = snatch::Steal{};
        return tp;
    }

    void judgeCell(int p, MovePlan& mv) const {
        const Snake& self = snakes_[p];
        const Snake& other = snakes_[otherPlayer(p)];
        if (collision::hitsWall(mv.next, arena_)) {
            mv.death = DeathCause::Wall;
            return;
        }
        if (collision::hitsSelf(mv.next, self, mv.grow)) {
            mv.death = DeathCause::SelfBite;
            return;
        }
        const int idx = snatch::targetIndex(other, mv.next, growth::growsThisTick(other));
        if (idx < 0) return;
        if (snatch::succeeds(self, other, idx)) mv.steal = snatch::aim(other, idx);
        else mv.death = DeathCause::FailedAttack;
    }

    // Satu-satunya yang mengubah state.
    void apply(const TickPlan& tp) {
        for (int p = 0; p < PlayerCount; ++p)
            if (tp.move[p].death != DeathCause::None) snakes_[p].setCause(tp.move[p].death);
        for (int p = 0; p < PlayerCount; ++p)
            if (tp.move[p].steal.active()) snatch::cut(snakes_[otherPlayer(p)], tp.move[p].steal);
        for (int p = 0; p < PlayerCount; ++p) snatch::reward(snakes_[p], tp.move[p].steal);
        for (int p = 0; p < PlayerCount; ++p)
            if (!snakes_[p].isDead()) growth::advance(snakes_[p], tp.move[p].next, tp.move[p].grow);
        for (int p = 0; p < PlayerCount; ++p) {
            if (snakes_[p].isDead()) continue;
            const int v = food_.take(tp.move[p].next);
            if (v > 0) growth::addCredit(snakes_[p], v);
        }
    }

    Arena arena_;
    Snake snakes_[PlayerCount];
    FoodField food_;
    Random rng_;
};

// Loop utama dan status program.
class App {
public:
    App() : match_(processSeed()) { syncSize(); }

    int run() {
        if (!renderer_.ok()) return 1;
        while (running_) {
            controls::poll(queue_);
            processQueue();
            if (LINES != lastLines_ || COLS != lastCols_) onResize();
            if (!started_) tryStart();
            if (timer_.ready() && canSimulate()) {
                match_.tick();
                dirty_ = true;
            }
            if (dirty_) draw();
            timer_.sleepSlice();
        }
        return 0;
    }

private:
    bool canSimulate() const { return started_ && !paused_ && renderer_.fits() && !match_.isOver(); }

    void syncSize() {
        lastLines_ = LINES;
        lastCols_ = COLS;
    }

    void startMatch() {
        paused_ = false;
        started_ = false;
        Arena arena;
        if (renderer_.open(arena)) {
            match_.start(arena);
            started_ = true;
        }
        timer_.reset();
        syncSize();
        fullClear_ = true;
        dirty_ = true;
    }

    void tryStart() {
        if (COLS >= cfg::MinTermCols && LINES >= cfg::HudRows + cfg::MinArenaH + cfg::FrameBorder)
            startMatch();
    }

    void onResize() {
        syncSize();
        fullClear_ = true;
        dirty_ = true;
        if (started_) renderer_.rebuild();  // rebuild() sendiri menolak bila terminal masih kecil
    }

    void handle(const InputEvent& ev) {
        switch (ev.command) {
            case Command::Quit: running_ = false; break;
            case Command::Restart: startMatch(); break;
            case Command::Pause:
                if (started_ && !match_.isOver()) {
                    paused_ = !paused_;
                    dirty_ = true;
                }
                break;
            case Command::Turn:
                if (canSimulate()) controls::steer(match_.snake(ev.player), ev.direction);
                break;
            case Command::Resize: onResize(); break;
            case Command::None: break;
        }
    }

    void processQueue() {
        for (int i = 0; i < queue_.size(); ++i) handle(queue_.at(i));
        queue_.clear();
    }

    void draw() {
        if (started_ && renderer_.fits()) {
            renderer_.drawGame(match_.snakes(), match_.food(), paused_, fullClear_);
            fullClear_ = false;
        } else {
            renderer_.drawTooSmall(started_);
            fullClear_ = true;
        }
        dirty_ = false;
    }

    Renderer renderer_;  // harus pertama: memiliki Terminal (initscr lebih dulu, endwin terakhir)
    Match match_;
    InputQueue queue_;
    TickTimer timer_;
    bool running_ = true;
    bool started_ = false;
    bool paused_ = false;
    bool dirty_ = true;
    bool fullClear_ = true;
    int lastLines_ = 0;
    int lastCols_ = 0;
};

int main() {
    App app;
    return app.run();
}
