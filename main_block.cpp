
#include <random>
#include <unordered_map>
#include <map>
#include <vector>
#include <chrono>
#include "xxhash64.h"
#include "String.h"
#define CHECK
#include "new_hash_table.h"

const size_t INSERT_NUM = 10000000;
const size_t FIND_NUM = 10000000;
const size_t TEST_NUM = 200000;

const uint32_t BLOCK_NUM = 64;

std::mt19937 rng(1337);
struct Hash {
    size_t operator() (const String &a) const {
        return XXHash64::hash(a.data(), 64, 0);
    }
};
struct Equal {
    bool operator() (const String &a, const String &b) const {
        return memcmp((void *)a.data(), (void *)b.data(), 64) == 0;
    }
};
std::unordered_map<String, std::vector<RowRef>, Hash, Equal> vis;

bool check(RowRefList* x, const std::vector<RowRef> & y) {
    if (x->get_row_count() != y.size()) return false;
    if (y.size() == 1) {
        return (y[0].block_offset == x->block_offset && y[0].row_num == x->row_num);
    }
    return true;
}

std::vector<String> m_s;

int main() {
    /// init
    auto buildtimeS = std::chrono::steady_clock::now();
    printf("info: init begin\n");
    double duration_millsecond;
    HashTable hashtable(10);
    auto buildtimeE = std::chrono::steady_clock::now();
    duration_millsecond = std::chrono::duration<double, std::milli>(buildtimeE - buildtimeS).count();
    printf("build time: %lfms\n", duration_millsecond);

#ifdef CHECK
    std::vector<std::pair<String, RowRef>> datas;
    for (int i = 0; i < TEST_NUM; i++) {
        const auto p = new char[64];
        RowRef rf(rng() % INSERT_NUM, rng() % INSERT_NUM);
        for (auto j = 0; j < 64; j++) {
            p[j] = rng() % (1 << 8);
        }
        if (vis.find(p) != vis.end()) {
            vis[p].emplace_back(rf);
        } else {
            vis[p] = {rf};
        }
        datas.emplace_back(p, rf);
    }
    printf("info: init end\n");

    /// check insert with block
    for (int i = 0; i < TEST_NUM;) {
        auto block_size = 0;
        auto keys = new String[BLOCK_NUM];
        auto values = new RowRef[BLOCK_NUM];
        for (int j = i; j < TEST_NUM && j < i + BLOCK_NUM; j++) {
            ++block_size;
            keys[j - i] = datas[j].first;
            values[j - i] = datas[j].second;
        }
        //printf("block size %d\n", block_size);
        hashtable.m_insert(keys, values, block_size);
        delete[] keys;
        delete[] values;
        i += BLOCK_NUM;
    }

    /// check find with block
    for (int i = 0; i < TEST_NUM;) {
        auto block_size = 0;
        auto keys = new String[BLOCK_NUM];
        for (int j = i; j < TEST_NUM && j < i + BLOCK_NUM; j++) {
            ++block_size;
            keys[j - i] = datas[j].first;
        }
        auto res = hashtable.m_find(keys, block_size);
        for (auto j = 0; j < block_size; j++) {
            //printf("info: find size %lu\n", vis[keys[j]].size());
            if (res[j] == 0) {
                printf("error: no find that must exist!!!!!!!!\n");
                exit(0);
            } else {
                if (!check(hashtable.get(res[j] - 1), vis[keys[j]])) {
                    printf("error: find RowRef no right!!!!!!!!\n");
                    exit(0);
                }
            }
        }
        delete[] keys;
        i += BLOCK_NUM;
    }

#else
    /// init
    std::vector<std::pair<String, RowRef>> datas;
    for (int i = 0; i < INSERT_NUM; i++) {
        const auto p = new char[64];
        RowRef rf(rng() % INSERT_NUM, rng() % INSERT_NUM);
        for (auto j = 0; j < 64; j++) {
            p[j] = rng() % (1 << 8);
        }
        datas.emplace_back(p, rf);
    }

    duration_millsecond = 0;
    for (int i = 0; i < INSERT_NUM;) {
        const auto p = new char[64];
        auto block_size = 0;
        auto keys = new String[BLOCK_NUM];
        auto values = new RowRef[BLOCK_NUM];
        for (int j = i; j < INSERT_NUM && j < i + BLOCK_NUM; j++) {
            ++block_size;
            keys[j - i] = datas[j].first;
            values[j - i] = datas[j].second;
            if (rng() % 4 != 0) {
                m_s.emplace_back(p);
            }
        }
        auto inserttimeS = std::chrono::steady_clock::now();
        hashtable.m_insert(keys, values, block_size);
        auto inserttimeE = std::chrono::steady_clock::now();
        duration_millsecond += std::chrono::duration<double, std::milli>(inserttimeE - inserttimeS).count();
        i += BLOCK_NUM;
    }
    printf("insert time: %lfms\n", duration_millsecond);
    printf("m_s size %lu\n", m_s.size());

    while (m_s.size() != FIND_NUM) {
        const auto p = new char[64];
        for (auto j = 0; j < 64; j++) {
            p[j] = rng() % (1 << 8);
        }
        m_s.emplace_back(p);
    }

    /// find
    duration_millsecond = 0;
    for(int i = 0; i < FIND_NUM;) {
        auto block_size = 0;
        auto keys = new String[BLOCK_NUM];
        for (int j = i; j < FIND_NUM && j < i + BLOCK_NUM; j++) {
            ++block_size;
            keys[j - i] = m_s[j];
        }
        auto findtimeS = std::chrono::steady_clock::now();
        hashtable.m_find(keys, block_size);
        auto findtimeE = std::chrono::steady_clock::now();
        duration_millsecond += std::chrono::duration<double, std::milli>(findtimeE - findtimeS).count();
        i += BLOCK_NUM;
    }
    printf("find time: %lfms\n", duration_millsecond);

    //printf("The current process consumes %lu KB memory\n",physical_memory_used_by_process());
#endif
}