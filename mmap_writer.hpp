#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <span>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

class mmap_writer
{
private:
    char* mapped_ptr {};
    int fd {-1};

    size_t current_offset {};
    size_t max_offset {};

    size_t capacity {};
    inline static const size_t default_expand_size {8192};
    size_t expand_size {default_expand_size};


    void expand(size_t new_size)
    {
        size_t new_capacity = capacity;
        while (new_capacity < new_size) { new_capacity += expand_size; }

        if (ftruncate(fd, static_cast<off_t>(new_capacity)) == -1)
        {
            const auto code = errno;
            ::munmap(mapped_ptr, capacity);
            ::close(fd);
            throw std::system_error {code, std::system_category(), "ftruncate failed"};
        }

        void* new_ptr = mremap(mapped_ptr, capacity, new_capacity, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED)
        {
            const auto code = errno;
            ::munmap(mapped_ptr, capacity);
            ::close(fd);
            throw std::system_error {code, std::system_category(), "mremap failed"};
        }

        mapped_ptr = static_cast<char*>(new_ptr);
        capacity = new_capacity;
    }

    void open_file(const std::filesystem::path& filename, bool truncate)
    {
        const int flag = truncate ? O_TRUNC : O_APPEND;

        // When using mmap with PROT_WRITE and MAP_SHARED, the open call must use O_RDWR instead of write-only mode.
        fd = ::open(filename.c_str(), O_RDWR | O_CREAT | flag, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            throw std::system_error {errno, std::system_category(), "Failed to open file"};
        }

        if (!truncate)
        {
            struct stat sb;
            if (fstat(fd, &sb) == -1)
            {
                const auto code = errno;
                ::close(fd);
                throw std::system_error {code, std::system_category(), "fstat failed"};
            }
            current_offset = static_cast<size_t>(sb.st_size);
        }

        capacity = current_offset + expand_size;

        if (ftruncate(fd, static_cast<off_t>(capacity)) == -1)
        {
            const auto code = errno;
            ::close(fd);
            throw std::system_error {code, std::system_category(), "ftruncate failed"};
        }

        void* addr = mmap(nullptr, capacity, PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
        {
            const auto code = errno;
            ::close(fd);
            throw std::system_error {errno, std::system_category(), "mmap failed"};
        }
        mapped_ptr = static_cast<char*>(addr);
    }

public:
    mmap_writer(const std::filesystem::path& filename, bool truncate) { open(filename, truncate); }

    ~mmap_writer() { close(); }

    mmap_writer(const mmap_writer&) = delete;
    mmap_writer& operator=(const mmap_writer&) = delete;

    mmap_writer(mmap_writer&& that) noexcept
        : mapped_ptr {std::exchange(that.mapped_ptr, nullptr)},
          fd {std::exchange(that.fd, -1)},
          current_offset {std::exchange(that.current_offset, 0)},
          max_offset {std::exchange(that.max_offset, 0)},
          capacity {std::exchange(that.capacity, 0)},
          expand_size {std::exchange(that.expand_size, default_expand_size)}

    {}

    mmap_writer& operator=(mmap_writer&& that) noexcept
    {
        if (this != &that)
        {
            close();
            mapped_ptr = std::exchange(that.mapped_ptr, nullptr);
            fd = std::exchange(that.fd, 0);
            current_offset = std::exchange(that.current_offset, 0);
            max_offset = std::exchange(that.max_offset, 0);
            capacity = std::exchange(that.capacity, 0);
            expand_size = std::exchange(that.expand_size, default_expand_size);
        }

        return *this;
    }

    [[nodiscard]]
    bool is_open() const noexcept
    {
        return mapped_ptr != nullptr;
    }

    explicit operator bool() const noexcept { return is_open(); }

    void close() noexcept
    {
        if (is_open())
        {
            ::munmap(mapped_ptr, capacity);
            if (max_offset < capacity)
            {
                std::ignore = ftruncate(fd, static_cast<off_t>(max_offset));
            }
            ::close(fd);

            mapped_ptr = nullptr;
            fd = -1;
            current_offset = 0;
            max_offset = 0;
            capacity = 0;
            expand_size = default_expand_size;
        }
    }

    void open(const std::filesystem::path& filename, bool truncate = true)
    {
        close();
        open_file(filename, truncate);
    }

    void reserve(size_t new_size) { expand(new_size); }
    void set_expand_size(size_t new_expand_size) { expand_size = new_expand_size; }

    void shrink_to_fit()
    {
        if (capacity == max_offset) { return; }

        void* new_ptr = mremap(mapped_ptr, capacity, max_offset, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED)
        {
            throw std::system_error {errno, std::system_category(), "mremap failed"};
        }

        std::ignore = ftruncate(fd, static_cast<off_t>(max_offset));

        mapped_ptr = static_cast<char*>(new_ptr);
        capacity = max_offset;
    }

    [[nodiscard]]
    size_t tell() const noexcept
    {
        return current_offset;
    }

    enum seekdir : unsigned char
    {
        beg,
        cur,
        end
    };

    void seek(size_t pos)
    {
        if (pos > capacity) { expand(pos); }

        current_offset = pos;
        max_offset = std::max(max_offset, current_offset);
    }

    void seek(ptrdiff_t off, seekdir dir)
    {
        size_t new_offset {current_offset};

        switch (dir)
        {
        case seekdir::beg:
            if (off < 0) { new_offset = 0; }
            else { new_offset = static_cast<size_t>(off); }
            break;

        case seekdir::cur:
            if (off < 0)
            {
                new_offset = (static_cast<size_t>(-off) > current_offset)
                                 ? 0
                                 : current_offset - static_cast<size_t>(-off);
            }
            else { new_offset += static_cast<size_t>(off); }
            break;

        case seekdir::end:
            if (off < 0)
            {
                new_offset = (static_cast<size_t>(-off) > max_offset)
                                 ? 0
                                 : max_offset - static_cast<size_t>(-off);
            }
            else { new_offset = max_offset + static_cast<size_t>(off); }
            break;
        }

        if (new_offset > capacity) { expand(new_offset); }

        current_offset = new_offset;
        max_offset = std::max(max_offset, current_offset);
    }

    void write(std::span<const char> buf)
    {
        if (current_offset + buf.size() > capacity) { expand(current_offset + buf.size()); }
        memcpy(mapped_ptr + current_offset, buf.data(), buf.size());
        current_offset += buf.size();
        max_offset = std::max(max_offset, current_offset);
    }

    void pwrite(size_t offset, std::span<const char> buf)
    {
        if (offset + buf.size() > capacity) { expand(offset + buf.size()); }
        memcpy(mapped_ptr + offset, buf.data(), buf.size());
        max_offset = std::max(max_offset, offset + buf.size());
    }

    void flush(bool async = false)
    {
        if (msync(mapped_ptr, max_offset, async ? MS_ASYNC : MS_SYNC) == -1)
        {
            throw std::system_error {errno, std::system_category(), "msync failed"};
        }
    }
};
