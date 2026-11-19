#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

// Общий лог сессии: оба устройства пишутся в один CSV-файл (разделитель ';').
// Формат файла не менялся: шапка, колонки Time(s); Dev1_Angle; Dev2_*.
class SessionLogger {
public:
    SessionLogger();
    ~SessionLogger();

    SessionLogger(const SessionLogger &) = delete;
    SessionLogger &operator=(const SessionLogger &) = delete;

    // Начать запись. Если файл с таким именем уже есть, он НЕ затирается,
    // а переименовывается в <имя>_ГГГГММДД_ЧЧММСС.<расширение>.
    bool start(const std::string &filename);
    void stop();

    // Не блокирует (atomic), можно вызывать из любого потока.
    bool isRecording() const { return m_is_recording.load(); }

    // device_id = 1 -> пишется angle1, device_id = 2 -> пишутся dev2_vals (6 значений).
    void logData(int device_id, float angle1, const std::vector<float> &dev2_vals);

    // Текст последней ошибки (пустой, если всё хорошо).
    std::string lastError() const;

private:
    void writeHeader();

    std::ofstream m_file;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_is_recording{false};
    std::chrono::steady_clock::time_point m_start_time;
    std::string m_last_error;
};
