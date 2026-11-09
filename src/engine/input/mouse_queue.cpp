#include "engine/input/mouse_queue.hpp"

namespace crusader {

// @orig 1328:0935
MouseInput::Kind MouseInput::kind() const {
    switch (code & 0x7F) {
    case kMove: return kMotionKind;
    case kLeftDown: case kLeftUp: return kLeft;
    case kRightDown: case kRightUp: return kRight;
    default: return kBoth;  // 0x2A, 0x54
    }
}

// @orig 1330:0652
void MouseQueue::send(const MouseInput& e) {
    if (queue_.size() < kCapacity) queue_.push_back(e);
}

// @orig 1360:045D; motion branch
void MouseQueue::move(int x, int y, std::uint16_t buttons) {
    motion_ = MouseInput{MouseInput::kMotion, MouseInput::kMove, buttons, x, y};
}

// @orig 1360:045D; press branches
void MouseQueue::press(bool right, int x, int y, std::uint16_t buttons) {
    const int b = right ? 1 : 0;
    MouseInput e{MouseInput::kPress, right ? MouseInput::kRightDown : MouseInput::kLeftDown, buttons, x, y};
    if (held_[b]) {
        // A second press within the delay: a double click, sent at once.
        e.type = MouseInput::kDoubleClick;
        send(e);
        held_[b].reset();
        return;
    }
    if (held_[1 - b]) {
        // The other button is held back: one press of both.
        e.code = MouseInput::kBothDown;
        send(e);
        held_[0].reset();
        held_[1].reset();
        return;
    }
    held_[b] = Held{e, 0, std::nullopt};
}

// @orig 1360:045D; release branches
void MouseQueue::release(bool right, int x, int y, std::uint16_t buttons) {
    const int b = right ? 1 : 0;
    const MouseInput e{MouseInput::kRelease, right ? MouseInput::kRightUp : MouseInput::kLeftUp, buttons, x, y};
    if (held_[b]) held_[b]->release = e;  // sent after its press
    else send(e);
}

// @orig 1360:045D; timer branch
void MouseQueue::tick() {
    for (auto& held : held_) {
        if (!held || ++held->ticks != kClickDelay) continue;
        send(held->press);
        if (held->release) send(*held->release);
        held.reset();
    }
}

// @orig 1360:0E1F
std::optional<MouseInput> MouseQueue::next() {
    if (!queue_.empty()) {
        const MouseInput e = queue_.front();
        queue_.pop_front();
        return e;
    }
    std::optional<MouseInput> e = motion_;
    motion_.reset();
    return e;
}

// @orig none: the port routes menu clicks directly (see Game::mouse_button)
void MouseQueue::clear() {
    held_[0].reset();
    held_[1].reset();
    queue_.clear();
    motion_.reset();
}

}  // namespace crusader
