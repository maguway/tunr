#include "SerialPortManager.h"
#include "SessionLogger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>
#include <cstring>

namespace {

// Текст ошибки Windows на английском (шрифт ImGui по умолчанию не содержит кириллицу).
std::string winErrorText(DWORD code) {
    wchar_t buf[256] = {0};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                   buf, 255, nullptr);
    std::string text;
    if (n > 0) {
        char out[512] = {0};
        const int len = WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n),
                                            out, static_cast<int>(sizeof(out)) - 1, nullptr, nullptr);
        if (len > 0) text.assign(out, static_cast<size_t>(len));
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n' ||
                                 text.back() == ' ' || text.back() == '.')) {
            text.pop_back();
        }
    }
    if (text.empty()) text = "Error";
    return text + " (code " + std::to_string(static_cast<unsigned long>(code)) + ")";
}

// Ошибки ReadFile, после которых порт точно "мёртв" (устройство отключили и т.п.).
bool isFatalCommError(DWORD err) {
    return err == ERROR_ACCESS_DENIED || err == ERROR_GEN_FAILURE ||
           err == ERROR_DEVICE_NOT_CONNECTED || err == ERROR_FILE_NOT_FOUND ||
           err == ERROR_INVALID_HANDLE;
}

constexpr size_t kPacketBytes = 24; // Dev2: 6 float подряд

} // namespace

SerialPortManager::SerialPortManager() = default;

SerialPortManager::~SerialPortManager() {
    close();
    stopRecording();
}

std::vector<std::string> SerialPortManager::getAvailablePorts() {
    std::vector<std::string> ports;
    typedef DWORD(WINAPI * GetCommPortsFn)(PULONG, ULONG, PULONG);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        auto fn = (GetCommPortsFn) GetProcAddress(hKernel, "GetCommPorts");
        if (fn) {
            ULONG buf[256];
            ULONG count = 0;
            if (fn(buf, 256, &count) == ERROR_SUCCESS) {
                for (ULONG i = 0; i < count; ++i) {
                    ports.push_back("COM" + std::to_string(buf[i]));
                }
                return ports;
            }
        }
    }

    // Запасной вариант для старых систем: пробуем открыть COM1..COM256.
    for (int i = 1; i <= 256; ++i) {
        std::string name = "COM" + std::to_string(i);
        std::wstring wname(name.begin(), name.end());
        std::wstring path = L"\\\\.\\" + wname;
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            ports.push_back(name);
            CloseHandle(h);
        } else if (GetLastError() == ERROR_ACCESS_DENIED) {
            // Порт существует, но уже открыт (в том числе нами) - он не должен пропадать из списка.
            ports.push_back(name);
        }
    }
    return ports;
}

bool SerialPortManager::open(const std::string &portName, int baudRate) {
    close();
    setError(std::string());

    std::wstring wname(portName.begin(), portName.end());
    std::wstring path = L"\\\\.\\" + wname;

    HANDLE h = CreateFileW(path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        setError(winErrorText(GetLastError()));
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        setError(winErrorText(GetLastError()));
        CloseHandle(h);
        return false;
    }
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    // Явно отключаем программное/аппаратное управление потоком и "abort on error":
    // иначе настройки, оставленные другой программой, могут съедать байты 0x11/0x13
    // или блокировать чтение после первой ошибки приёма.
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(h, &dcb)) {
        setError(winErrorText(GetLastError()));
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &timeouts);

    m_lost = false;
    m_handle = h;
    m_running = true;
    m_reader = std::thread(&SerialPortManager::readLoop, this);
    return true;
}

void SerialPortManager::close() {
    m_running = false;
    if (m_reader.joinable())
        m_reader.join();
    if (m_handle) {
        CloseHandle((HANDLE) m_handle);
        m_handle = nullptr;
    }
    m_lost = false;
}

bool SerialPortManager::isOpen() const {
    return m_handle != nullptr && !m_lost;
}

bool SerialPortManager::connectionLost() const {
    return m_handle != nullptr && m_lost;
}

std::string SerialPortManager::lastError() const {
    std::lock_guard<std::mutex> lock(m_errMutex);
    return m_lastError;
}

void SerialPortManager::setError(const std::string &text) {
    std::lock_guard<std::mutex> lock(m_errMutex);
    m_lastError = text;
}

void SerialPortManager::markLost(unsigned long errorCode) {
    setError("Connection lost: " + winErrorText(static_cast<DWORD>(errorCode)));
    m_lost = true;
    m_running = false;
}

void SerialPortManager::clearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rawBuffer.clear();
    m_displayText.clear();
    m_byteCountForNewLine = 0;
    m_logPendingBytes = 0;
}

