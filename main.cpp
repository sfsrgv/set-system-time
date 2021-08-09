#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <unordered_map>


const std::unordered_map<std::string, int> WDAYS_TO_NUMBER = {{"Mon",  0},
                                                              {"Thu",  1},
                                                              {"Wed",  2},
                                                              {"Thur", 3},
                                                              {"Fri",  4},
                                                              {"Sat",  5},
                                                              {"Sun",  6}};
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

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

struct tm ParseDateFromResponse(const std::string &response) {
    auto start_pos = response.find("Date:");
    auto finish_pos = response.find("GMT", start_pos) - start_pos;
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
    return current_time;
}

void SetDate(struct tm &current_time) {
    struct timeval systime{};
    char *text_time;

    systime.tv_sec = mktime(&current_time);
    systime.tv_usec = 0;

    settimeofday(&systime, nullptr);
    gettimeofday(&systime, nullptr);

    text_time = ctime(&systime.tv_sec);
    printf("The system time is set to %s\n", text_time);
}

int main(int argc, char *argv[]) {
    std::ofstream os(argv[1]);
    CURL *curl;
    std::string readBuffer;
    curl = curl_easy_init();
    std::string url = "http://google.com";
    struct tm date{};
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HEADER, true);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_perform(curl);
        date = ParseDateFromResponse(readBuffer);
    }
    curl_easy_cleanup(curl);

    SetDate(date);
    return 0;
}