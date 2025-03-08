#include "../mmap_reader.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

void generate_test_file(const string& filename, size_t size)
{
    ofstream out(filename);
    for (size_t i = 0; i < size; ++i) { out << "This is line " << i << "\n"; }
}

void benchmark_whole_file_read(const string& filename)
{
    cout << "\nwhole file read:\n";

    auto start = chrono::high_resolution_clock::now();
    ifstream in(filename, ios::binary);
    in.seekg(0, ios::end);
    size_t file_size = in.tellg();
    in.seekg(0, ios::beg);
    string content(file_size, '\0');
    in.read(&content[0], file_size);
    auto end = chrono::high_resolution_clock::now();
    auto fstream_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    start = chrono::high_resolution_clock::now();
    mmap_reader mmapReader(filename);
    string mmap_content = mmapReader.str();
    end = chrono::high_resolution_clock::now();
    auto mmap_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "fstream time: " << fstream_duration << " ms\n";
    cout << "mmap time: " << mmap_duration << " ms\n";
    cout << "ratio (mmap/fstream): " << static_cast<double>(mmap_duration) / fstream_duration
         << "\n";
}

void benchmark_line_read(const string& filename)
{
    cout << "\nTesting line read:\n";

    auto start = chrono::high_resolution_clock::now();
    ifstream in(filename);
    string line;
    string content;
    while (getline(in, line)) { content += line + '\n'; }
    auto end = chrono::high_resolution_clock::now();
    auto fstream_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    start = chrono::high_resolution_clock::now();
    mmap_reader mmapReader(filename);
    string mmap_content;
    for (string_view line : mmapReader.lines())
    {
        mmap_content += line;
        mmap_content += '\n';
    }
    end = chrono::high_resolution_clock::now();
    auto mmap_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "fstream time: " << fstream_duration << " ms\n";
    cout << "mmap time: " << mmap_duration << " ms\n";
    cout << "ratio (mmap/fstream): " << static_cast<double>(mmap_duration) / fstream_duration
         << "\n";
}

void benchmark_char_read(const string& filename)
{
    cout << "\nTesting char read:\n";

    auto start = chrono::high_resolution_clock::now();
    ifstream in(filename);
    char ch;
    string content;
    while (in.get(ch)) { content += ch; }
    auto end = chrono::high_resolution_clock::now();
    auto fstream_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    start = chrono::high_resolution_clock::now();
    mmap_reader mmapReader(filename);
    string mmap_content;
    for (char ch : mmapReader.chars()) { mmap_content += ch; }
    end = chrono::high_resolution_clock::now();
    auto mmap_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "fstream time: " << fstream_duration << " ms\n";
    cout << "mmap time: " << mmap_duration << " ms\n";
    cout << "ratio (mmap/fstream): " << static_cast<double>(mmap_duration) / fstream_duration
         << "\n";
}

int main()
{
    const string filename = "large_lines.txt";
    const size_t num_lines = 1000000;

    generate_test_file(filename, num_lines);

    benchmark_whole_file_read(filename);
    benchmark_line_read(filename);
    benchmark_char_read(filename);

    filesystem::remove(filename);
}
