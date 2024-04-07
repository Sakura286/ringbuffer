#include <array>
#include <chrono>
#include <iostream>
#include <iterator>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <atomic>

std::mutex m;

constexpr size_t BUF_SIZE = 15;
constexpr size_t MAX_SIZE = 10;
constexpr size_t MIN_SIZE = 5;

// milliseconds
constexpr long DURATION = 100;

std::array<uint8_t, BUF_SIZE> buffers;

auto const it_start = buffers.begin();
auto it_front = it_start;
auto it_rear = it_start;
auto const it_end = buffers.end();
std::atomic<bool> can_write = true;

std::atomic<bool> reader_exit = false;


void echoArray()
{
    for (auto it: buffers)
    {
        std::cout << it;
    }
    std::cout << std::endl;
    std::array<char, 51> rf = {0};
    for (int i = 0; i < 51; i++)
    {
        rf.at(i) = ' ';
    }
    rf.at(it_rear - it_start) = 'r';
    if (rf.at(it_front - it_start) == 'r')
    {
        rf.at(it_front - it_start) = '*';
    } else
    {
        rf.at(it_front - it_start) = 'f';
    }

    for (auto it: rf)
    {
        std::cout << it;
    }
    std::cout << std::endl;
}

void send(std::ofstream &ofs, size_t output_size)
{
    std::ostream_iterator<std::uint8_t> it_output(ofs);
    if (it_front + output_size <= it_end)
    {
        std::copy(it_front, it_front + output_size, it_output);
    } else
    {
        std::copy(it_front, it_end, it_output);
        std::copy(it_start, it_start + MIN_SIZE - (it_end - it_front), it_output);
    }
    it_front = it_start + (it_front + output_size - it_start) % BUF_SIZE;
    ofs.flush();
}

void writer()
{

    std::ifstream ifs;
    std::string input_path = "/dev/stdin";
    ifs.open(input_path, std::ios::in | std::ios::binary);

    if (!ifs.is_open())
    {
        std::cerr << "ERROR: cannot open " << input_path << std::endl;
    }

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::microseconds {1});
        {
            std::lock_guard<std::mutex> guard(m);
            if (can_write)
            {
                if (ifs.eof())
                {
                    reader_exit = true;
                    break;
                }
                // Write MAX_SIZE as far as possible.
                std::streamsize input_size = static_cast<std::streamsize>(
                        std::min(static_cast<size_t>
                                 (it_end - it_rear), MAX_SIZE));
                ifs.read(reinterpret_cast<char *>(&*it_rear), input_size);

                //std::cout << "Write " << input_size << " bytes" << std::endl;
                it_rear += input_size;

                if (it_rear == it_end)
                {
                    it_rear = it_start;
                }
                //echoArray();
            }
        }
        std::this_thread::yield();
    }
    ifs.close();
}


void reader()
{
    std::ofstream ofs;
    ofs.open("/dev/null", std::ios::out | std::ios::app | std::ios::binary);
    auto start = std::chrono::system_clock::now();
    while (true)
    {
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        {
            std::lock_guard<std::mutex> guard(m);

            size_t data_size = (it_rear - it_front + BUF_SIZE) % BUF_SIZE;

            if (data_size >= MAX_SIZE)
            {
                can_write = false;
            } else if (data_size < MIN_SIZE)
            {
                can_write = true;
            }

            if (data_size >= MIN_SIZE)
            {
                size_t output_size = MIN_SIZE;
                send(ofs, output_size);
                start = std::chrono::system_clock::now();
                //std::cout << "sent " << output_size << " bytes data.A" << std::endl;
                //echoArray();
            } else if (std::chrono::duration_cast<std::chrono::milliseconds>
                               (std::chrono::system_clock::now() - start).count() > DURATION
                       && data_size > 0)
            {

                size_t output_size = data_size;
                send(ofs, output_size);
                start = std::chrono::system_clock::now();
                //std::cout << "sent " << output_size << " bytes data.B" << std::endl;
                //echoArray();
            }

            if (reader_exit && std::chrono::duration_cast<std::chrono::milliseconds>
                                       (std::chrono::system_clock::now() - start).count() > DURATION)
            {
                break;
            }
        }
        std::this_thread::yield();
    }
    ofs.close();
}

int main()
{

    std::thread th_writer(writer);
    std::thread th_reader(reader);

    th_writer.join();
    th_reader.join();

    std::cout << "Execution Finished." << std::endl;
    return 0;
}
