#include "../mmap_reader.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

void test_open_and_close()
{
    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << "Hello, mmap_reader!\nThis is a test file.\n";
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);
    assert(reader.is_open());
    reader.close();
    assert(!reader.is_open());

    std::filesystem::remove(test_file);
}

void test_data_and_size()
{
    std::string test_data = "Hello, mmap_reader!\nThis is a test file.\n";

    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << test_data;
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);
    assert(reader.is_open());
    assert(reader.size() == test_data.size());
    assert(reader.view() == "Hello, mmap_reader!\nThis is a test file.\n");

    reader.close();
    std::filesystem::remove(test_file);
}

void test_seek_and_tell()
{
    std::string test_data = "Hello, mmap_reader!\nThis is a test file.\n";

    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << test_data;
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);
    reader.seek(7);
    assert(reader.tell() == 7);

    reader.seek(5, mmap_reader::seekdir::cur);
    assert(reader.tell() == 12);

    reader.seek(0, mmap_reader::seekdir::cur);
    assert(reader.tell() == 12);

    reader.seek(1024, mmap_reader::seekdir::cur);
    assert(reader.tell() == test_data.size());

    reader.seek(-1024, mmap_reader::seekdir::cur);
    assert(reader.tell() == 0);

    reader.seek(0, mmap_reader::seekdir::beg);
    assert(reader.tell() == 0);

    reader.seek(-10, mmap_reader::seekdir::beg);
    assert(reader.tell() == 0);

    reader.seek(1024, mmap_reader::seekdir::beg);
    assert(reader.tell() == test_data.size());

    reader.seek(0, mmap_reader::seekdir::end);
    assert(reader.tell() == test_data.size());

    reader.seek(1, mmap_reader::seekdir::end);
    assert(reader.tell() == test_data.size());

    reader.seek(-5, mmap_reader::seekdir::end);
    assert(reader.tell() == test_data.size() - 5);

    reader.seek(-1024, mmap_reader::seekdir::end);
    assert(reader.tell() == 0);

    reader.close();
    std::filesystem::remove(test_file);
}

void test_read_and_pread()
{
    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << "Hello, mmap_reader!\nThis is a test file.\n";
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);
    std::vector<char> buffer(5);
    size_t read_bytes = reader.read(buffer);
    assert(read_bytes == 5);
    assert(std::string(buffer.data(), read_bytes) == "Hello");

    reader.seek(0);
    read_bytes = reader.pread(buffer, 7);
    assert(read_bytes == 5);
    assert(std::string(buffer.data(), read_bytes) == "mmap_");

    reader.close();
    std::filesystem::remove(test_file);
}

void test_getline_and_getchar()
{
    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << "Hello, mmap_reader!\nThis is a test file.\n";
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);

    auto line = reader.getline();
    assert(line.has_value());
    assert(line.value() == "Hello, mmap_reader!");

    auto ch = reader.getchar();
    assert(ch.has_value());
    assert(ch.value() == 'T');

    reader.close();
    std::filesystem::remove(test_file);
}

void test_lines_and_chars()
{
    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << "Hello, mmap_reader!\nThis is a test file.\n";
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);

    auto lines = reader.lines();
    auto it = lines.begin();
    assert(*it == "Hello, mmap_reader!");
    ++it;
    assert(*it == "This is a test file.");

    reader.seek(0);

    auto chars = reader.chars();
    auto char_it = chars.begin();
    assert(*char_it == 'H');
    ++char_it;
    assert(*char_it == 'e');

    reader.close();
    std::filesystem::remove(test_file);
}

void test_view_and_str()
{
    const std::filesystem::path test_file = "test_file.txt";
    std::ofstream ofs(test_file);
    ofs << "Hello, mmap_reader!\nThis is a test file.\n";
    ofs.close();

    mmap_reader reader;
    reader.open(test_file);

    auto view = reader.view();
    assert(view == "Hello, mmap_reader!\nThis is a test file.\n");

    auto str = reader.str();
    assert(str == "Hello, mmap_reader!\nThis is a test file.\n");

    auto partial_view = reader.view(7, 5);
    assert(partial_view == "mmap_");

    auto partial_str = reader.str(7, 5);
    assert(partial_str == "mmap_");

    reader.close();
    std::filesystem::remove(test_file);
}

int main()
{
    test_open_and_close();
    test_data_and_size();
    test_seek_and_tell();
    test_read_and_pread();
    test_getline_and_getchar();
    test_lines_and_chars();
    test_view_and_str();

    std::cout << "All tests passed!" << std::endl;

    mmap_reader reader {"../test.txt"};

    std::cout << "view:" << '\n';
    std::string_view content_view = reader.view();
    std::cout << content_view << '\n';

    std::cout << "str:" << '\n';
    std::string content_str = reader.str();
    std::cout << content_str << '\n';

    std::cout << "lines:" << '\n';
    for (std::string_view line : reader.lines()) { std::cout << line << '\n'; }

    reader.seek(0); // Reset to beginning

    std::cout << "chars:" << '\n';
    for (char c : reader.chars()) { std::cout << c; }

    reader.seek(0); // Reset to beginning

    std::cout << "10 bytes:" << '\n';
    std::string str(10, '\0');
    reader.read(str);
    std::cout << str << '\n';

    std::cout << "10 bytes, offset = 6:" << '\n';
    reader.pread(str, 6);
    std::cout << str << '\n';
    for (char c : str) { std::cout << c << '-'; }
}
