#pragma once

#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <format>
#include <span>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

class mmap_writer
{
private:
    struct File
    {
    private:
        static int open(std::string_view filename, bool truncate)
        {
            const int flag = truncate ? O_TRUNC : O_APPEND;

            // When using mmap with PROT_WRITE and MAP_SHARED, the open call must use O_RDWR instead of write-only mode.
            const int fd = ::open(filename.data(), O_RDWR | O_CREAT | flag, S_IRUSR | S_IWUSR);
            if (fd == -1)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         std::format("mmap_writer: cannot open file {}", filename)};
            }

            return fd;
        }

    public:
        mmap_writer* writer;
        int fd;
        bool should_close;

        File(mmap_writer* writer_, std::string_view filename, bool truncate)
            : writer {writer_}, fd {open(filename, truncate)}, should_close {true}
        {}

        File(mmap_writer* writer_, int fd_) : writer {writer_}, fd {fd_}, should_close {false}
        {
            if (fd < 0) { throw std::invalid_argument {"mmap_writer: invalid file descriptor"}; }
        }

        File(const File&) = delete;
        File& operator=(const File&) = delete;
        File(File&& that) noexcept = delete;
        File& operator=(File&& that) noexcept = delete;

        ~File()
        {
            if (writer->max_write_pos < writer->file_size)
            {
                if (ftruncate(fd, static_cast<off_t>(writer->max_write_pos)) == -1)
                {
                    perror("mmap_writer: ftruncate failed");
                }
            }

            if (should_close && ::close(fd) == -1) { perror("mmap_writer: close file failed"); }
        }

        [[nodiscard]]
        size_t file_size() const
        {
            struct stat state_buf;
            if (::fstat(fd, &state_buf) == -1)
            {
                throw std::system_error {
                    errno,
                    std::system_category(),
                    std::format("mmap_writer: fstat failed with fd = {}", std::to_string(fd))};
            }

            return static_cast<size_t>(state_buf.st_size);
        }

        void resize(size_t new_size)
        {
            if (ftruncate(fd, static_cast<off_t>(new_size)) == -1)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         "mmap_writer: ftruncate failed"};
            }
        }
    };

    struct MmapData
    {
    public:
        mmap_writer* writer;

        char* mapped_ptr {};

        explicit MmapData(mmap_writer* writer_) : writer {writer_} {}

        MmapData(const MmapData&) = delete;
        MmapData& operator=(const MmapData&) = delete;
        MmapData(MmapData&& that) noexcept = delete;
        MmapData& operator=(MmapData&& that) noexcept = delete;

        ~MmapData()
        {
            if (mapped_ptr != nullptr && ::munmap(mapped_ptr, writer->file_size) == -1)
            {
                perror("mmap_writer: munmap failed");
            }
        }

        void memory_map()
        {
            void* addr =
                ::mmap(nullptr, writer->file_size, PROT_WRITE, MAP_SHARED, writer->file.fd, 0);
            if (addr == MAP_FAILED)
            {
                throw std::system_error {
                    errno,
                    std::system_category(),
                    std::format("mmap_writer: mmap failed with fd = {}", writer->file.fd)};
            }

            mapped_ptr = static_cast<char*>(addr);
        }

        void remap(size_t new_size)
        {
            void* new_ptr = mremap(mapped_ptr, writer->file_size, new_size, MREMAP_MAYMOVE);
            if (new_ptr == MAP_FAILED)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         "mmap_writer: mremap failed"};
            }

            mapped_ptr = static_cast<char*>(new_ptr);
        }

        void sync(bool async)
        {
            if (msync(mapped_ptr, writer->max_write_pos, async ? MS_ASYNC : MS_SYNC) == -1)
            {
                throw std::system_error {errno,
                                         std::system_category(),
                                         "mmap_writer: msync failed"};
            }
        }
    };

    size_t file_size;
    size_t write_pos;
    size_t max_write_pos;

    enum class seekdir : unsigned char
    {
        beg,
        cur,
        end
    };

    size_t expand_size {8192};

    File file;
    MmapData mmap_data;

    void expand(size_t required_size)
    {
        if (expand_size == 0)
        {
            throw std::logic_error {"mmap_writer: expand_size must be greater than 0"};
        }

        size_t new_file_size = file_size;
        while (new_file_size < required_size) { new_file_size += expand_size; }

        file.resize(new_file_size);
        mmap_data.remap(new_file_size);
        file_size = new_file_size;
    }

    void init(bool truncate, size_t reserved_size)
    {
        const size_t old_file_size = file.file_size();
        if (old_file_size == 0) { file_size = (reserved_size > 0 ? reserved_size : 8192); }
        else { file_size = old_file_size + reserved_size; }

        file.resize(file_size);
        mmap_data.memory_map();

        if (!truncate)
        {
            write_pos = old_file_size;
            max_write_pos = old_file_size;
        }
        else
        {
            write_pos = 0;
            max_write_pos = 0;
        }
    }

