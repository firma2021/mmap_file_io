#pragma once

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>

using namespace std;

inline size_t sys_page_size()
{
    long ret = sysconf(_SC_PAGE_SIZE);
    if (ret == -1) { throw system_error {errno, system_category(), "sysconf(_SC_PAGE_SIZE) failed"}; }
    return ret;
}

inline size_t align_offset_to_page_multiple(size_t offset)
{
    const size_t page_size = sys_page_size();
    return offset / page_size * page_size;
}

class mapped_file
{
private:
    void* addr_ {nullptr};
    size_t len_ {0};
    size_t total_len_ {0};
    int fd_ {-1};

    bool is_internal_fd {true};


    static int open_file(const filesystem::path& path, bool write_mode)
    {
        if (path.empty()) { throw invalid_argument {"file path is empty"}; }

        auto fd = ::open(path.string().data(), write_mode ? O_RDWR : O_RDONLY);

        if (fd == -1) { throw system_error {errno, system_category(), "cannot open file "s + path.string()}; }

        return fd;
    }

    static size_t query_file_size(int fd)
    {
        struct stat state_buf;
        if (::fstat(fd, &state_buf) == -1)
        {
            const auto code = errno;
            ::close(fd);
            throw system_error {code, system_category(), "fstat fd = "s + to_string(fd) + "failed"};
        }

        return state_buf.st_size;
    }

    struct MmapContext
    {
        void* addr;
    };

    static MmapContext memory_map() {}

public:
    mapped_file() = default;

    explicit mapped_file(const filesystem::path& path, const size_t offset = 0, const size_t map_size = 0)
    {
        int fd = open_file(path, true);
        size_t len = query_file_size(fd);

        auto* addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
        {
            const auto code = errno;
            ::close(fd);
            throw system_error {code, system_category(), "mmap file "s + path.string() + "failed"};
        }

        this->addr_ = addr;
        this->len_ = len;
        this->fd_ = fd;
    }

    explicit mapped_file(const int map_fd, const size_t offset = 0, const size_t map_size = 0) : fd_(map_fd)
    {
        is_internal_fd = false;

        struct stat sb;
        if (::fstat(fd_, &sb) == -1)
        {
            const auto code = errno;
            ::close(fd_);
            throw system_error {code, system_category(), "mmap file fd = "s + to_string(fd_) + "failed"};
        }
        size_t len = sb.st_size;

        auto* addr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (addr == MAP_FAILED)
        {
            const auto code = errno;
            ::close(fd_);
            throw system_error {code, system_category(), "mmap file fd = "s + to_string(fd_) + "failed"};
        }

        this->addr_ = addr;
        this->len_ = len;
    }

    ~mapped_file() { unmap(); }

    void unmap()
    {
        if (!is_open()) { return; }

        if (addr_ != nullptr) { ::munmap(addr_, len_); }

        if (is_internal_fd) { ::close(fd_); }

        addr_ = nullptr;
        len_ = 0;
        total_len_ = 0;
        fd_ = -1;
    }

    mapped_file(const mapped_file&) = delete;
    mapped_file& operator=(const mapped_file&) = delete;


    mapped_file(mapped_file&& other) noexcept : addr_ {exchange(other.addr_, nullptr)}, len_ {exchange(other.len_, 0)}, fd_ {exchange(other.fd_, -1)} {}

    mapped_file& operator=(mapped_file&& other) noexcept
    {
        unmap();

        addr_ = exchange(other.addr_, nullptr);
        len_ = exchange(other.len_, 0);
        fd_ = exchange(other.fd_, -1);

        return *this;
    }

    void sync(bool async = false)
    {
        if (!*this) { return; }

        if (async) { ::msync(addr_, len_, MS_ASYNC); }
        else { ::msync(addr_, len_, MS_SYNC); }
    }

    [[nodiscard]]
    size_t length() const
    {
        return len_;
    }

    explicit operator bool() const { return addr_ != nullptr; }

    [[nodiscard]]
    bool is_open() const
    {
        return fd_ != -1;
    }

    [[nodiscard]]
    int fd() const
    {
        return fd_;
    }

    [[nodiscard]]
    size_t size() const
    {
        return len_;
    }

    [[nodiscard]]
    bool empty() const
    {
        return size() == 0;
    }

    [[nodiscard]]
    size_t total_size() const
    {
        return total_len_;
    }

    [[nodiscard]]
    size_t mapping_offset() const
    {
        return total_len_ - len_;
    }

    [[nodiscard]]
    char* data() const
    {
        return static_cast<char*>(addr_);
    }

    [[nodiscard]]
    void* get() const
    {
        return addr_;
    }

    char& operator[](const size_t i) noexcept { return data()[i]; }
    const char& operator[](const size_t i) const noexcept { return data()[i]; }
};
