#pragma once
// A running usecode function (UPROCESS.C): a kernel process that executes
// bytecode on its own stack.
//
// Call frame (stack grows down), bp points at the saved bp:
//   bp+6..  arguments, then the `this` pointer (pushed last by the caller)
//   bp+4    caller class id      bp+2  caller ip      bp+0  caller bp
//   bp-n    locals (reserved by the `init` opcode)
// A frame whose saved class/ip are 0xFFFF is the process entry: returning
// from it terminates the process with its result register.

#include "engine/process.hpp"
#include "engine/usecode/uc_stack.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace crusader {

class UCMachine;

class UCProcess : public Process {
public:
    // Starts function `offset` of class `cid`. `this_ptr`/`this_size`: the
    // object the function runs on; its bytes are copied onto the new stack and
    // the pointer passed to the function points at that copy.
    UCProcess(UCMachine& machine, std::uint16_t cid, std::uint16_t offset, std::uint32_t this_ptr,
              std::size_t this_size, std::span<const std::uint8_t> args);

    std::uint16_t class_id() const { return class_id_; }
    std::uint16_t ip() const { return ip_; }
    UCStack& stack() { return stack_; }
    // The result of the last call, for an intrinsic whose result comes once
    // the process resumes (a modal gump: Gump_run_modal's return value).
    void set_call_result(std::uint32_t value) { temp_ = value; }

protected:
    void run() override;
    void on_terminate() override;
    void on_added() override;

private:
    friend class UCMachine;

    void call(std::uint16_t cid, std::uint16_t offset);
    // Returns true when returning from the entry frame.
    bool ret();

    UCMachine& machine_;
    UCStack stack_;
    std::uint16_t class_id_ = 0xFFFF;
    std::uint16_t ip_ = 0xFFFF;
    std::uint16_t bp_ = 0;
    std::uint16_t this_copy_ = 0;  // stack offset of the copied object, 0 if none
    std::uint16_t this_slot_ = 0;  // stack offset of the `this` pointer
    std::uint32_t temp_ = 0;             // return value of calls/intrinsics, spawned pid
    std::vector<std::pair<std::uint16_t, std::uint8_t>> free_on_terminate_;  // (handle, 1 string/2 slist/3 list)
};

}  // namespace crusader
