#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

#include "include/curl/curl.h"

#define LOG(x) cout << (x) << endl
#define LOGV(x) cout << #x " = " << (x) << endl
#define CHUNK_SIZE 1024*1024*10

using namespace std;

mutex g_thread_mutex;
mutex g_done_mutex;
mutex g_write_mutex;
bool g_done = false;
int g_num_threads = 0;
int g_thread_no = 0;

struct Writer {
    FILE *m_file;
    int m_thread_no;

    Writer(FILE *file, int thread_no) {
        m_file = file;
        m_thread_no = thread_no;
    }
};

bool parseParameters(int argc, char **argv, string& url, int& threads, int& conns, string& out) {
    if (argc < 3) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        string param = argv[i];
        string dim = "=";
        int pos = param.find(dim);

        if (pos == string::npos) {
            return false;
        }

        string key = param.substr(0, pos);
        string value = param.substr(pos + 1);

        if (key == "--url") {
            url = value;
        }
        else if (key == "--thread") {
            threads = stoi(value);
        }
        else if (key == "--conn") {
            conns = stoi(value);
        }
        else if (key == "--out") {
            out = value;
        }
        else {
            return false;
        }
    }

    return true;
}

size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    if (size*nmemb == 0) {
        g_done_mutex.lock();
        g_done = true;
        g_done_mutex.unlock();
        return 0;
    }

    Writer *writer = (Writer *)userdata;
    FILE *file = writer->m_file;
    int thread_no = writer->m_thread_no;

    while (thread_no != g_thread_no + 1) {
        this_thread::sleep_for(10ms);
    }

    g_write_mutex.lock();
    size_t written = fwrite(ptr, size, nmemb, file);
    g_write_mutex.unlock();
    
    return written;
}

void getDownloadInfo(CURL* curl) {
    curl_off_t downloadSize, downloadTime, downloadSpeed, nameLookupTime, connectTime;
    CURLcode res;

    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadSize);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &downloadTime);
    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &downloadSpeed);
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &nameLookupTime);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &connectTime);

    LOG("Download size (bytes):");
    LOGV(downloadSize);
    LOG("Download time (minisecond):");
    LOGV(downloadTime / 1000);
    LOG("Download speed (bytes/s):");
    LOGV(downloadSpeed);
    LOG("Name lookup time (minisecond):");
    LOGV(nameLookupTime / 1000);
    LOG("Connect time (minisecond)");
    LOGV(connectTime / 1000);
} 

size_t getFileSize(string out) {
    FILE *file;
    size_t fileSize;

    file = fopen(out.c_str(), "rb");

    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);

    fclose(file);

    return fileSize;
}

curl_off_t getContentLengthDownload(string url) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    // curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    res = curl_easy_perform(curl);
    LOGV(res);

    if (res != CURLE_OK) {
        return false;
    }

    curl_off_t contentLength;

    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);

    return contentLength;
}   

bool downloadAs(string url, int conns, string out, string range = "", int threadNo = 0) {
    LOGV(range);
    FILE *file;
    
    file = fopen(out.c_str(), "ab");

    if (!file) {
        LOGV(errno);
        return false;
    }

    struct Writer writer(file, threadNo);

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    if (!curl) {
        LOGV(errno);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    // curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, long(conns));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&writer);

    res = curl_easy_perform(curl);
    LOGV(res);

    // if (res == CURLE_OK)
    //     getDownloadInfo(curl);

    curl_easy_cleanup(curl);

    fclose(file);
    
    g_thread_mutex.lock();
    --g_num_threads;
    ++g_thread_no;
    g_thread_mutex.unlock();

    if (res != CURLE_OK) {
        g_done = true;
    }

    return (res == CURLE_OK);
}

bool downloadMultipleThreads(string url, int conns, string out, string range, int threadNo) {
    LOGV(threadNo);
    return downloadAs(url, conns, out, range, threadNo);
}

void handleMultipleThreads(string url, int threads, int conns, string out, int beginByte, int endByte) {
    string range;
    int posByte = beginByte, chunk;
    int threadNo = 0;

    while (!g_done && posByte <= endByte) {
        if (g_num_threads < threads) {
            chunk = (endByte - posByte + 1 > CHUNK_SIZE ? CHUNK_SIZE : endByte - posByte + 1);
            LOGV(posByte);
            LOGV(chunk);
            range = to_string(posByte) + "-" + to_string(posByte + chunk - 1);
            LOGV(g_num_threads);

            thread thr(downloadMultipleThreads, url, conns, out, range, ++threadNo);
            thr.detach();

            posByte += chunk;

            g_thread_mutex.lock();
            ++g_num_threads;
            g_thread_mutex.unlock();
        }
        
        this_thread::sleep_for(10ms);
    }

    while (g_num_threads > 0) {
        this_thread::sleep_for(10ms);
    }
}

// ./download --url=<download url> --thread=<number of threads> --conn=<number of connections> --out=<download path>
int main(int argc, char **argv) {
    string url, out;
    int threads, conns;

    threads = conns = 1;

    if (!parseParameters(argc, argv, url, threads, conns, out)) {
        LOG("Failed to parse parameters!");
        return -1;
    }

    LOGV(url);
    LOGV(out);
    LOGV(threads);
    LOGV(conns);

    curl_global_init(CURL_GLOBAL_ALL);

    size_t fileSize = getFileSize(out);
    LOG("Current output file size:");
    LOGV(fileSize);

    curl_off_t contentLength = getContentLengthDownload(url);
    LOGV(contentLength);

    if (contentLength == 0) {
        LOG("Failed to get content length!");
        return -2;
    }

    if (fileSize >= contentLength) {
        LOG("Failed to resume!");
        return -3;
    }

    handleMultipleThreads(url, threads, conns, out, fileSize, contentLength - 1);

    curl_global_cleanup();

    LOG("Success!");
    return 0;
}