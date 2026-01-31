#include "serial_port_handler.hpp"

#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

// Linux用シリアル通信ライブラリ.
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

namespace winch {

namespace {

// 値の連続送信開始・停止コマンド.
constexpr int INVALID_FD = -1;
constexpr const char* kStartCommand = "NO99CX\r";
constexpr const char* kStopCommand  = "NO99CY\r";

bool ReadLine(int fd, std::string& buffer) {
    buffer.clear();
    char c;

    while (true) {
        int n = read(fd, &c, 1);
        if (n < 0) return false;
        if (n == 0) return false;

        if (c == '\n') {
            return true;
        }
        if (c != '\r') {
            buffer += c;
        }
    }
}

void WriteSerial(int fd, const std::string& data) {
    write(fd, data.c_str(), data.size());
}

}  // namespace

SerialPortHandler::SerialPortHandler(
        const std::string& port_name,
        const std::atomic_bool& stop_flag,
        std::shared_ptr<TimeSeriesStorage> storage)
        : port_name_(port_name), stop_flag_(stop_flag),
            storage_(std::move(storage)),
            serial_port_{INVALID_FD} {}

bool SerialPortHandler::Initialize() {
    // ポートを開く．
    serial_port_ = open(port_name_.c_str(), O_RDWR);

    if (serial_port_ < 0) {
        // ポートが開けなかった場合，エラーを表示してfalseを返す．
        std::cerr << "Error " << errno << " from open: " 
                  << strerror(errno) << std::endl;
        std::cerr << "ポートを開けませんでした．ポート名: " << port_name_ << std::endl;
        std::cerr << "Note: ポート名が正しいか、sudo権限があるか確認してください。" 
                  << std::endl;
        return false;
    }

    struct termios tty;
    if (tcgetattr(serial_port_, &tty) != 0) {
        // シリアルポートの属性取得に失敗した場合，エラーを表示してfalseを返す．
        std::cerr << "Error " << errno << " from tcgetattr: " 
                  << strerror(errno) << std::endl;
        std::cerr << "ポートの属性を取得できませんでした．ポート名: " << port_name_ << std::endl;
        return false;
    }

    // baudrate=115200.
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // bytesize=serial.EIGHTBITS (CS8).
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // parity=serial.PARITY_EVEN.
    tty.c_cflag |= PARENB;  // パリティ有効.
    tty.c_cflag &= ~PARODD; // 偶数パリティ(Even).

    // stopbits=serial.STOPBITS_ONE.
    tty.c_cflag &= ~CSTOPB; // 1ストップビット.

    // その他設定 (Rawモードにするためのおまじない).
    tty.c_cflag &= ~CRTSCTS; // フロー制御なし.
    tty.c_cflag |= CREAD | CLOCAL; // 受信有効、ローカルライン.

    tty.c_lflag &= ~ICANON; // カノニカルモード無効.
    tty.c_lflag &= ~ECHO;   // エコーバック無効.
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ISIG;   // シグナル無効.

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // ソフトウェアフロー制御無効.
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

    tty.c_oflag &= ~OPOST; // 出力処理無効.
    tty.c_oflag &= ~ONLCR; // 改行変換無効.

    // timeout=1.0 (VTIMEは0.1秒単位なので10で1秒).
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    // 設定を反映
    if (tcsetattr(serial_port_, TCSANOW, &tty) != 0) {
        std::cerr << "Error " << errno << " from tcsetattr: " 
                  << strerror(errno) << std::endl;
        std::cerr << "ポートの属性を設定できませんでした．ポート名: " << port_name_ << std::endl;
        return false;
    }

    is_initialized_ = true;
    return true;
}

void SerialPortHandler::Update() {
    // 初期化されていなければ早期リターン.
    if (!is_initialized_) return;

    // データ受信開始コマンドを送信.
    WriteSerial(serial_port_, kStartCommand);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto start_time = std::chrono::steady_clock::now();
    std::string buffer;
    std::string line;
    constexpr int kHz100_ms = 10;  // 100 Hz = 10 ms.
    auto next_read_time = std::chrono::steady_clock::now();

    while (!stop_flag_.load()) {
        // 100Hz周期でポーリング
        auto now = std::chrono::steady_clock::now();
        if (now < next_read_time) {
            std::this_thread::sleep_for(next_read_time - now);
        }
        next_read_time += std::chrono::milliseconds(kHz100_ms);

        // 1文字ずつ読み込む（ノンブロッキング）.
        char c;
        int n = read(serial_port_, &c, 1);

        if (n > 0) {
            if (c == '\n') {
                // 改行で1行完成.
                size_t comma_pos = buffer.find(',');
                if (comma_pos != std::string::npos) {
                    try {
                        std::string val_str = buffer.substr(comma_pos + 1);
                        double value = std::stod(val_str);
                        if (storage_) {
                            auto now = std::chrono::steady_clock::now();
                            std::chrono::duration<double> elapsed = now - start_time;
                            storage_->Add(elapsed.count(), value);
                        }
                    }
                    catch (...) {
                        // パース失敗は無視.
                    }
                }
                buffer.clear();
            } else if (c != '\r') {
                // \r は無視、それ以外をバッファに追加.
                buffer += c;
            }
        }
        // n <= 0 の場合はデータなし。次の周期まで待機.
    }

    // データ受信停止コマンドを送信.
    WriteSerial(serial_port_, kStopCommand);
}

void SerialPortHandler::Finalize() {
    if (serial_port_ >= 0) {
        // ポートを閉じる.
        close(serial_port_);
        serial_port_ = INVALID_FD;
    }

    // 初期化フラグをリセット.
    is_initialized_ = false;
}

}  // namespace winch