public:
    mmap_writer(std::string_view filename, bool truncate, size_t reserved_size = 0)
        : file {this, filename, truncate}, mmap_data {this}
    {
        init(truncate, reserved_size);
    }

    mmap_writer(int fd, bool truncate, size_t reserved_size = 0) : file {this, fd}, mmap_data {this}
    {
        init(truncate, reserved_size);
    }


    [[nodiscard]] size_t size() const noexcept { return max_write_pos; }

    [[nodiscard]] size_t capacity() const noexcept { return file_size; }

    void set_expand_size(size_t new_expand_size)
    {
        expand_size = new_expand_size > 0 ? new_expand_size : 8192;
    }

    void shrink_to_fit()
    {
        if (max_write_pos < file_size)
        {
            file.resize(max_write_pos);
            mmap_data.remap(max_write_pos);
            file_size = max_write_pos;
        }
    }

    [[nodiscard]]
    size_t tell() const noexcept
    {
        return write_pos;
    }

    static const seekdir beg {seekdir::beg};
    static const seekdir cur {seekdir::cur};
    static const seekdir end {seekdir::end};

    void seek(size_t pos)
    {
        if (pos > file_size) { expand(pos); }

        write_pos = pos;
        max_write_pos = std::max(max_write_pos, write_pos);
    }

    void seek(ptrdiff_t off, seekdir dir)
    {
        size_t new_pos {write_pos};

        switch (dir)
        {
        case seekdir::beg:
            new_pos = off < 0 ? 0 : static_cast<size_t>(off);
            break;

        case seekdir::cur:
            if (off < 0)
            {
                new_pos = (static_cast<size_t>(-off) > write_pos)
                              ? 0
                              : write_pos - static_cast<size_t>(-off);
            }
            else { new_pos += static_cast<size_t>(off); }
            break;

        case seekdir::end:
            if (off < 0)
            {
                new_pos = (static_cast<size_t>(-off) > max_write_pos)
                              ? 0
                              : max_write_pos - static_cast<size_t>(-off);
            }
            else { new_pos = max_write_pos + static_cast<size_t>(off); }
            break;
        default:
            break;
        }

        seek(new_pos);
    }

    void write(std::span<const char> data)
    {
        if (write_pos + data.size() > file_size) { expand(write_pos + data.size()); }
        std::ranges::copy(data, mmap_data.mapped_ptr + write_pos);
        write_pos += data.size();
        max_write_pos = std::max(max_write_pos, write_pos);
    }

    void pwrite(std::span<const char> data, size_t offset)
    {
        if (offset + data.size() > file_size) { expand(offset + data.size()); }
        std::ranges::copy(data, mmap_data.mapped_ptr + offset);
        max_write_pos = std::max(max_write_pos, offset + data.size());
    }

    void flush(bool async = false) { mmap_data.sync(async); }
};
