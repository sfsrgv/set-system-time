#include <iostream>
#include <string>
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <fstream>

using namespace std;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

struct Date {
    int year;
    std::string month;
    int day;
    std::string week_day;
    int hour;
    int minute;
    int second;
};

std::ostream &operator<<(std::ostream &os, const Date &date) {
    os << date.day << " " << date.month << " " << date.year << ", "
    << date.week_day << " " << date.hour << ":" << date.minute
    << ":" << date.second;
    return os;
}

std::istream &operator>>(std::istream &is, Date &date) {
    is >> date.week_day
    >> date.day
    >> date.month
    >> date.year
    >> date.hour
    >> date.minute
    >> date.second;
    return is;
}

Date ParseDateFromResponse(const std::string &response) {
    auto start_pos = response.find("Date:");
    auto finish_pos = response.find("GMT", start_pos) - start_pos;
    std::string date_line = response.substr(start_pos + 5, finish_pos);
    std::replace(date_line.begin(), date_line.end(), ',', ' ');
    std::replace(date_line.begin(), date_line.end(), ':', ' ');
    std::istringstream date_stream(date_line);
    Date date;
    date_stream >> date;
    return date;
}


int main(int argc, char *argv[]) {
    std::ofstream os(argv[1]);
    CURL *curl;
    std::string readBuffer;
    curl = curl_easy_init();
    std::string url = "http://google.com";
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HEADER, true);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_perform(curl);
        std::cout << ParseDateFromResponse(readBuffer) << std::endl;
    }
    curl_easy_cleanup(curl);
    return 0;
}