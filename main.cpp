#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <unordered_map>
#include <thread>
#include <fstream>

const std::unordered_map<std::string, int> WDAYS_TO_NUMBER = {{"Mon", 0},
                                                              {"Tue", 1},
                                                              {"Wed", 2},
                                                              {"Thu", 3},
                                                              {"Fri", 4},
                                                              {"Sat", 5},
                                                              {"Sun", 6}};
const std::unordered_map<std::string, int> MONTH_TO_NUMBER = {{"Jan",  0},
                                                              {"Feb",  1},
                                                              {"Mar",  2},
                                                              {"Apr",  3},
                                                              {"May",  4},
                                                              {"June", 5},
                                                              {"July", 6},
                                                              {"Aug",  7},
                                                              {"Sep",  8},
                                                              {"Oct",  9},
                                                              {"Nov",  10},
                                                              {"Dec",  11},
};

class Logger {
public:
    Logger(const std::string &filename) : filename_(filename), counter_(0) {
        std::ofstream os(filename_);
    }

    void SendMessage(const std::string &message) {
        std::string message_formatted = std::to_string(counter_) + ": " + message;
        std::thread thread([message_formatted, this]() {
            std::ofstream os(filename_, std::ios_base::out | std::ios_base::app);
            os << message_formatted;
        });
        thread.join();
        ++counter_;
    }

private:
    const std::string filename_;
    int counter_;
};

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

struct tm ParseDateFromResponse(const std::string &response, Logger &logger) {
    auto start_pos = response.find("Date:");
    auto finish_pos = response.find("GMT", start_pos) - start_pos;
    if (start_pos != std::string::npos) {
        std::string date_line = response.substr(start_pos + 5, finish_pos);
        std::replace(date_line.begin(), date_line.end(), ',', ' ');
        std::replace(date_line.begin(), date_line.end(), ':', ' ');
        std::istringstream date_stream(date_line);

        struct tm current_time{};
        std::string buffer;
        date_stream >> buffer;
        current_time.tm_wday = WDAYS_TO_NUMBER.at(buffer);
        date_stream >> current_time.tm_mday;
        date_stream >> buffer;
        current_time.tm_mon = MONTH_TO_NUMBER.at(buffer);
        date_stream >> current_time.tm_year
                    >> current_time.tm_hour
                    >> current_time.tm_min
                    >> current_time.tm_sec;
        current_time.tm_year -= 1900;
        current_time.tm_hour += 3; // for GMT+3 timezone;

        auto time = mktime(&current_time);
        logger.SendMessage("Date successfully parsed. Got date: " + std::string(ctime(&time)));
        return current_time;
    } else {
        logger.SendMessage("Error while parsing HTTP response. No field 'Date' was found");
        return {};
    }
}

void SetDate(struct tm &current_time, Logger &logger) {
    struct timeval systime{};
    systime.tv_sec = mktime(&current_time);
    systime.tv_usec = 0;
    if (settimeofday(&systime, nullptr) == 0) {
        logger.SendMessage("Time was set: " + std::string(ctime(&systime.tv_sec)));
    } else {
        logger.SendMessage("Error while setting time.\n");
    }
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "No filename was entered." << std::endl;
    } else {
        Logger logger(argv[1]);

        struct timeval systime{};
        gettimeofday(&systime, nullptr);
        logger.SendMessage("Current time on machine: " + std::string(ctime(&systime.tv_sec)));

        CURL *curl;
        std::string readBuffer;
        curl = curl_easy_init();
        std::string url = "http://google.com";
        struct tm date{};
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            logger.SendMessage("Made request to " + url + "\n");
            curl_easy_setopt(curl, CURLOPT_HEADER, true);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_perform(curl);
            date = ParseDateFromResponse(readBuffer, logger);
        } else {
            logger.SendMessage("Error while initializing curl to " + url + "\n");
        }
        curl_easy_cleanup(curl);
        SetDate(date, logger);

        curl = curl_easy_init();
        url = "https://example.com";
        readBuffer.erase();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_perform(curl);
            logger.SendMessage("Response from " + url + ":\n" + readBuffer + "\n");
        } else {
            logger.SendMessage("Error while initializing curl to " + url + "\n");
        }
    }
    return 0;
}