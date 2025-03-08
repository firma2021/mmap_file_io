#pragma once

#include <cerrno>
#include <cstring>
#include <fcntl.h> // open
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/mman.h> // mmap, munmap, msync
#include <sys/stat.h> // fstat
#include <unistd.h>   // close
#include <utility>    // std::exchange

class mmap_reader
{
private:
    char* mapped_ptr {nullptr};
    size_t map_size {0};
    int fd {-1};

    size_t current_offset {0};

    static int open_file(const std::filesystem::path& path)
    {
        const auto fd = ::open(path.c_str(), O_RDONLY);

        if (fd == -1)
        {
            throw std::system_error {errno,
                                     std::system_category(),
                                     std::string {"cannot open file "} + path.string()};
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
            throw std::system_error {old_errno,
                                     std::system_category(),
                                     std::string {"fstat failed with fd = "} + std::to_string(fd)};
        }

        return state_buf.st_size;
    }

    void memory_map()
    {
        map_size = file_size(fd);
        void* addr = ::mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);

        if (addr == MAP_FAILED)
        {
            const auto code = errno;
            ::close(fd);
            throw std::system_error {code,
                                     std::system_category(),
                                     std::string {"mmap failed with fd = "} + std::to_string(fd)};
        }
        mapped_ptr = static_cast<char*>(addr);
    }

private:
    [[nodiscard]]
    bool eof() const noexcept
    {
        return current_offset >= map_size;
    }

    char next_char() noexcept { return mapped_ptr[current_offset++]; }

    std::string_view next_line(char delimiter) noexcept
    {
        size_t cur = current_offset;
        int found_delimiter = 0;

        while (cur < map_size)
        {
            if (mapped_ptr[cur++] == delimiter)
            {
                found_delimiter = 1;
                break;
            }
        }

        std::string_view line {mapped_ptr + current_offset, cur - current_offset - found_delimiter};

        current_offset = cur;

        return line;
    }

    class LineReader
    {
    private:
        mmap_reader* reader;
        char delimiter;

        class iterator
        {
        private:
            mmap_reader* reader {};
            char delimiter {};

        public:
            iterator() = default;
            iterator(mmap_reader* in_, char delimiter_) : reader {in_}, delimiter {delimiter_} {}

            std::string_view operator*() const noexcept { return reader->next_line(delimiter); }

            iterator& operator++() { return *this; }

            bool operator!=([[maybe_unused]] const iterator& eof_iter) const noexcept
            {
                return !reader->eof();
            }
        };

    public:
        explicit LineReader(mmap_reader& r, char delim = '\n') : reader(&r), delimiter(delim) {}

        iterator begin() { return iterator {reader, delimiter}; }
        iterator end() { return {}; }
    };

    class CharReader
    {
    private:
        mmap_reader* reader;

        class iterator
        {
        private:
            mmap_reader* reader {};

        public:
            iterator() = default;
            explicit iterator(mmap_reader* in_) : reader {in_} {}

            char operator*() const noexcept { return reader->next_char(); }

            iterator& operator++() { return *this; }

            bool operator!=([[maybe_unused]] const iterator& eof_iter) const noexcept
            {
                return !reader->eof();
            }
        };

    public:
        explicit CharReader(mmap_reader& r) : reader {&r} {}

        iterator begin() { return iterator {reader}; }
        iterator end() { return {}; }
    };

