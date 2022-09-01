#pragma once
#include <string.h>
#include <string>

class String
{
public:

    String() :data_(nullptr) {}

    String(const char *d) : data_(d), size_(64){}

    const char *data() const { return data_; }

    size_t size() const { return size_; }

    bool empty() const { return data_ == nullptr; }

    char operator[](size_t n) const
    {
        return data_[n];
    }

    void clear()
    {
        delete(data_);
    }

    std::string ToString() const;

    // Three-way comparison.  Returns value:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"
    int compare(const String &b) const;

    bool starts_with(const String &x) const
    {
        return ((size_ >= x.size_) &&
                (memcmp(data_, x.data_, x.size_) == 0));
    }

    bool ends_with(const String &x) const
    {
        return ((size_ >= x.size_) &&
                (memcmp(data_ + size_ - x.size_, x.data_, x.size_) == 0));
    }

private:
    const char *data_;
    size_t size_ = 64;
    // size_t size_;
};

inline bool operator==(const String &x, const String &y)
{
    return ((x.size() == y.size()) &&
            (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const String &x, const String &y)
{
    return !(x == y);
}

inline std::string String::ToString() const
{
    std::string result;
    result.assign(data_, size_);
    return result;
}

inline int String::compare(const String &b) const
{
    const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
    int r = memcmp(data_, b.data_, min_len);
    if (r == 0)
    {
        if (size_ < b.size_)
            r = -1;
        else if (size_ > b.size_)
            r = +1;
    }
    return r;
}