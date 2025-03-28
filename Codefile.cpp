#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <map>
#include <chrono>
#include <condition_variable>
#include <algorithm>

// Configuration
const int NUM_PRODUCERS = 2;
const int NUM_CONSUMERS = 2;
const int TOP_N = 3; // Track top N most congested traffic lights
const int QUEUE_SIZE = 10;

// Shared resources
std::queue<std::string> trafficQueue;
std::mutex queueMutex;
std::mutex dataMutex;
std::condition_variable queueCondVar;
std::map<std::string, int> congestionData;

// Sample traffic data (replaces file reading)
std::vector<std::string> trafficData = {
    "2025-03-28 08:00:00,TL1,5",
    "2025-03-28 08:01:00,TL2,3",
    "2025-03-28 08:02:00,TL1,7",
    "2025-03-28 08:03:00,TL3,4",
    "2025-03-28 08:04:00,TL2,6",
    "2025-03-28 08:05:00,TL1,2",
    "2025-03-28 08:06:00,TL3,8",
    "2025-03-28 08:07:00,TL2,5"
};

// Producer function
void producer(int producerID, std::vector<std::string> dataChunk) {
    for (const std::string &line : dataChunk) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondVar.wait(lock, [] { return trafficQueue.size() < QUEUE_SIZE; });

        trafficQueue.push(line);
        std::cout << "Producer " << producerID << " added: " << line << std::endl;
        
        lock.unlock();
        queueCondVar.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// Consumer function
void consumer(int consumerID) {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (trafficQueue.empty()) break;
        queueCondVar.wait(lock, [] { return !trafficQueue.empty(); });

        std::string data = trafficQueue.front();
        trafficQueue.pop();
        lock.unlock();
        queueCondVar.notify_all();

        std::istringstream ss(data);
        std::string timestamp, lightID, cars;
        std::getline(ss, timestamp, ',');
        std::getline(ss, lightID, ',');
        std::getline(ss, cars, ',');
        int numCars = std::stoi(cars);

        {
            std::lock_guard<std::mutex> dataLock(dataMutex);
            congestionData[lightID] += numCars;
        }

        std::cout << "Consumer " << consumerID << " processed: " << data << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// Function to get the top N most congested traffic lights
void getTopN() {
    std::vector<std::pair<std::string, int>> sortedData(congestionData.begin(), congestionData.end());
    std::sort(sortedData.begin(), sortedData.end(), [](auto &a, auto &b) {
        return a.second > b.second;
    });

    std::cout << "Top " << TOP_N << " congested traffic lights:\n";
    for (int i = 0; i < std::min(TOP_N, (int)sortedData.size()); ++i) {
        std::cout << sortedData[i].first << ": " << sortedData[i].second << " cars\n";
    }
}

int main() {
    // Split data among producers
    int chunkSize = trafficData.size() / NUM_PRODUCERS;
    std::vector<std::vector<std::string>> dataChunks(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        dataChunks[i] = std::vector<std::string>(trafficData.begin() + i * chunkSize,
                                                 trafficData.begin() + (i + 1) * chunkSize);
    }
    if (trafficData.size() % NUM_PRODUCERS != 0) {
        dataChunks.back().insert(dataChunks.back().end(),
                                 trafficData.begin() + NUM_PRODUCERS * chunkSize, trafficData.end());
    }

    // Start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer, i, dataChunks[i]);
    }

    // Start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer, i);
    }

    // Wait for all threads to finish
    for (auto &p : producers) p.join();
    for (auto &c : consumers) c.join();

    // Display top congested traffic lights
    getTopN();

    return 0;
}