std::string SerialPortManager::getFormattedData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_displayText;
}

std::vector<unsigned char> SerialPortManager::getRawData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rawBuffer;
}

std::vector<unsigned char> SerialPortManager::getTailBytes(size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (count == 0 || m_rawBuffer.size() < count)
        return {};
    return std::vector<unsigned char>(m_rawBuffer.end() - static_cast<std::ptrdiff_t>(count),
                                      m_rawBuffer.end());
}

void SerialPortManager::readLoop() {
    unsigned char tmp[1024];
    unsigned int consecutiveFailures = 0;
    HANDLE h = (HANDLE) m_handle;

    while (m_running) {
        DWORD read = 0;
        BOOL ok = ReadFile(h, tmp, sizeof(tmp), &read, nullptr);
        if (!ok) {
            const DWORD err = GetLastError();
            // Подтверждаем ошибку приёма (overrun/framing), чтобы драйвер продолжил работу.
            DWORD commErrors = 0;
            COMSTAT stat{};
            ClearCommError(h, &commErrors, &stat);

            if (isFatalCommError(err) || ++consecutiveFailures > 100) {
                markLost(err); // устройство пропало: UI увидит isOpen() == false
                break;
            }
            Sleep(10);
            continue;
        }
        consecutiveFailures = 0;

        if (read == 0) { // таймаут чтения, данных нет
            Sleep(10);
            continue;
        }

        // Старая запись "сырого" лога конкретного устройства (кнопки Rec/Stop)
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            if (m_logFile) {
                for (DWORD i = 0; i < read; ++i) {
                    fprintf(m_logFile, "%02X ", tmp[i]);
                    if ((i + 1) % 16 == 0) fprintf(m_logFile, "\n");
                }
                fflush(m_logFile);
            }
        }

        // Данные для общего лога сессии собираем под мьютексом,
        // а саму запись (с flush на диск) делаем уже ПОСЛЕ его освобождения,
        // чтобы не блокировать UI на время записи файла.
        SessionLogger *logger = m_logger.load();
        const bool sessionActive = (logger != nullptr) && logger->isRecording();
        int logDevice = 0;
        float logAngle = 0.0f;
        std::vector<float> logValues;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_totalBytes += read;
            m_rawBuffer.insert(m_rawBuffer.end(), tmp, tmp + read);
            m_angleUpdated = false;
            m_logPendingBytes += read;
            parseAndFormat();

            if (sessionActive) {
                const ParseMode mode = m_parseMode.load();
                int dev = m_deviceId.load();
                if (dev == 0) {
                    dev = (mode == ParseMode::RawFloat) ? 1 : (mode == ParseMode::RawOnly ? 2 : 0);
                }

                if (dev == 1 && mode != ParseMode::RawOnly) {
                    // Устройство 1: пишем угол, только если в этом куске данных он реально обновился
                    if (m_angleUpdated) {
                        logDevice = 1;
                        logAngle = m_currentAngle.load();
                    }
                } else if (dev == 2 && mode == ParseMode::RawOnly) {
                    // Устройство 2 (пакет 24 байта): пишем, когда накопилось не меньше одного пакета
                    // новых байт. Так неполные куски пакета не превращаются в мусорные строки.
                    if (m_rawBuffer.size() >= kPacketBytes && m_logPendingBytes >= kPacketBytes) {
                        logValues.resize(6);
                        std::memcpy(logValues.data(), &m_rawBuffer[m_rawBuffer.size() - kPacketBytes],
                                    kPacketBytes);
                        logDevice = 2;
                        m_logPendingBytes = 0;
                    }
                }
            } else {
                m_logPendingBytes = 0;
            }
        }

        if (logDevice == 1) {
            logger->logData(1, logAngle, {});
        } else if (logDevice == 2) {
            logger->logData(2, 0.0f, logValues);
        }
    }
}

