#include "SessionLogger.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {

// Текущее локальное время в виде строки. std::localtime не потокобезопасна,
// поэтому вызывается только под мьютексом логгера (единственное место использования).
std::string formatLocalTime(const char *fmt) {
    std::time_t now_c = std::time(nullptr);
    const std::tm *parts = std::localtime(&now_c);
    if (!parts) return "unknown";
    char buffer[80] = {0};
    if (std::strftime(buffer, sizeof(buffer), fmt, parts) == 0) return "unknown";
    return buffer;
}

// Если файл от предыдущей сессии уже существует и не пуст, переименовываем его,
// чтобы новая запись не затирала старые данные. Любая ошибка здесь не критична:
// в худшем случае поведение будет таким же, как раньше (файл перезапишется).
void backupExistingFile(const std::string &filename) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p(filename);

    if (!fs::exists(p, ec) || ec) return;
    const auto size = fs::file_size(p, ec);
    if (ec || size == 0) return;

    const std::string stamp = formatLocalTime("%Y%m%d_%H%M%S");
    const fs::path dir = p.parent_path();
    const std::string stem = p.stem().string();
    const std::string ext = p.extension().string();

    fs::path backup = dir / (stem + "_" + stamp + ext);
    for (int n = 1; fs::exists(backup, ec) && n < 1000; ++n) {
        backup = dir / (stem + "_" + stamp + "_" + std::to_string(n) + ext);
    }
    fs::rename(p, backup, ec);
}

} // namespace

SessionLogger::SessionLogger() = default;

SessionLogger::~SessionLogger() {
    stop();
}

bool SessionLogger::start(const std::string &filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_is_recording) return false; // уже пишем

    m_last_error.clear();
    backupExistingFile(filename);

    m_file.clear(); // сбрасываем флаги ошибок после возможной прошлой неудачи
    m_file.open(filename);
    if (!m_file.is_open()) {
        m_last_error = "Cannot open " + filename;
        return false;
    }

    m_start_time = std::chrono::steady_clock::now();
    writeHeader();
    m_is_recording = true;
    return true;
}

void SessionLogger::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_is_recording) {
        m_is_recording = false;
    }
    if (m_file.is_open()) {
        m_file.close();
    }
}

std::string SessionLogger::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_error;
}

void SessionLogger::writeHeader() {
    // Печатаем текущую дату и время для шапки
    const std::string buffer = formatLocalTime("%Y-%m-%d %H:%M:%S");

    m_file << "# =========================================================\n";
    m_file << "# TURN - DUAL MONITOR SESSION LOG\n";
    m_file << "# Date: " << buffer << "\n";
    m_file << "# =========================================================\n\n";

    // Заголовки колонок
    m_file << "Time(s); Dev1_Angle; Dev2_Angle; Dev2_X; Dev2_Y; Dev2_Dist; Dev2_Power; Dev2_Res\n";
    m_file.flush();
}

void SessionLogger::logData(int device_id, float angle1, const std::vector<float> &dev2_vals) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_is_recording || !m_file.is_open()) return;

    // Вычисляем время с начала записи в секундах (double: миллисекунды не теряются на длинных сессиях)
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - m_start_time;
    const double time_s = elapsed.count();

    // Формируем строку с фиксированной шириной для красоты
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << std::setw(8) << time_s << ";";

    if (device_id == 1) {
        // Пишем угол первого устройства, остальные колонки пустые
        oss << std::setprecision(2) << std::setw(10) << angle1 << ";";
        oss << "          ;          ;          ;          ;          ;          ";
    } else if (device_id == 2 && dev2_vals.size() >= 6) {
        // Пишем данные второго устройства
        oss << "          ;"; // Dev1_Angle пустой
        oss << std::setprecision(2) << std::setw(10) << dev2_vals[0] << ";"; // Angle
        oss << std::setprecision(4) << std::setw(10) << dev2_vals[1] << ";"; // X
        oss << std::setprecision(4) << std::setw(10) << dev2_vals[2] << ";"; // Y
        oss << std::setprecision(4) << std::setw(10) << dev2_vals[3] << ";"; // Dist
        // Power: на вкладке Monitor он выводится как %.2e, значит может быть очень малым/большим.
        // Формат %.2f превратил бы такие значения в 0.00, поэтому здесь общий формат (6 значащих цифр).
        // Для обычных значений (например 55.3) число читается так же, как раньше.
        oss << std::defaultfloat << std::setprecision(6) << std::setw(10) << dev2_vals[4] << ";"; // Power
        oss << std::fixed << std::setprecision(2) << std::setw(10) << dev2_vals[5];               // Res
    }

    m_file << oss.str() << "\n";
    m_file.flush(); // Сбрасываем на диск, чтобы при сбое питания данные не потерялись

    if (!m_file) {
        // Диск полон / файл недоступен: корректно останавливаем запись и сообщаем об ошибке
        m_last_error = "Write to session log failed";
        m_is_recording = false;
        m_file.close();
    }
}