public:
    [[nodiscard]]
    bool is_open() const noexcept
    {
        return mapped_ptr != nullptr;
    }

    explicit operator bool() const noexcept { return is_open() && current_offset < map_size; }

    void close() noexcept
    {
        if (is_open())
        {
            ::munmap(mapped_ptr, map_size);
            ::close(fd);
            mapped_ptr = nullptr;
            map_size = 0;
            fd = -1;
            current_offset = 0;
        }
    }

    void open(const std::filesystem::path& path)
    {
        close();
        fd = open_file(path);
        memory_map();
    }

    void open(const int new_fd)
    {
        close();
        fd = new_fd;
        memory_map();
    }

    mmap_reader() = default;
    explicit mmap_reader(const std::filesystem::path& path) { open(path); }
    explicit mmap_reader(int fd) { open(fd); }

    ~mmap_reader() { close(); }

    mmap_reader(const mmap_reader&) = delete;
    mmap_reader& operator=(const mmap_reader&) = delete;

    mmap_reader(mmap_reader&& that) noexcept
        : mapped_ptr {std::exchange(that.mapped_ptr, nullptr)},
          map_size {std::exchange(that.map_size, 0)},
          fd {std::exchange(that.fd, -1)},
          current_offset {std::exchange(that.current_offset, 0)}
    {}

    mmap_reader& operator=(mmap_reader&& that) noexcept
    {
        if (this != &that)
        {
            close();
            mapped_ptr = std::exchange(that.mapped_ptr, nullptr);
            map_size = std::exchange(that.map_size, 0);
            fd = std::exchange(that.fd, -1);
            current_offset = std::exchange(that.current_offset, 0);
        }

        return *this;
    }

    [[nodiscard]]
    const char* data() const noexcept
    {
        return mapped_ptr;
    }

    [[nodiscard]]
    size_t size() const noexcept
    {
        return map_size;
    }

    [[nodiscard]]
    size_t tell() const noexcept
    {
        return current_offset;
    }

    void seek(size_t pos) noexcept { current_offset = std::min(pos, map_size); }

    enum seekdir : unsigned char
    {
        beg,
        cur,
        end
    };

    void seek(ptrdiff_t off, seekdir dir) noexcept
    {
        switch (dir)
        {
        case seekdir::beg:
            if (off < 0) { current_offset = 0; }
            else { current_offset = std::min(static_cast<size_t>(off), map_size); }
            break;
        case seekdir::cur:
            if (off < 0)
            {
                current_offset = (static_cast<size_t>(-off) > current_offset)
                                     ? 0
                                     : current_offset - static_cast<size_t>(-off);
            }
            else { current_offset = std::min(current_offset + static_cast<size_t>(off), map_size); }
            break;
        case seekdir::end:
            if (off < 0)
            {
                current_offset = (static_cast<size_t>(-off) > map_size)
                                     ? 0
                                     : map_size - static_cast<size_t>(-off);
            }
            else { current_offset = map_size; }
            break;
        }
    }

    size_t read(std::span<char> buf) noexcept
    {
        const size_t to_read = std::min(buf.size(), map_size - current_offset);
        memcpy(buf.data(), mapped_ptr + current_offset, to_read);

        current_offset += to_read;

        return to_read;
    }

    size_t pread(std::span<char> buf, size_t offset) const noexcept
    {
        if (offset >= map_size) { return 0; }

        const size_t to_read = std::min(buf.size(), map_size - offset);
        memcpy(buf.data(), mapped_ptr + offset, to_read);

        return to_read;
    }

    [[nodiscard]]
    std::optional<std::string_view> getline(char delimiter = '\n') noexcept
    {
        if (current_offset >= map_size) { return std::nullopt; }

        size_t cur = current_offset;
        int found_delimiter = 0;

        while (cur < map_size)
        {
            if (mapped_ptr[cur++] == delimiter)
            {
                found_delimiter = 1;
                break;
            }
        }

        std::string_view line {mapped_ptr + current_offset, cur - current_offset - found_delimiter};

        current_offset = cur;

        return line;
    }

    [[nodiscard]]
    std::optional<char> getchar() noexcept
    {
        if (current_offset >= map_size) { return std::nullopt; }
        return mapped_ptr[current_offset++];
    }

    [[nodiscard]]
    LineReader lines(char delimiter = '\n')
    {
        return LineReader(*this, delimiter);
    }

    [[nodiscard]]
    CharReader chars()
    {
        return CharReader(*this);
    }

    [[nodiscard]]
    std::string_view view() const noexcept
    {
        return {mapped_ptr, map_size};
    }

    [[nodiscard]]
    std::string str() const
    {
        return {mapped_ptr, map_size};
    }

    [[nodiscard]]
    std::string_view view(size_t offset, size_t len) const noexcept
    {
        if (offset >= map_size) { return {}; }

        const size_t available = map_size - offset;
        const size_t actual_len = std::min(len, available);
        return {mapped_ptr + offset, actual_len};
    }

    [[nodiscard]]
    std::string str(size_t offset, size_t len) const
    {
        if (offset >= map_size) { return {}; }

        const size_t available = map_size - offset;
        const size_t actual_len = std::min(len, available);
        return {mapped_ptr + offset, actual_len};
    }
};