// Переводит первые `count` байт буфера в hex-текст для окна лога и удаляет их из буфера.
// Формат тот же, что и раньше: "xx " на байт, перенос строки после каждого 16-го байта.
void SerialPortManager::consumeToDisplay(size_t count) {
    if (count == 0) return;
    if (count > m_rawBuffer.size()) count = m_rawBuffer.size();

    static const char kHex[] = "0123456789abcdef";
    std::string chunk;
    chunk.reserve(count * 3 + count / 16 + 1);
    for (size_t j = 0; j < count; ++j) {
        const unsigned char b = m_rawBuffer[j];
        chunk.push_back(kHex[b >> 4]);
        chunk.push_back(kHex[b & 0x0F]);
        chunk.push_back(' ');
        m_byteCountForNewLine++;
        if (m_byteCountForNewLine % 16 == 0) chunk.push_back('\n');
    }
    m_displayText += chunk;

    const size_t MAX_DISPLAY = 65536;
    if (m_displayText.size() > MAX_DISPLAY) {
        m_displayText.erase(0, m_displayText.size() - MAX_DISPLAY);
        size_t firstNewLine = m_displayText.find('\n');
        if (firstNewLine != std::string::npos && firstNewLine < 20) {
            m_displayText.erase(0, firstNewLine + 1);
        }
    }
    m_rawBuffer.erase(m_rawBuffer.begin(), m_rawBuffer.begin() + static_cast<std::ptrdiff_t>(count));
}

void SerialPortManager::parseAndFormat() {
    // Ограничение размера буфера действует во ВСЕХ режимах, включая RawOnly:
    // раньше в RawOnly (Dev2) буфер рос бесконечно.
    const size_t MAX_RAW_BUFFER = 1024 * 1024;
    if (m_rawBuffer.size() > MAX_RAW_BUFFER) {
        size_t eraseCount = m_rawBuffer.size() - (MAX_RAW_BUFFER / 2);
        m_rawBuffer.erase(m_rawBuffer.begin(), m_rawBuffer.begin() + static_cast<std::ptrdiff_t>(eraseCount));
    }

    const ParseMode mode = m_parseMode.load();

    // Если режим RawOnly - просто накапливаем данные и ничего не парсим.
    // UI забирает последние байты через getTailBytes().
    if (mode == ParseMode::RawOnly) {
        return;
    }

    if (mode == ParseMode::Marker) {
        size_t i = 0;
        while (i + 3 < m_rawBuffer.size()) {
            if (m_rawBuffer[i] == 0x23 && m_rawBuffer[i + 1] == 0x23) {
                unsigned char high = m_rawBuffer[i + 2];
                unsigned char low = m_rawBuffer[i + 3];
                unsigned int raw = (static_cast<unsigned int>(high) << 8) | low;
                m_rawValue = raw;
                m_currentAngle = (static_cast<float>(raw) * 360.0f) / 4096.0f;
                m_angleUpdated = true;
                i += 4;
            } else {
                i++;
            }
        }

        if (i > 0) {
            if (i > m_rawBuffer.size() - 3) i = m_rawBuffer.size() > 3 ? m_rawBuffer.size() - 3 : 0;
            consumeToDisplay(i);
        }
    } else if (mode == ParseMode::Raw16Bit) {
        size_t i = 0;
        while (i + 1 < m_rawBuffer.size()) {
            unsigned char high = m_rawBuffer[i];
            unsigned char low = m_rawBuffer[i + 1];
            unsigned int val = (static_cast<unsigned int>(high) << 8) | low;
            m_rawValue = val;
            m_currentAngle = (static_cast<float>(val) * 360.0f) / 4096.0f;
            m_angleUpdated = true;
            i += 2;
        }
        consumeToDisplay(i);
    } else if (mode == ParseMode::RawFloat) {
        size_t i = 0;
        while (i + 3 < m_rawBuffer.size()) {
            float f;
            std::memcpy(&f, &m_rawBuffer[i], sizeof(float));
            if (f >= -360.0f && f <= 720.0f) {
                m_currentAngle = f;
                const float scaled = f * 4096.0f / 360.0f;
                // Приведение float -> unsigned для значений <= -1 было неопределённым поведением
                // (получался мусор вроде 4294965069). Для scaled > -1 (в т.ч. все f >= 0) результат
                // прежний и корректно определён; более отрицательные углы приводятся к 0..4095
                // (как угол по модулю 360).
                if (scaled > -1.0f) {
                    m_rawValue = static_cast<unsigned int>(scaled);
                } else {
                    m_rawValue = static_cast<unsigned int>(static_cast<int>(std::floor(scaled)) + 4096);
                }
                m_angleUpdated = true;
            }
            i += 4;
        }
        consumeToDisplay(i);
    }
}

void SerialPortManager::startRecording(const std::string &filename) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile) {
        fclose(m_logFile);
    }
    m_logFile = fopen(filename.c_str(), "w");
}

void SerialPortManager::stopRecording() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile) {
        fclose(m_logFile);
        m_logFile = nullptr;
    }
}

bool SerialPortManager::isRecording() const {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logFile != nullptr;
}

void SerialPortManager::writeLogLine(const std::string &line) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile) {
        fprintf(m_logFile, "%s\n", line.c_str());
        fflush(m_logFile);
    }
}
