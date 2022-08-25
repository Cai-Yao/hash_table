#pragma once 
#include <algorithm>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <functional>
#include <memory.h>
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
        auto it = &buf[m_size];
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
    uint32_t find(key_t key) {
        auto hash_value = hash(key);
        auto bucket_value = hash_value & mask();
        auto place_value = first[bucket_value];
        while (place_value && buf[place_value - 1].first != key) {
            place_value = next[place_value];       
        }
        return place_value;
    }

    RowRefList* get(uint32_t pos) {
        return &buf[pos].second;
    }

    void resize() {
        degree = degree + (degree > 23 ? 1 : 2);
        auto temp_buf = new Cell[buf_size()];
        auto temp_first = new uint32_t[bucket_size()];
        auto temp_next = new uint32_t[buf_size() + 1];
        memmove(temp_buf, buf, sizeof(Cell) * m_size);
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
};