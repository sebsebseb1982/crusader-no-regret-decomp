// Kernel scheduling semantics (no game data needed).

#include "engine/input/keyboard.hpp"
#include "engine/kernel.hpp"

#include <cstdio>
#include <functional>
#include <vector>

using namespace crusader;

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                \
        }                                                              \
    } while (0)

// Counts its runs; optional body for scripted behaviour.
class Counter : public Process {
public:
    int runs = 0;
    std::function<void(Counter&)> body;
    using Process::kernel;

protected:
    void run() override {
        ++runs;
        if (body) body(*this);
    }
};

class Ticker : public TimedProcess {
public:
    int ticks = 0;
    explicit Ticker(std::uint32_t hz) { set_rate(hz); }

protected:
    void tick() override { ++ticks; }
};

constexpr std::uint32_t kFrameUs = 1'000'000 / 60;

void test_pids() {
    Kernel k;
    auto& a = k.spawn<Counter>();
    auto& b = k.spawn<Counter>();
    CHECK(a.pid() == 1 && b.pid() == 2);
    a.terminate();
    k.run_slice();
    CHECK(k.get(1) == nullptr);
    auto& c = k.spawn<Counter>();
    CHECK(c.pid() == 1);  // lowest free slot reused
    CHECK(k.process_count() == 2);
}

void test_sleep() {
    Kernel k;
    auto& p = k.spawn<Counter>();
    p.sleep(30);  // half a second
    for (int i = 0; i < 29; ++i) {
        k.advance_time(kFrameUs);
        k.run_slice();
    }
    CHECK(p.runs == 0);
    k.advance_time(kFrameUs);
    k.advance_time(kFrameUs);  // rounding margin
    k.run_slice();
    CHECK(p.runs == 1);
}

void test_wait_and_result() {
    Kernel k;
    auto& worker = k.spawn<Counter>();
    auto& other = k.spawn<Counter>();
    auto& waiter = k.spawn<Counter>();
    waiter.wait_for(worker.pid());
    waiter.wait_for(other.pid());  // AND
    k.run_slice();
    CHECK(waiter.runs == 0);
    worker.terminate(7);
    k.run_slice();
    CHECK(waiter.runs == 0);  // still waiting for `other`
    other.terminate(42);
    k.run_slice();
    CHECK(waiter.runs == 1);
    CHECK(waiter.wake_result() == 42);
}

void test_wait_any() {
    Kernel k;
    auto& a = k.spawn<Counter>();
    auto& b = k.spawn<Counter>();
    auto& waiter = k.spawn<Counter>();
    waiter.wait_for_any({a.pid(), b.pid()});
    k.run_slice();
    CHECK(waiter.runs == 0);
    b.terminate(3);
    k.run_slice();
    CHECK(waiter.runs == 1 && waiter.wake_result() == 3);
    // Terminating the other one later must not disturb the waiter.
    a.terminate();
    k.run_slice();
    CHECK(waiter.runs == 2);
}

void test_wait_on_finished_process() {
    Kernel k;
    auto& p = k.spawn<Counter>();
    p.wait_for(999);  // no such process: no wait
    k.run_slice();
    CHECK(p.runs == 1);
}

void test_waiter_terminated_first() {
    Kernel k;
    auto& target = k.spawn<Counter>();
    auto& waiter = k.spawn<Counter>();
    waiter.wait_for(target.pid());
    waiter.terminate();
    k.run_slice();
    auto& reused = k.spawn<Counter>();  // takes the waiter's old pid
    CHECK(reused.pid() == 2);
    target.terminate();
    k.run_slice();
    CHECK(reused.runs == 1);  // not blocked: never treated as a waiter
    CHECK(!reused.waiting());
}

void test_levels() {
    Kernel k;
    auto& world = k.spawn<Counter>();
    auto& clock = k.spawn<Counter>();
    clock.set_runs_at_any_level(true);
    auto& menu = k.spawn<Counter>();
    k.raise_level(menu);
    CHECK(k.level() == 2);
    k.run_slice();
    CHECK(world.runs == 0 && clock.runs == 1 && menu.runs == 1);
    menu.terminate();
    k.run_slice();
    CHECK(k.level() == 1);
    k.run_slice();
    CHECK(world.runs == 1);
}

