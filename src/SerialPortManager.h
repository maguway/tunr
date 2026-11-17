#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdio>

enum class ParseMode {
    Marker,      // Старый режим: ## + High + Low (для старого Dev1)
    Raw16Bit,    // 2 байта (High + Low) (новый Dev1, если не float)
    RawFloat,    // 4 байта float (новый Dev1, как на скрине)
    RawOnly      // Просто накапливать сырые данные (для Dev2, как было раньше)
};

class SerialPortManager {
public:
    SerialPortManager();
    ~SerialPortManager();

    // Новые методы для управления режимом
    void setParseMode(ParseMode mode);
    void setParseMarker(bool enable); // Оставим для совместимости
    std::vector<std::string> getAvailablePorts();
    bool open(const std::string &portName, int baudRate = 115200);
    void close();
    bool isOpen() const;
    void clearBuffer();
    std::string getFormattedData();
    std::vector<unsigned char> getRawData();
    size_t totalBytesReceived() const;
    float getCurrentAngle() const;
    unsigned int getRawValue() const;
    void startRecording(const std::string &filename);
    void stopRecording();
    bool isRecording() const;
    void writeLogLine(const std::string &line);

private:
    void readLoop();

    void parseAndFormat();

    void *m_handle = nullptr;
    std::thread m_reader;
    std::atomic<bool> m_running{false};

    mutable std::mutex m_mutex;
    std::vector<unsigned char> m_rawBuffer;
    std::string m_displayText;
    size_t m_lastParsePos = 0;
    std::atomic<size_t> m_totalBytes{0};

    float m_currentAngle = 0.0f;
    unsigned int m_rawValue = 0;
    size_t m_byteCountForNewLine = 0;

    FILE *m_logFile = nullptr;
    std::mutex m_logMutex;
    ParseMode m_parseMode = ParseMode::Marker; // По умолчанию - старый режим
};
