#pragma once
// Strings and lists referenced from usecode by 16-bit handles (0 = none).
// A list stores fixed-size elements; a "string list" (slist) stores 2-byte
// string handles and owns those strings.

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace crusader {

struct UCList {
    std::size_t element_size = 2;
    std::vector<std::uint8_t> data;

    std::size_t size() const { return element_size ? data.size() / element_size : 0; }
    std::span<const std::uint8_t> at(std::size_t i) const {
        return std::span(data).subspan(i * element_size, element_size);
    }
    std::uint16_t at2(std::size_t i) const {
        auto e = at(i);
        return static_cast<std::uint16_t>(e[0] | e[1] << 8);
    }
    void append(std::span<const std::uint8_t> element) { data.insert(data.end(), element.begin(), element.end()); }
    bool contains(std::span<const std::uint8_t> element) const;
    void remove(std::span<const std::uint8_t> element);
};

class UCHeap {
public:
    std::uint16_t new_string(std::string s);
    const std::string* string(std::uint16_t id) const;
    std::uint16_t duplicate_string(std::uint16_t id);
    void free_string(std::uint16_t id) { strings_.erase(id); }

    std::uint16_t new_list(UCList list);
    UCList* list(std::uint16_t id);
    std::uint16_t duplicate_list(std::uint16_t id);
    // Copies an slist and the strings it references.
    std::uint16_t duplicate_string_list(std::uint16_t id);
    void free_list(std::uint16_t id) { lists_.erase(id); }
    void free_string_list(std::uint16_t id);

    std::size_t string_count() const { return strings_.size(); }
    std::size_t list_count() const { return lists_.size(); }

private:
    template <class Map>
    static std::uint16_t next_id(const Map& map, std::uint16_t& cursor);

    std::unordered_map<std::uint16_t, std::string> strings_;
    std::unordered_map<std::uint16_t, UCList> lists_;
    std::uint16_t next_string_ = 1;
    std::uint16_t next_list_ = 1;
};

}  // namespace crusader
