#pragma once

#include <cerrno>
#include <fcntl.h> // open
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <sys/mman.h> // mmap, munmap, msync
#include <sys/stat.h> // fstat
#include <system_error>
#include <unistd.h>   // close
#include <utility>    // exchange

using namespace std;

class MmapReader
{
private:
    char* map_start {nullptr};
    size_t map_size {0};
    int fd {-1};

    static int open_file(const filesystem::path& path)
    {
        const auto fd = ::open(path.c_str(), O_RDONLY);

        if (fd == -1)
        {
            throw system_error {errno, system_category(), "cannot open file "s + path.string()};
        }

        return fd;
    }

    static size_t file_size(int fd)
    {
        struct stat state_buf;
        if (::fstat(fd, &state_buf) == -1)
        {
            const auto old_errno = errno;
            ::close(fd);
            throw system_error {old_errno,
                                system_category(),
                                "fstat failed with fd = "s + to_string(fd)};
        }

        return state_buf.st_size;
    }

    void init()
    {
        map_size = file_size(fd);
        void* addr = ::mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);

        if (addr == MAP_FAILED)
        {
            const auto code = errno;
            ::close(fd);
            throw system_error {code,
                                system_category(),
                                "mmap file fd = "s + to_string(fd) + "failed"};
        }
        map_start = static_cast<char*>(addr);
    }

public:
    [[nodiscard]]
    bool is_open() const
    {
        return map_start != nullptr;
    }

    explicit operator bool() const { return map_start != nullptr; }

    void close() noexcept
    {
        if (is_open())
        {
            ::munmap(map_start, map_size);
            ::close(fd);
            map_start = nullptr;
            map_size = 0;
            fd = -1;
        }
    }
    void open(const filesystem::path& path)
    {
        close();
        fd = open_file(path);
        init();
    }

    void open(const int fd)
    {
        close();
        this->fd = fd;
        init();
    }

    MmapReader() = default;
    MmapReader(const filesystem::path& path) { open(path); }
    MmapReader(int fd) { open(fd); }

    ~MmapReader() { close(); }

    MmapReader(const MmapReader&) = delete;
    MmapReader& operator=(const MmapReader&) = delete;

    MmapReader(MmapReader&& that) noexcept
        : map_start {exchange(that.map_start, nullptr)},
          map_size {exchange(that.map_size, 0)},
          fd {exchange(that.fd, -1)}
    {}

    MmapReader& operator=(MmapReader&& that) noexcept
    {
        if (this != &that)
        {
            close();
            map_start = exchange(that.map_start, nullptr);
            map_size = exchange(that.map_size, 0);
            fd = exchange(that.fd, -1);
        }

        return *this;
    }

    [[nodiscard]]
    size_t size() const
    {
        return map_size;
    }

    [[nodiscard]]
    string_view view() const
    {
        return {map_start, map_size};
    }

    [[nodiscard]]
    string str() const
    {
        return {map_start, map_size};
    }

    [[nodiscard]]
    const char* data() const noexcept
    {
        return map_start;
    }
};

class MmapLineReader
{
private:
    MmapReader* in {};
    char delimiter {'\n'};

public:
    explicit MmapLineReader(MmapReader& in_) : in {&in_} {}

    void set_delimiter(char delimiter_) { delimiter = delimiter_; }

    bool read_line(string_view& out, size_t& offset)
    {
        if (offset == in->size()) { return false; }

        size_t cur {offset};
        int found_delimiter {0};

        while (cur < in->size())
        {
            if (in->data()[cur++] == delimiter)
            {
                found_delimiter = 1;
                break;
            }
        }

        out = string_view {in->data() + offset, cur - offset - found_delimiter};
        offset = cur;

        return true;
    }

    class iterator
    {
    private:
        MmapLineReader* reader {};
        size_t offset {};
        string_view line;

    public:
        typedef string_view value_type;
        typedef const value_type* pointer;
        typedef const value_type& reference;
        typedef ptrdiff_t difference_type;
        typedef std::forward_iterator_tag iterator_category;

        iterator() = default;
        explicit iterator(MmapLineReader* reader_) : reader {reader_} { ++*this; }

        reference operator*() const noexcept { return line; }
        pointer operator->() const noexcept { return &line; }
        iterator& operator++()
        {
            if (!reader->read_line(line, offset))
            {
                reader = nullptr;
                offset = 0;
            }
            return *this;
        }
        iterator operator++(int)
        {
            iterator temp(*this);
            ++*this;
            return temp;
        }

        bool operator==(const iterator& rhs) const noexcept
        {
            return reader == rhs.reader && offset == rhs.offset;
        }
        bool operator!=(const iterator& that) const noexcept { return !operator==(that); }
    };

    iterator begin() { return iterator {this}; }
    iterator end() const noexcept { return iterator {}; }
};


class MmapCharReader
{
private:
    MmapReader* in {};

public:
    MmapCharReader(MmapReader& in_) : in {&in_} {}

    [[nodiscard]]
    const char& read_char(size_t offset) const noexcept
    {
        return in->data()[offset];
    }

    class iterator
    {
    private:
        const MmapCharReader* reader {};
        size_t offset {};

    public:
        typedef std::contiguous_iterator_tag iterator_concept;
        typedef char value_type;
        typedef const char* pointer;
        typedef const char& reference;
        typedef ptrdiff_t difference_type;
        typedef std::random_access_iterator_tag iterator_category;

        iterator() = default;
        explicit iterator(const MmapCharReader* reader, size_t offset = 0) noexcept
            : reader(reader), offset(offset)
        {}

        reference operator*() const noexcept { return reader->read_char(offset); }
        pointer operator->() const noexcept { return &reader->read_char(offset); }

        iterator& operator++() noexcept
        {
            ++offset;
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator temp(*this);
            ++*this;
            return temp;
        }
        iterator& operator--() noexcept
        {
            --offset;
            return *this;
        }
        iterator operator--(int) noexcept
        {
            iterator temp(*this);
            --*this;
            return temp;
        }
        iterator& operator+=(ptrdiff_t n) noexcept
        {
            offset += n;
            return *this;
        }
        iterator& operator-=(ptrdiff_t n) noexcept
        {
            offset -= n;
            return *this;
        }
        iterator operator+(ptrdiff_t n) const noexcept { return iterator(reader, offset + n); }
        iterator operator-(ptrdiff_t n) const noexcept { return iterator(reader, offset - n); }
        difference_type operator-(const iterator& rhs) const noexcept
        {
            return offset - rhs.offset;
        }
        reference operator[](difference_type n) const noexcept
        {
            return reader->read_char(offset + n);
        }
        friend iterator operator+(difference_type n, iterator i) noexcept { return i + n; }

        bool operator==(const iterator& rhs) const noexcept
        {
            return reader == rhs.reader && offset == rhs.offset;
        }
        bool operator!=(const iterator& rhs) const noexcept { return !operator==(rhs); }
        bool operator<(const iterator& rhs) const noexcept
        {
            return reader < rhs.reader || (reader == rhs.reader && offset < rhs.offset);
        }
        bool operator>(const iterator& rhs) const noexcept
        {
            return reader > rhs.reader || (reader == rhs.reader && offset > rhs.offset);
        }
        bool operator<=(const iterator& rhs) const noexcept { return !operator>(rhs); }
        bool operator>=(const iterator& rhs) const noexcept { return !operator<(rhs); }
    };

    typedef iterator const_iterator;

    iterator begin() const noexcept { return iterator(this); }
    iterator end() const noexcept { return iterator(this, in->size()); }
};
