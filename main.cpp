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
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

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

int PORT = 80;

std::mutex mutex;
std::condition_variable condition;
std::string message;
bool is_ready = false;
bool is_processed = false;
bool is_end = false;
std::string filename;

void LoggerThread() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        if (is_end) {
            break;
        }
        condition.wait(lock, [] {
            return is_ready || is_end;
        });
        is_ready = false;

        std::ofstream os(filename, std::ios_base::out | std::ios_base::app);
        os << message;

        is_processed = true;
        lock.unlock();
        condition.notify_one();
    }
}

void SendMessage(std::thread &thread, const std::string& line, bool is_last = false) {
    message = line;
    {
        std::lock_guard<std::mutex> lock(mutex);
        is_ready = true;
    }
    condition.notify_one();
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [] {
            return is_processed;
        });
        is_end = is_last;
        is_processed = false;
        condition.notify_one();
    }

}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

struct tm ParseDateFromResponse(const std::string &response, std::thread &logger){
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
        SendMessage(logger, "Date successfully parsed. Got date: " + std::string(ctime(&time)));
        return current_time;
    } else {
        SendMessage(logger, "Error while parsing HTTP response. No field 'Date' was found");
        return {};
    }
}

void SetDate(struct tm &current_time, std::thread &logger) {
    struct timeval systime{};
    systime.tv_sec = mktime(&current_time);
    systime.tv_usec = 0;
    if (settimeofday(&systime, nullptr) == 0) {
        SendMessage(logger, "Time was set: " + std::string(ctime(&systime.tv_sec)));
    } else {
        SendMessage(logger, "Error while setting time.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cout << "No filename was entered." << std::endl;
    } else {
        filename = argv[1];
        std::cout << filename << std::endl;
        std::thread logger(LoggerThread);
        struct timeval systime{};
        gettimeofday(&systime, nullptr);
        SendMessage(logger, "Current time on machine: " + std::string(ctime(&systime.tv_sec)));

        struct hostent *host = gethostbyname("google.com");

        if (host == nullptr || host->h_addr == nullptr) {
            SendMessage(logger, "Error retrieving DNS information.\n", true);
            return 1;
        }
        struct sockaddr_in client{};
        bzero(&client, sizeof(client));
        client.sin_family = AF_INET;
        client.sin_port = htons(PORT);
        memcpy(&client.sin_addr, host->h_addr, host->h_length);
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            SendMessage(logger, "Error creating socket.\n", true);
            return 1;
        }

        if (connect(sock, (struct sockaddr *) &client, sizeof(client)) < 0) {
            close(sock);
            SendMessage(logger, "Could not connect\n", true);
            return 1;
        }

        std::string request = "HEAD / 550 HTTP/1.1\r\n\r\n";

        if (send(sock, request.c_str(), request.length(), 0) != (int) request.length()) {
            SendMessage(logger, "Error sending request.\n", true);
            return 1;
        }

        char sym;
        std::stringstream response;
        while (read(sock, &sym, 1) > 0) {
            response << sym;
        }

        struct tm date = ParseDateFromResponse(response.str(), logger);
        SetDate(date, logger);

        CURL *curl = curl_easy_init();
        std::string readBuffer;
        std::string url = "https://example.com";
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_perform(curl);
            SendMessage(logger, "Response from " + url + ":\n" + readBuffer + "\n", true);
        } else {
            SendMessage(logger, "Error while initializing curl to " + url + "\n", true);
        }
        logger.join();
    }
    return 0;
}