#pragma once

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SessionLogger;

enum class ParseMode {
    Marker,   // Старый режим: ## + High + Low (для старого Dev1)
    Raw16Bit, // 2 байта (High + Low) (новый Dev1, если не float)
    RawFloat, // 4 байта float (новый Dev1, как на скрине)
    RawOnly   // Просто накапливать сырые данные (для Dev2, как было раньше)
};

class SerialPortManager {
public:
    SerialPortManager();
    ~SerialPortManager();

    SerialPortManager(const SerialPortManager &) = delete;
    SerialPortManager &operator=(const SerialPortManager &) = delete;

    // Режим парсинга можно менять в любой момент (потокобезопасно).
    void setParseMode(ParseMode mode) { m_parseMode = mode; }
    // Оставлено для совместимости: false -> RawOnly, true -> Marker.
    void setParseMarker(bool enable) { m_parseMode = enable ? ParseMode::Marker : ParseMode::RawOnly; }

    std::vector<std::string> getAvailablePorts();

    bool open(const std::string &portName, int baudRate = 115200);
    void close();

    // true только пока порт открыт И связь не потеряна.
    bool isOpen() const;
    // true, если порт был открыт, но устройство пропало (отключили кабель и т.п.).
    bool connectionLost() const;
    // Текст последней ошибки открытия/связи (пусто, если ошибок не было).
    std::string lastError() const;

    void clearBuffer();

    std::string getFormattedData();
    // Полная копия накопленного буфера (оставлено для совместимости; для UI лучше getTailBytes).
    std::vector<unsigned char> getRawData();
    // Последние `count` байт буфера одной короткой копией.
    // Если в буфере меньше `count` байт, возвращает пустой вектор.
    std::vector<unsigned char> getTailBytes(size_t count);
    size_t totalBytesReceived() const { return m_totalBytes; }

    float getCurrentAngle() const { return m_currentAngle; }
    unsigned int getRawValue() const { return m_rawValue; }

    // Запись "сырого" лога конкретного устройства (старые кнопки Rec/Stop)
    void startRecording(const std::string &filename);
    void stopRecording();
    bool isRecording() const;
    void writeLogLine(const std::string &line);

    // Подключение общего логгера сессии.
    // deviceId: 1 = Dev1 (угол), 2 = Dev2 (пакет из 6 float), 0 = определить по режиму парсинга
    // (RawFloat -> 1, RawOnly -> 2). Вызывать до open().
    void setSessionLogger(SessionLogger *logger, int deviceId = 0) {
        m_logger = logger;
        m_deviceId = deviceId;
    }

private:
    void readLoop();
    void parseAndFormat();                 // вызывать под m_mutex
    void consumeToDisplay(size_t count);   // вызывать под m_mutex
    void markLost(unsigned long errorCode);
    void setError(const std::string &text);

    void *m_handle = nullptr;
    std::thread m_reader;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_lost{false};

    mutable std::mutex m_mutex;
    std::vector<unsigned char> m_rawBuffer;
    std::string m_displayText;
    std::atomic<size_t> m_totalBytes{0};

    std::atomic<float> m_currentAngle{0.0f};
    std::atomic<unsigned int> m_rawValue{0};
    size_t m_byteCountForNewLine = 0; // под m_mutex
    bool m_angleUpdated = false;      // под m_mutex: в последнем куске данных найден новый угол
    size_t m_logPendingBytes = 0;     // под m_mutex: сколько байт пришло с момента последней записи в лог сессии

    FILE *m_logFile = nullptr;
    mutable std::mutex m_logMutex;

    std::atomic<ParseMode> m_parseMode{ParseMode::Marker};

    std::atomic<SessionLogger *> m_logger{nullptr};
    std::atomic<int> m_deviceId{0};

    mutable std::mutex m_errMutex;
    std::string m_lastError;
};