void test_spawn_during_slice() {
    Kernel k;
    Counter* child = nullptr;
    auto& parent = k.spawn<Counter>();
    parent.body = [&](Counter& self) {
        if (!child) {
            child = &self.kernel().spawn<Counter>();
        }
    };
    k.run_slice();
    CHECK(child && child->runs == 1);  // higher pid: runs in the same pass
}

void test_timed() {
    Kernel k;
    auto& t = k.spawn<Ticker>(10);
    for (int i = 0; i < 60; ++i) {
        k.advance_time(kFrameUs);
        k.run_slice();
    }
    CHECK(t.ticks >= 9 && t.ticks <= 10);
    CHECK(k.ticks() >= 59 && k.ticks() <= 60);
}

void test_count_processes() {
    Kernel k;
    auto& a = k.spawn<Counter>();
    a.set_owner(5);
    a.set_type(0x100);
    auto& b = k.spawn<Counter>();
    b.set_owner(5);
    b.set_type(0x200);
    CHECK(k.count_processes(5, 0x100) == 1);
    CHECK(k.count_processes(5, Kernel::kAnyType) == 2);
    CHECK(k.count_processes(6, Kernel::kAnyType) == 0);
}

void test_fail_processes() {
    Kernel k;
    auto& a = k.spawn<Counter>();
    a.set_owner(5);
    a.set_type(0x100);
    auto& b = k.spawn<Counter>();
    b.set_owner(5);
    b.set_type(0x200);
    auto& other = k.spawn<Counter>();
    other.set_owner(6);
    other.set_type(0x100);
    auto& system = k.spawn<Counter>();
    system.set_owner(5);
    system.set_type(0x100);
    system.set_runs_at_any_level(true);

    CHECK(k.fail_processes(5, 0x100, 1) == 1);
    CHECK(a.terminated() && a.failed());
    CHECK(!b.terminated() && !other.terminated() && !system.terminated());
    CHECK(k.fail_processes(0, 0x100, 2) == 0);  // wrong level
    CHECK(k.fail_processes(0, Kernel::kAnyType, 1) == 2);
    CHECK(b.terminated() && other.terminated() && !system.terminated());
}

void test_keyboard_repeat() {
    // KERNEL.C 3574 / KDAEMON.C: a press, ignored typematic repeats, then the
    // daemon's repeats once the key has been held for 10 ticks.
    Keyboard kb;
    std::vector<GameKeyEvent> got;
    kb.set_handler([&](const GameKeyEvent& e) { got.push_back(e); });
    kb.key(0x48, true, true, 0, 0, 100);
    kb.key(0x48, true, true, 0, 0, 101);  // typematic: ignored
    CHECK(got.size() == 1 && got[0].pressed && got[0].count == 1 && got[0].extended);
    kb.repeat(105);
    CHECK(got.size() == 1);
    kb.repeat(110);
    kb.repeat(111);
    CHECK(got.size() == 3 && got[1].count == 2 && got[2].count == 3 && got[2].time == 111);
    CHECK(kb.down(0x48, true) == 3 && kb.down(0x48, false) == 0);
    kb.key(0x48, true, false, 0, 0, 120);
    CHECK(got.size() == 4 && !got[3].pressed && got[3].count == 0 && kb.down(0x48, true) == 0);
    kb.repeat(200);
    CHECK(got.size() == 4);
}

}  // namespace

int main() {
    test_pids();
    test_sleep();
    test_wait_and_result();
    test_wait_any();
    test_wait_on_finished_process();
    test_waiter_terminated_first();
    test_levels();
    test_spawn_during_slice();
    test_timed();
    test_count_processes();
    test_fail_processes();
    test_keyboard_repeat();
    std::printf(failures ? "%d failure(s)\n" : "all kernel checks passed\n", failures);
    return failures ? 1 : 0;
}
