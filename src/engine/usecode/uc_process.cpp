#include "engine/usecode/uc_process.hpp"

#include "engine/usecode/uc_machine.hpp"

namespace crusader {

// @orig ?: UCProcess of UPROCESS.C (segment 1420), not split yet
UCProcess::UCProcess(UCMachine& machine, std::uint16_t cid, std::uint16_t offset, std::uint32_t this_ptr,
                     std::size_t this_size, std::span<const std::uint8_t> args)
    : machine_(machine) {
    // Copy of the object the function runs on, then the arguments, then the
    // pointer to that copy (its pid part is patched in on_added()).
    if (this_ptr != 0 && this_size > 0) {
        stack_.push_zero(this_size);
        this_copy_ = stack_.sp();
        machine_.dereference(this_ptr, stack_.bytes(this_copy_, this_size));
    }
    stack_.push(args);
    if (this_copy_) {
        stack_.push4(0);
        this_slot_ = stack_.sp();
    }
    call(cid, offset);
}

void UCProcess::on_added() {
    if (this_copy_) {
        stack_.write(this_slot_, UCMachine::stack_ptr(pid(), this_copy_), 4);
    }
}

// @orig ?: UCProcess of UPROCESS.C (segment 1420), not split yet
void UCProcess::call(std::uint16_t cid, std::uint16_t offset) {
    stack_.push2(class_id_);
    stack_.push2(ip_);
    stack_.push2(bp_);
    class_id_ = cid;
    ip_ = offset;
    bp_ = stack_.sp();
}

// @orig ?: UCProcess of UPROCESS.C (segment 1420), not split yet
bool UCProcess::ret() {
    stack_.set_sp(bp_);
    bp_ = stack_.pop2();
    ip_ = stack_.pop2();
    class_id_ = stack_.pop2();
    return ip_ == 0xFFFF && class_id_ == 0xFFFF;
}

void UCProcess::run() { machine_.execute(*this); }

// @orig ?: UCProcess of UPROCESS.C (segment 1420), not split yet
void UCProcess::on_terminate() {
    UCHeap& heap = machine_.heap();
    for (const auto& [handle, kind] : free_on_terminate_) {
        switch (kind) {
            case 1: heap.free_string(handle); break;
            case 2: heap.free_string_list(handle); break;
            case 3: heap.free_list(handle); break;
            default: break;
        }
    }
    free_on_terminate_.clear();
}

}  // namespace crusader
