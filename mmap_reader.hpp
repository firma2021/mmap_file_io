#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdio>  // perror
#include <fcntl.h> // open
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h> // mmap, munmap, msync
#include <sys/stat.h> // fstat
#include <unistd.h>   // close

class mmap_reader
{
private:
    struct File
    {
    private:
        static int open(std::string_view path)
        {
            const int fd = ::open(path.data(), O_RDONLY);

            if (fd == -1)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         std::format("mmap_reader: cannot open file {}", path)};
            }

            return fd;
        }

        void close()
        {
            if (should_close && ::close(fd) == -1) { perror("mmap_reader: close file failed"); }
        }

    public:
        int fd;
        bool should_close;

        explicit File(std::string_view path) : fd {open(path)}, should_close {true} {}

        explicit File(int fd) : fd {fd}, should_close {false}
        {
            if (fd < 0) { throw std::invalid_argument {"mmap_reader: invalid file descriptor"}; }
        }

        ~File() { close(); }

        File(const File&) = delete;
        File& operator=(const File&) = delete;
        File(File&& that) noexcept = delete;
        File& operator=(File&& that) noexcept = delete;
    };

    struct MmapData
    {
    private:
        static size_t file_size(int fd)
        {
            struct stat state_buf;
            if (::fstat(fd, &state_buf) == -1)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         std::string {"mmap_reader: fstat failed with fd = "} +
                                             std::to_string(fd)};
            }

            return state_buf.st_size;
        }

        void memory_map(int fd)
        {
            map_size = file_size(fd);

            void* addr = ::mmap(nullptr, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (addr == MAP_FAILED)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         std::string {"mmap_reader: mmap failed with fd = "} +
                                             std::to_string(fd)};
            }

            mapped_ptr = static_cast<char*>(addr);
        }

        void unmap()
        {
            if (::munmap(mapped_ptr, map_size) == -1) { perror("mmap_reader: munmap failed"); }
        }

    public:
        char* mapped_ptr;
        size_t map_size;

        explicit MmapData(int fd) { memory_map(fd); }
        ~MmapData() { unmap(); }

        MmapData(const MmapData&) = delete;
        MmapData& operator=(const MmapData&) = delete;

        MmapData(MmapData&& that) noexcept = delete;

        MmapData& operator=(MmapData&& that) noexcept = delete;
    };

    File file;
    MmapData mmap_data;

    size_t read_pos {0};

    enum class seekdir : unsigned char
    {
        beg,
        cur,
        end
    };

private:
    [[nodiscard]]
    bool eof() const noexcept
    {
        return read_pos >= mmap_data.map_size;
    }

    char next_char() noexcept { return mmap_data.mapped_ptr[read_pos++]; }

    std::string_view next_line(char delimiter) noexcept
    {
        int found_delimiter = 0;

        size_t cur = read_pos;
        while (cur < mmap_data.map_size)
        {
            if (mmap_data.mapped_ptr[cur++] == delimiter)
            {
                found_delimiter = 1;
                break;
            }
        }

        std::string_view line {mmap_data.mapped_ptr + read_pos, cur - read_pos - found_delimiter};

        read_pos = cur;

        return line;
    }

    class LineReader
    {
    private:
        mmap_reader& reader;
        char delimiter;

        class iterator
        {
        private:
            mmap_reader& reader;
            char delimiter {};

        public:
            iterator(mmap_reader& in_, char delimiter_) : reader {in_}, delimiter {delimiter_} {}

            std::string_view operator*() const noexcept { return reader.next_line(delimiter); }

            iterator& operator++() { return *this; }

            bool operator!=([[maybe_unused]] const iterator& eof_iter) const noexcept
            {
                return !reader.eof();
            }
        };

    public:
        explicit LineReader(mmap_reader& r, char delim) : reader {r}, delimiter {delim} {}

        iterator begin() { return iterator {reader, delimiter}; }
        iterator end() { return iterator {reader, delimiter}; }
    };

    class CharReader
    {
    private:
        mmap_reader& reader;

        class iterator
        {
        private:
            mmap_reader& reader;

        public:
            explicit iterator(mmap_reader& in_) : reader {in_} {}

            char operator*() const noexcept { return reader.next_char(); }

            iterator& operator++() { return *this; }

            bool operator!=([[maybe_unused]] const iterator& eof_iter) const noexcept
            {
                return !reader.eof();
            }
        };

    public:
        explicit CharReader(mmap_reader& r) : reader {r} {}

        iterator begin() { return iterator {reader}; }
        iterator end() { return iterator {reader}; }
    };

