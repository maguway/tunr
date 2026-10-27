#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdio>

class SerialPortManager
{
public:
    SerialPortManager();
    ~SerialPortManager();
    void setParseMarker(bool enable);

    std::vector<std::string> getAvailablePorts();

    bool open(const std::string& portName, int baudRate = 115200);
    void close();
    bool isOpen() const;

    void clearBuffer();

    std::string getFormattedData();
    std::vector<unsigned char> getRawData();
    size_t totalBytesReceived() const;

    float getCurrentAngle() const;
    unsigned int getRawValue() const;

    void startRecording(const std::string& filename);
    void stopRecording();
    bool isRecording() const;
    void writeLogLine(const std::string& line);

private:
    void readLoop();
    void parseAndFormat();

    void* m_handle = nullptr; // HANDLE
    std::thread m_reader;
    std::atomic<bool> m_running{false};

    mutable std::mutex m_mutex; // защита буфера
    std::vector<unsigned char> m_rawBuffer; // накопленные байты
    std::string m_displayText; // готовый текст для GUI
    size_t m_lastParsePos = 0; // позиция последнего разбора
    std::atomic<size_t> m_totalBytes{0};

    float m_currentAngle = 0.0f;
    unsigned int m_rawValue = 0;
    size_t m_byteCountForNewLine = 0;

    FILE* m_logFile = nullptr;
    std::mutex m_logMutex;
    std::atomic<bool> m_useMarker{true};
};
