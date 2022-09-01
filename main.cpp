
#include <random>
#include <unordered_map>
#include <chrono>
#include "xxhash64.h"
#include "String.h"
// #define CHECK
// #define OLD
#define USE_BLOCK
#ifdef OLD
#include "old_hash_table.h"
#else
#include "new_hash_table.h"
#endif

const size_t INSERT_NUM = 100000;
const size_t FIND_NUM = 100000;
const uint32_t BLOCK_NUM = 64;

std::mt19937 rng(1337);
struct Hash {
    size_t operator() (const String &a) const {
        return XXHash64::hash(a.data(), 64, 0);
    }
};
struct Equal {
    bool operator() (const String &a, const String &b) const {
        return memcmp(a.data(), b.data(), 64) == 0;
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

size_t physical_memory_used_by_process()
{
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];
    while (fgets(line, 128, file) != nullptr) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            int len = strlen(line);
            const char* p = line;
            for (; std::isdigit(*p) == false; ++p) {}
            line[len - 3] = 0;
            result = atoi(p);

            break;
        }
    }
    fclose(file);
    return result;
}

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
    for (int i = 0; i < 1000; i++) {
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
    }
    printf("info: init end\n");
    for (auto it : vis) {
        for (auto b : it.second) {
            auto temp = b;
            hashtable.insert(it.first, std::move(temp));
        }
    }

#ifdef OLD
    for (auto it : vis) {
        printf("info: find size %u\n", it.second.size());
        auto place_value = hashtable.find(it.first);
        if (place_value != -1) {
            if (!check(hashtable.get(place_value), it.second)) {
                printf("error: find RowRef no right!!!!!!!!\n");
            }
        }
        else {
            printf("error: no find that must exist!!!!!!!!\n");
        }
    }
#else
    for (auto it : vis) {
        printf("info: find size %u\n", it.second.size());
        auto place_value = hashtable.find(it.first);
        if (place_value) {
            if (!check(hashtable.get(place_value - 1), it.second)) {
                printf("error: find RowRef no right!!!!!!!!\n");
            }
        }
        else {
            printf("error: no find that must exist!!!!!!!!\n");
        }
    }
#endif
#else
    auto inserttimeS = std::chrono::steady_clock::now();
    duration_millsecond = 0;
    for (int i = 0; i < INSERT_NUM; i++) {
        const auto p = new char[64];
        RowRef rf(rng() % INSERT_NUM, rng() % INSERT_NUM);
        for (auto j = 0; j < 64; j++) {
            p[j] = rng() % (1 << 8);
        }
        //printf("info: insert id %d, key %u\n", i, key);
        auto inserttimeS = std::chrono::steady_clock::now();
        hashtable.insert(p, std::move(rf));
        auto inserttimeE = std::chrono::steady_clock::now();
        duration_millsecond += std::chrono::duration<double, std::milli>(inserttimeE - inserttimeS).count();
        if (rng() % 4 != 0) {
            m_s.emplace_back(p);
        }
    }
    printf("insert time: %lfms\n", duration_millsecond);
    printf("collision num: %u\n", hashtable.next_num());

    
    duration_millsecond = 0;
    auto p = 0;
    const auto temp_p = new char[64];
    for(int i = 0; i < FIND_NUM; i++) {
        if (p < m_s.size()) {
            hashtable.find(m_s[p++]);
            continue;
        }
        for (auto j = 0; j < 64; j++) {
            temp_p[j] = rng() % (1 << 8);
        }
        auto findtimeS = std::chrono::steady_clock::now();
        hashtable.find(temp_p);
        auto findtimeE = std::chrono::steady_clock::now();
        duration_millsecond += std::chrono::duration<double, std::milli>(findtimeE - findtimeS).count();
    }
    printf("find time: %lfms\n", duration_millsecond);
    printf("collision num: %u\n", hashtable.next_num());

    //printf("The current process consumes %lu KB memory\n",physical_memory_used_by_process());
#endif
}