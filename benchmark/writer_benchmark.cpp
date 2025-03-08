#include "../mmap_writer.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

void bench(const string& filename, size_t size)
{
    vector<string> lines;
    for (size_t i = 0; i < size; ++i) { lines.emplace_back(format("This is line {}\n", i)); }

    auto start = chrono::high_resolution_clock::now();
    {
        ofstream out(filename);
        for (const auto& str : lines) { out << str; }
    }
    auto end = chrono::high_resolution_clock::now();
    auto fstream_duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    start = chrono::high_resolution_clock::now();
    {
        mmap_writer writer(filename, true);
        size_t total_size = 0;
        for (const auto& str : lines) { total_size += str.size(); }
        writer.reserve(total_size);

        for (const auto& str : lines) { writer.write(str); }
    }
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
    const size_t num_lines = 100'0000;

    bench(filename, num_lines);

    filesystem::remove(filename);
}
