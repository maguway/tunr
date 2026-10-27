#include "SerialPortManager.h"

#include <windows.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

SerialPortManager::SerialPortManager() = default;

SerialPortManager::~SerialPortManager()
{
    close();
}

std::vector<std::string> SerialPortManager::getAvailablePorts()
{
    std::vector<std::string> ports;

    // Современный способ (Windows 10 1803+)
    typedef DWORD (WINAPI
        *GetCommPortsFn)(PULONG, ULONG, PULONG);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel)
    {
        auto fn = (GetCommPortsFn)GetProcAddress(hKernel, "GetCommPorts");
        if (fn)
        {
            ULONG buf[256];
            ULONG count = 0;
            if (fn(buf, 256, &count) == ERROR_SUCCESS)
            {
                for (ULONG i = 0; i < count; ++i)
                {
                    ports.push_back("COM" + std::to_string(buf[i]));
                }
                return ports;
            }
        }
    }

    // Fallback: перебор COM1..COM256
    for (int i = 1; i <= 256; ++i)
    {
        std::string name = "COM" + std::to_string(i);
        std::wstring wname(name.begin(), name.end());
        std::wstring path = L"\\\\.\\" + wname;
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            ports.push_back(name);
            CloseHandle(h);
        }
    }
    return ports;
}

bool SerialPortManager::open(const std::string& portName, int baudRate)
{
    close();

    std::wstring wname(portName.begin(), portName.end());
    std::wstring path = L"\\\\.\\" + wname;

    HANDLE h = CreateFileW(path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb))
    {
        CloseHandle(h);
        return false;
    }
    dcb.BaudRate = baudRate; // <-- ИСПОЛЬЗУЕМ ПЕРЕДАННУЮ СКОРОСТЬ
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    if (!SetCommState(h, &dcb))
    {
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

    m_handle = h;
    m_running = true;
    m_reader = std::thread(&SerialPortManager::readLoop, this);
    return true;
}

void SerialPortManager::close()
{
    m_running = false;
    if (m_reader.joinable())
        m_reader.join();
    if (m_handle)
    {
        CloseHandle((HANDLE)m_handle);
        m_handle = nullptr;
    }
}

bool SerialPortManager::isOpen() const
{
    return m_handle != nullptr;
}

void SerialPortManager::clearBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rawBuffer.clear();
    m_displayText.clear();
    m_lastParsePos = 0;
}

std::string SerialPortManager::getFormattedData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_displayText;
}

void SerialPortManager::readLoop()
{
    unsigned char tmp[1024];
    while (m_running)
    {
        DWORD read = 0;
        BOOL ok = ReadFile((HANDLE)m_handle, tmp, sizeof(tmp), &read, nullptr);
        if (!ok || read == 0)
        {
            Sleep(10);
            continue;
        }

        // --- Запись в файл (HEX) ---
        {
            std::lock_guard<std::mutex> logLock(m_logMutex);
            if (m_logFile)
            {
                for (DWORD i = 0; i < read; ++i)
                {
                    fprintf(m_logFile, "%02X ", tmp[i]);
                    if ((i + 1) % 16 == 0) fprintf(m_logFile, "\n");
                }
                fflush(m_logFile);
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalBytes += read;
        m_rawBuffer.insert(m_rawBuffer.end(), tmp, tmp + read);
        parseAndFormat();
    }
}

void SerialPortManager::parseAndFormat()
{
    // --- ЗАЩИТА ОТ ПЕРЕПОЛНЕНИЯ RAW-БУФЕРА ---
    const size_t MAX_RAW_BUFFER = 1024 * 1024; // 1 Мегабайт
    if (m_rawBuffer.size() > MAX_RAW_BUFFER) {
        // Оставляем только последние 512 КБ, чтобы не разорвать текущий пакет
        size_t eraseCount = m_rawBuffer.size() - (MAX_RAW_BUFFER / 2);
        m_rawBuffer.erase(m_rawBuffer.begin(), m_rawBuffer.begin() + eraseCount);
    }

    // --- НОВОЕ: Если маркер не нужен, просто оставляем всё как есть ---
    if (!m_useMarker)
    {
        return;
    }
    // -----------------------------------------------------------------

    // Ищем маркер 0x23 0x23
    size_t i = 0;

    while (i + 3 < m_rawBuffer.size()) // нужно минимум 4 байта: ## + старший + младший
    {
        if (m_rawBuffer[i] == 0x23 && m_rawBuffer[i + 1] == 0x23)
        {
            // Нашли пакет
            unsigned char high = m_rawBuffer[i + 2];
            unsigned char low = m_rawBuffer[i + 3];

            // Собираем сырое значение (старший байт, затем младший)
            m_rawValue = (static_cast<unsigned int>(high) << 8) | low;

            // Вычисляем угол: (значение / 4096) * 360
            m_currentAngle = (static_cast<float>(m_rawValue) * 360.0f) / 4096.0f;

            // Пропускаем обработанный пакет
            i += 4;
        }
        else
        {
            i++; // Если это не маркер — просто идём дальше
        }
    }

    // Удаляем из буфера всё, что мы уже просмотрели (чтобы не копить мусор)
    if (i > 0)
    {
        if (i > m_rawBuffer.size() - 3) i = m_rawBuffer.size() > 3 ? m_rawBuffer.size() - 3 : 0;

        // --- ДОБАВЛЕНО: Формируем текст для лога ---
        std::ostringstream oss;
        for (size_t j = 0; j < i; ++j)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)m_rawBuffer[j] << ' ';
            m_byteCountForNewLine++;
            if (m_byteCountForNewLine % 16 == 0)
                oss << '\n';
        }
        m_displayText += oss.str();

        // Ограничиваем лог, чтобы он не жрал память бесконечно
        const size_t MAX_DISPLAY = 65536;
        if (m_displayText.size() > MAX_DISPLAY)
        {
            m_displayText.erase(0, m_displayText.size() - MAX_DISPLAY);
            // Чтобы обрезка не сломала текущую строку, найдем первый \n
            size_t firstNewLine = m_displayText.find('\n');
            if (firstNewLine != std::string::npos && firstNewLine < 20)
            {
                m_displayText.erase(0, firstNewLine + 1);
            }
        }
        // -----------------------------------------------------------

        m_rawBuffer.erase(m_rawBuffer.begin(), m_rawBuffer.begin() + i);
    }
}

float SerialPortManager::getCurrentAngle() const
{
    return m_currentAngle;
}

unsigned int SerialPortManager::getRawValue() const
{
    return m_rawValue;
}

size_t SerialPortManager::totalBytesReceived() const
{
    return m_totalBytes;
}

void SerialPortManager::startRecording(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile)
    {
        fclose(m_logFile);
    }
    m_logFile = fopen(filename.c_str(), "w"); // Текстовый режим
}

void SerialPortManager::stopRecording()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile)
    {
        fclose(m_logFile);
        m_logFile = nullptr;
    }
}

bool SerialPortManager::isRecording() const
{
    return m_logFile != nullptr;
}

std::vector<unsigned char> SerialPortManager::getRawData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rawBuffer;
}

void SerialPortManager::setParseMarker(bool enable)
{
    m_useMarker = enable;
}

void SerialPortManager::writeLogLine(const std::string& line)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile)
    {
        fprintf(m_logFile, "%s\n", line.c_str());
        fflush(m_logFile);
    }
}