public:
    explicit mmap_reader(std::string_view path) : file {path}, mmap_data {file.fd} {}
    explicit mmap_reader(int fd) : file {fd}, mmap_data {fd} {}

    explicit operator bool() const noexcept { return !eof(); }

    [[nodiscard]]
    const char* data() const noexcept
    {
        return mmap_data.mapped_ptr;
    }

    [[nodiscard]]
    size_t size() const noexcept
    {
        return mmap_data.map_size;
    }

    [[nodiscard]]
    size_t tell() const noexcept
    {
        return read_pos;
    }

    void seek(size_t pos) noexcept { read_pos = std::min(pos, mmap_data.map_size); }

    static const seekdir beg {seekdir::beg};
    static const seekdir cur {seekdir::cur};
    static const seekdir end {seekdir::end};

    void seek(ptrdiff_t off, seekdir dir) noexcept
    {
        switch (dir)
        {
        case seekdir::beg:
            if (off < 0) { read_pos = 0; }
            else { read_pos = std::min(static_cast<size_t>(off), mmap_data.map_size); }
            break;
        case seekdir::cur:
            if (off < 0)
            {
                read_pos = (static_cast<size_t>(-off) > read_pos)
                               ? 0
                               : read_pos - static_cast<size_t>(-off);
            }
            else { read_pos = std::min(read_pos + static_cast<size_t>(off), mmap_data.map_size); }
            break;
        case seekdir::end:
            if (off < 0)
            {
                read_pos = (static_cast<size_t>(-off) > mmap_data.map_size)
                               ? 0
                               : mmap_data.map_size - static_cast<size_t>(-off);
            }
            else { read_pos = mmap_data.map_size; }
            break;
        default:
            break;
        }
    }

    std::span<char> read(std::span<char> buf) noexcept
    {
        const size_t to_read = std::min(buf.size(), mmap_data.map_size - read_pos);
        std::copy_n(mmap_data.mapped_ptr + read_pos, to_read, buf.data());

        read_pos += to_read;

        return buf.first(to_read);
    }

    size_t pread(std::span<char> buf, size_t offset) const noexcept
    {
        if (offset >= mmap_data.map_size) { return 0; }

        const size_t to_read = std::min(buf.size(), mmap_data.map_size - offset);
        std::copy_n(mmap_data.mapped_ptr + offset, to_read, buf.data());

        return to_read;
    }

    [[nodiscard]]
    std::optional<std::string_view> getline(char delimiter = '\n') noexcept
    {
        if (eof()) { return std::nullopt; }
        return next_line(delimiter);
    }

    [[nodiscard]]
    std::optional<char> getchar() noexcept
    {
        if (eof()) { return std::nullopt; }
        return next_char();
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
        return {mmap_data.mapped_ptr, mmap_data.map_size};
    }

    [[nodiscard]]
    std::string str() const
    {
        return {mmap_data.mapped_ptr, mmap_data.map_size};
    }

    [[nodiscard]]
    std::string_view view(size_t offset, size_t len) const noexcept
    {
        if (offset >= mmap_data.map_size) { return {}; }

        const size_t available = mmap_data.map_size - offset;
        const size_t actual_len = std::min(len, available);
        return {mmap_data.mapped_ptr + offset, actual_len};
    }

    [[nodiscard]]
    std::string str(size_t offset, size_t len) const
    {
        return std::string {view(offset, len)};
    }
};
