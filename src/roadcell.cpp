#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <thread>      // sleep用
#include <cstring>     // strerror用

// Linux用シリアル通信ライブラリ
#include <fcntl.h>     // Contains file controls like O_RDWR
#include <errno.h>     // Error integer and strerror() function
#include <termios.h>   // Contains POSIX terminal control definitions
#include <unistd.h>    // write(), read(), close()

// ==========================
// 設定
// ==========================
// Raspberry Piでは一般的にUSB変換なら "/dev/ttyUSB0"、
// GPIO直結なら "/dev/serial0" や "/dev/ttyAMA0" などになります。
const char* PORT_NAME = "/dev/ttyUSB0"; 

int serial_port; // ファイルディスクリプタ

// ==========================
// シリアルポート初期化
// ==========================
bool setupSerial() {
    // ポートを開く
    serial_port = open(PORT_NAME, O_RDWR);

    if (serial_port < 0) {
        std::cerr << "Error " << errno << " from open: " << strerror(errno) << std::endl;
        std::cerr << "Note: ポート名が正しいか、sudo権限があるか確認してください。" << std::endl;
        return false;
    }

    struct termios tty;
    if(tcgetattr(serial_port, &tty) != 0) {
        std::cerr << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
        return false;
    }

    // --- Pythonの設定に合わせる ---
    // baudrate=115200
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // bytesize=serial.EIGHTBITS (CS8)
    tty.c_cflag &= ~CSIZE; 
    tty.c_cflag |= CS8;
    
    // parity=serial.PARITY_EVEN
    tty.c_cflag |= PARENB;  // パリティ有効
    tty.c_cflag &= ~PARODD; // 奇数パリティをオフ＝偶数パリティ(Even)

    // stopbits=serial.STOPBITS_ONE
    tty.c_cflag &= ~CSTOPB; // 2ビットをオフ＝1ストップビット

    // その他設定 (Rawモードにするためのおまじない)
    tty.c_cflag &= ~CRTSCTS; // フロー制御なし
    tty.c_cflag |= CREAD | CLOCAL; // 受信有効、ローカルライン

    tty.c_lflag &= ~ICANON; // カノニカルモード無効（1行単位ではなくバイト単位で処理）
    tty.c_lflag &= ~ECHO;   // エコーバック無効
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ISIG;   // シグナル無効

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // ソフトウェアフロー制御無効
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // 特殊文字変換無効

    tty.c_oflag &= ~OPOST; // 出力処理無効
    tty.c_oflag &= ~ONLCR; // 改行変換無効

    // timeout=1.0 (VTIMEは0.1秒単位なので10で1秒)
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    // 設定を反映
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        std::cerr << "Error " << errno << " from tcsetattr: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

// ==========================
// 1行読み込み (readline相当)
// ==========================
bool readLine(std::string& buffer) {
    buffer = "";
    char c;
    int n;
    
    while(true) {
        n = read(serial_port, &c, 1);
        
        if (n < 0) return false; // エラー
        if (n == 0) return false; // タイムアウト（データなし）

        // 改行コードが来たら終了 (\n)
        if (c == '\n') {
            return true;
        }
        // \r はバッファに入れない（無視）
        if (c != '\r') {
            buffer += c;
        }
    }
}

// ==========================
// 送信
// ==========================
void writeSerial(const std::string& data) {
    write(serial_port, data.c_str(), data.size());
}

// ==========================
// メイン処理
// ==========================
int main() {
    if (!setupSerial()) return 1;

    // --- 連続送信開始 ---
    std::cout << "Sending Start Command..." << std::endl;
    writeSerial("NO99CX\r");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<double> t_list;
    std::vector<double> load_list;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "Start continuous read (100 Hz)" << std::endl;

    std::string line;
    
    // --- 10秒間取得 ---
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        double t = elapsed.count();

        if (t > 10.0) break;

        // データ読み込み
        if (readLine(line)) {
            // 受信文字列: DSCB01,+12.345
            // カンマ区切りでパース
            size_t commaPos = line.find(',');
            if (commaPos != std::string::npos) {
                try {
                    std::string valStr = line.substr(commaPos + 1);
                    double value = std::stod(valStr);

                    t_list.push_back(t);
                    load_list.push_back(value);

                    // ターミナル表示 (printfなどで整形)
                    printf("%6.3f s : %8.4f kN\n", t, value);
                }
                catch (...) {
                    // 数値変換失敗時は無視
                    continue;
                }
            }
        }
    }

    // --- 連続送信停止 ---
    writeSerial("NO99CY\r");
    close(serial_port);
    std::cout << "Finished acquisition" << std::endl;

    // --- CSV保存 ---
    std::ofstream file("data.csv");
    file << "Time[s],Load[kN]\n";
    for (size_t i = 0; i < t_list.size(); ++i) {
        file << t_list[i] << "," << load_list[i] << "\n";
    }
    file.close();

    std::cout << "Saved to data.csv" << std::endl;

    return 0;
}
