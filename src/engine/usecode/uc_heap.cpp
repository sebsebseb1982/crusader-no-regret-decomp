#include "engine/usecode/uc_heap.hpp"

#include <algorithm>
#include <stdexcept>

namespace crusader {

// @orig ?: list helpers of INTER.C (segment 1418), not split yet
bool UCList::contains(std::span<const std::uint8_t> element) const {
    for (std::size_t i = 0; i < size(); ++i) {
        if (std::equal(element.begin(), element.end(), at(i).begin())) {
            return true;
        }
    }
    return false;
}

// @orig ?: list helpers of INTER.C (segment 1418), not split yet
void UCList::remove(std::span<const std::uint8_t> element) {
    for (std::size_t i = 0; i < size();) {
        if (std::equal(element.begin(), element.end(), at(i).begin())) {
            data.erase(data.begin() + static_cast<std::ptrdiff_t>(i * element_size),
                       data.begin() + static_cast<std::ptrdiff_t>((i + 1) * element_size));
        } else {
            ++i;
        }
    }
}

// @orig ?: string / list heap of INTER.C (segment 1418), not split yet
template <class Map>
std::uint16_t UCHeap::next_id(const Map& map, std::uint16_t& cursor) {
    for (std::uint32_t tries = 0; tries < 0xFFFF; ++tries) {
        const std::uint16_t id = cursor;
        cursor = static_cast<std::uint16_t>(cursor == 0xFFFF ? 1 : cursor + 1);
        if (!map.contains(id)) {
            return id;
        }
    }
    throw std::runtime_error("usecode heap exhausted");
}

std::uint16_t UCHeap::new_string(std::string s) {
    const auto id = next_id(strings_, next_string_);
    strings_.emplace(id, std::move(s));
    return id;
}

const std::string* UCHeap::string(std::uint16_t id) const {
    auto it = strings_.find(id);
    return it == strings_.end() ? nullptr : &it->second;
}

std::uint16_t UCHeap::duplicate_string(std::uint16_t id) {
    const std::string* s = string(id);
    return s ? new_string(*s) : 0;
}

std::uint16_t UCHeap::new_list(UCList list) {
    const auto id = next_id(lists_, next_list_);
    lists_.emplace(id, std::move(list));
    return id;
}

UCList* UCHeap::list(std::uint16_t id) {
    auto it = lists_.find(id);
    return it == lists_.end() ? nullptr : &it->second;
}

std::uint16_t UCHeap::duplicate_list(std::uint16_t id) {
    const UCList* l = list(id);
    return l ? new_list(*l) : 0;
}

// @orig ?: string / list heap of INTER.C (segment 1418), not split yet
std::uint16_t UCHeap::duplicate_string_list(std::uint16_t id) {
    const UCList* src = list(id);
    if (!src) {
        return 0;
    }
    UCList copy{2, {}};
    for (std::size_t i = 0; i < src->size(); ++i) {
        const std::uint16_t s = duplicate_string(src->at2(i));
        const std::uint8_t bytes[2] = {static_cast<std::uint8_t>(s), static_cast<std::uint8_t>(s >> 8)};
        copy.append(bytes);
    }
    return new_list(std::move(copy));
}

// @orig ?: string / list heap of INTER.C (segment 1418), not split yet
void UCHeap::free_string_list(std::uint16_t id) {
    if (const UCList* l = list(id)) {
        for (std::size_t i = 0; i < l->size(); ++i) {
            free_string(l->at2(i));
        }
    }
    free_list(id);
}

}  // namespace crusader
