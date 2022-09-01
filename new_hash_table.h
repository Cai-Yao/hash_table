#pragma once 
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>
#include "String.h"
#include "xxhash32.h"

struct RowRef {
    using SizeT = uint32_t;
    SizeT row_num = 0;
    uint8_t block_offset;

    RowRef() {}
    RowRef(size_t row_num_count, uint8_t block_offset_)
            : row_num(row_num_count), block_offset(block_offset_) {}
};

struct RowRefList : RowRef {
    struct Batch {
        static constexpr size_t MAX_SIZE = 7;

        SizeT size = 0; 
        Batch* next;
        RowRef row_refs[MAX_SIZE];

        Batch(Batch* parent) : next(parent) {}

        bool full() const { return size == MAX_SIZE; }

        Batch* insert(RowRef&& row_ref) {
            if (full()) {
                auto batch = new Batch(this);
                batch->insert(std::move(row_ref));
                return batch;
            }

            row_refs[size++] = std::move(row_ref);
            return this;
        }
    };

    RowRefList() {}
    RowRefList(size_t row_num_, uint8_t block_offset_) : RowRef(row_num_, block_offset_) {}

    void insert(RowRef&& row_ref) {
        row_count++;

        if (!next) {
            next = new Batch (nullptr);
        }
        next = next->insert(std::move(row_ref));
    }

    uint32_t get_row_count() { return row_count; }

private:
    Batch* next = nullptr;
    uint32_t row_count = 1;
};



class HashTable {
public:
    using key_t = String;
    using Cell = std::pair<key_t, RowRefList>;
    HashTable(uint32_t size) {
        m_size = 0;
        degree = size;
        buf = new Cell[buf_size()];

        first = new uint32_t[bucket_size()];
        next = new uint32_t[buf_size() + 1];
    }
    ~HashTable() {
        m_size = 0;
        if (buf) {
            delete buf;
            buf = nullptr;
        }
        if (first) {
            delete first;
            first = nullptr;
        }
        if (next) {
            delete next;
            next = nullptr;
        }
    }
    void insert(const key_t &key, RowRef && value) {
        auto place_value = find(key);
        //printf("debug: insert -> find place_value %u, m_size %u\n", place_value, m_size);
        if (place_value) {
            auto block = &buf[place_value - 1];
            block->second.insert(std::move(value));
            return;
        }
        new (&buf[m_size]) Cell(key, RowRefList(value.row_num, value.block_offset));
        ++m_size;
        auto hash_value = hash(key);
        auto bucket_value = hash_value & mask();
        
        next[m_size] = first[bucket_value];
        first[bucket_value] = m_size;

        if (is_full()) {
            resize();
        }
    }

    /// insert with block
    void m_insert(key_t* keys, RowRef* values, unsigned int block_size) {
        std::vector<uint32_t> index_list;
        for (auto i = 0; i < block_size; ++i) {
            auto place_value = find(keys[i]);
            if (place_value) {
                auto block = &buf[place_value - 1];
                block->second.insert(std::move(values[i]));
            } else {
                index_list.emplace_back(i);
            }
        }
        for (auto i : index_list) {
            new (&buf[m_size]) Cell(keys[i], RowRefList(values[i].row_num, values[i].block_offset));
            ++m_size;
            if (is_full()) {
                resize();
                continue;
            }
            auto hash_value = hash(keys[i]);
            auto bucket_value = hash_value & mask();

            next[m_size] = first[bucket_value];
            first[bucket_value] = m_size;
        }
    }
    uint32_t find(key_t key) {
        auto hash_value = hash(key);
        auto bucket_value = hash_value & mask();
        auto place_value = first[bucket_value];
        while (place_value && buf[place_value - 1].first != key) {
            ++collision_num;
            place_value = next[place_value];       
        }
        return place_value;
    }

    /// find with block
    uint32_t* m_find(key_t* keys, uint32_t block_size) {
        auto* res = new uint32_t[block_size];
        std::vector<std::tuple<uint32_t, uint32_t>> place_values;
        std::vector<std::tuple<uint32_t, uint32_t>> place_values_new;
        for (auto i = 0; i < block_size; i++) {
            auto hash_value = hash(keys[i]);
            auto bucket_value = hash_value & mask();
            auto place_value = first[bucket_value];
            place_values.emplace_back(i, place_value);
        }
        while (!place_values.empty()) {
            place_values_new.clear();
            for (auto it : place_values) {
                auto place_value = std::get<1>(it);
                auto index = std::get<0>(it);
                if (place_value && buf[place_value - 1].first != keys[index]) {
                    place_values_new.emplace_back(index, next[place_value]);
                } else {
                    res[index] = place_value;
                }
            }
            swap(place_values, place_values_new);
        }
        return res;
    }

    RowRefList* get(uint32_t pos) {
        return &buf[pos].second;
    }

    void resize() {
        degree = degree + (degree > 23 ? 1 : 2);
        auto temp_buf = new Cell[buf_size()];
        auto temp_first = new uint32_t[bucket_size()];
        auto temp_next = new uint32_t[buf_size() + 1];
        std::memcpy(temp_buf, buf, sizeof(Cell) * m_size);
        delete buf;
        delete first;
        delete next;
        buf = temp_buf;
        first = temp_first;
        next = temp_next;
        for (auto i = 0; i < m_size; i++) {
            auto hash_value = hash(buf[i].first);
            auto bucket_value = hash_value & mask();
            next[i + 1] = first[bucket_value];
            first[bucket_value] = i + 1;
        }
    }
    uint32_t next_num() const { return collision_num; }
    uint32_t hash(const key_t &key) const {
        
        return XXHash32::hash(key.data(), 64, 0);
    }
    uint32_t buf_size() {
        return (1 << degree);
    }
    uint32_t bucket_size() {
        return (1 << (degree));
    }
    uint32_t mask() {
        return bucket_size() - 1;
    }
    bool is_full() {
        return m_size >= buf_size();
    }
private:
    uint32_t degree;
    uint32_t m_size;
    Cell* buf;
    uint32_t* first;
    uint32_t* next;
    uint32_t collision_num{0};
};