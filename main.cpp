#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <fstream>
#include <map>
#include <algorithm>

std::mutex mutex;
std::map<std::string, std::map<std::string, size_t>> logCounts;

namespace fs = std::filesystem;

std::vector<std::string> getFilesRecursive(const std::string &folderPath) {
    std::vector<std::string> filePaths;

    for (const auto &entry: fs::directory_iterator(folderPath)) {
        if (entry.is_directory()) {
            auto subdirectoryFiles = getFilesRecursive(entry.path().string());
            filePaths.insert(filePaths.end(), subdirectoryFiles.begin(), subdirectoryFiles.end());
        } else if (entry.is_regular_file()) {
            filePaths.push_back(entry.path().string());
        }
    }

    return filePaths;
}

void getLogCounts(const std::string &logFilePath) {
    std::ifstream logFile(logFilePath);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        return;
    }

    std::map<std::string, std::map<std::string, size_t>> fileLogCounts;

    std::string line;
    while (std::getline(logFile, line)) {
        size_t timestampStart = line.find('[') + 1;
        size_t timestampEnd = line.find(']', timestampStart);
        std::string timestamp = line.substr(timestampStart, timestampEnd - timestampStart);

        size_t levelStart = line.find('[', timestampEnd + 1) + 1;
        size_t levelEnd = line.find(']', levelStart);
        std::string level = line.substr(levelStart, levelEnd - levelStart);

        size_t nameStart = line.find('[', levelEnd + 1) + 1;
        size_t nameEnd = line.find(']', nameStart);
        std::string name = line.substr(nameStart, nameEnd - nameStart);

        std::string message = line.substr(nameEnd + 2);

        fileLogCounts[name][level]++;
    }

    logFile.close();

    mutex.lock();

    for (auto &[prog_name, map]: fileLogCounts) {
        for (auto &[type, amount]: map) {
            logCounts[prog_name][type] += amount;
        }
    }

    mutex.unlock();
}

int main() {
    std::string directoryPath;
    std::cout << "Enter directory path: ";
    std::getline(std::cin, directoryPath);

    auto files = getFilesRecursive(directoryPath);

    std::vector<std::thread> threads;

    for (const auto &file_path: files) {
        if(!file_path.ends_with(".log"))continue;
        threads.emplace_back(getLogCounts, file_path);
    }

    for (auto &thread: threads) {
        thread.join();
    }

    std::vector<std::pair<size_t, std::string>> progOrder;
    for (auto &[key, map]: logCounts) {
        size_t sum = 0;
        for (auto &[type, amount]: map) {
            sum += amount;
        }
        progOrder.emplace_back(sum, key);
    }

    std::sort(progOrder.begin(), progOrder.end());
    std::reverse(progOrder.begin(), progOrder.end());

    std::cout << std::endl;
    std::cout << "Process\tTrace\tDebug\tInfo\tWarn\tError" << std::endl;
    for (auto &[count, prog_name]: progOrder) {
        std::cout << prog_name << "\t";
        std::cout << logCounts[prog_name]["Trace"] << "\t";
        std::cout << logCounts[prog_name]["Debug"] << "\t";
        std::cout << logCounts[prog_name]["Info"] << "\t";
        std::cout << logCounts[prog_name]["Warn"] << "\t";
        std::cout << logCounts[prog_name]["Error"] << std::endl;
    }

    return 0;
}
