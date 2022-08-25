#pragma once 
#include <algorithm>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <functional>
#include <memory.h>
#include <assert.h>
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
    HashTable(uint32_t degree_size) {
        m_size = 0;
        degree = degree_size;
        buf = new Cell[buf_size()];
    }
    ~HashTable() {
        m_size = 0;
        if (buf) {
            delete buf;
            buf = nullptr;
        }
    }
    void insert(key_t key, RowRef && value) {
        {      
            auto place_value = find(key);
            if (place_value != -1) {
                auto block = &buf[place_value];
                block->second.insert(std::move(value));
                return;
            }
        }
        auto hash_value = hash(key);

        auto [is_find, place_value] = find_cell(key, hash_value, place(hash_value));
        auto it = &buf[place_value];
        assert(is_find == false);
        new (&buf[place_value]) Cell(key, RowRefList(value.row_num, value.block_offset));
        ++m_size;
        if (is_full()) {
            resize();
        }
    }
    
    std::pair<bool, uint32_t> find_cell(key_t key, uint32_t h, uint32_t place_value) {
        if (is_zero(place_value)) return {false, place_value};
        while (buf[place_value].first != key) {
            place_value = next(place_value);
            if (is_zero(place_value)) return {false, place_value};
        }

        return {true, place_value};
    }
    uint32_t find(key_t key) {
        auto hash_value = hash(key);
        auto [is_find, place_value] = find_cell(key, hash_value, place(hash_value));
        if (is_find) return place_value;
        return -1;
    }

    RowRefList* get(uint32_t pos) {
        return &buf[pos].second;
    }

    void resize() {
        auto old_size = buf_size();
        auto old_buf = buf;
        degree = degree + (degree > 23 ? 1 : 2);
        auto new_buf = new Cell[buf_size()];
        buf = new_buf;
        for (auto i = 0; i < old_size; ++i) {
            if (!is_zero(i))
                reinsert(new_buf, i);
        }
        delete old_buf;
    }
    void reinsert(Cell* old_buf, uint32_t pos) {
        auto hash_value = hash(old_buf[pos].first); 
        auto place_value = place(hash_value);

        auto [is_find, new_place_value] = find_cell(old_buf[pos].first, hash_value, place_value);
        memcpy(static_cast<void*>(&buf[new_place_value]), &old_buf[pos], sizeof(old_buf[pos]));
    }
    bool is_zero(uint32_t place_value) const {
        if (!buf[place_value].second.row_num && !buf[place_value].second.block_offset) {
            return true;
        }
        return false;
    }
    uint32_t hash(const key_t &key) const {
        return XXHash32::hash(key.data(), 64, 0);
    }
    uint32_t place(uint32_t h) const {
        return h & mask(); 
    }
    uint32_t next(uint32_t pos) const {
        ++pos;
        return pos & mask();
    }
    uint32_t buf_size() const {
        return (1 << degree);
    }
    uint32_t max_fill() const { return 1 << (degree - 1); }
    uint32_t mask() const {
        return buf_size() - 1;
    }
    bool is_full() const {
        return m_size > max_fill();
    }
private:
    uint32_t degree;
    uint32_t m_size;
    Cell* buf;
};