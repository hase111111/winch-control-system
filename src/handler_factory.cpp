#include "handler_factory.hpp"

#include <iostream>

#include "can_handler.hpp"
#include "input_handler.hpp"
#include "serial_port_handler.hpp"
#include "test_counter_handler.hpp"
#include "time_series_storage.hpp"
#include "udp_handler.hpp"

namespace winch {

HandlerFactory::HandlerFactory(const ConfigLoader& config, std::atomic_bool& stop_flag)
    : config_(config), stop_flag_(stop_flag) {}

std::vector<std::shared_ptr<IHandler>> HandlerFactory::CreateHandlers(
    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const {
    std::vector<std::shared_ptr<IHandler>> handlers;

    const bool enable_serial = config_.GetVal<bool>("Flags", "enable_serial", true);
    const bool enable_udp = config_.GetVal<bool>("Flags", "enable_udp", true);
    const bool enable_can = config_.GetVal<bool>("Flags", "enable_can", true);

    std::cout << "Handler flags: serial=" << (enable_serial ? "on" : "off")
              << ", udp=" << (enable_udp ? "on" : "off")
              << ", can=" << (enable_can ? "on" : "off") << std::endl;

    // 入力ハンドラーを生成.
    std::cout << "Create InputHandler" << std::endl;
    handlers.emplace_back(std::make_shared<InputHandler>(stop_flag_, storages));

    if (enable_serial) {
        std::cout << "Create SerialPortHandler" << std::endl;
        if (auto serial_handler = CreateSerialHandler(storages)) {
            handlers.emplace_back(std::move(serial_handler));
        }
    }

    if (enable_udp) {
        std::cout << "Create UdpHandler" << std::endl;
        if (auto udp_handler = CreateUdpHandler(storages)) {
            handlers.emplace_back(std::move(udp_handler));
        }
    }

    if (enable_can) {
        std::cout << "Create CanHandler" << std::endl;
        if (auto can_handler = CreateCanHandler()) {
            handlers.emplace_back(std::move(can_handler));
        }
    }

    // テスト用カウントハンドラーを生成.
    std::cout << "Create TestCounterHandler" << std::endl;
    handlers.emplace_back(std::make_shared<TestCounterHandler>(
        stop_flag_, FindStorageByName(storages, "Test Counter")));

    return handlers;
}

std::shared_ptr<IHandler> HandlerFactory::CreateSerialHandler(
    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const {
    const auto serial_port = config_.GetVal<std::string>("Serial", "port", "/dev/ttyUSB0");
    std::cout << "Serial port: " << serial_port << std::endl;
    auto storage = FindStorageByName(storages, "Serial Port Roadcell");
    if (!storage) {
        std::cerr << "Serial用ストレージが見つかりません．" << std::endl;
        return nullptr;
    }

    return std::make_shared<SerialPortHandler>(serial_port, stop_flag_, storage);
}

std::shared_ptr<IHandler> HandlerFactory::CreateUdpHandler(
    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const {
    const int udp_port = config_.GetVal<int>("UDP", "port", 5005);
    std::cout << "UDP port: " << udp_port << std::endl;
    auto pot0 = FindStorageByName(storages, "UDP Potentiometer0");
    auto pot1 = FindStorageByName(storages, "UDP Potentiometer1");
    if (!pot0 || !pot1) {
        std::cerr << "UDP用ストレージが見つかりません．" << std::endl;
        return nullptr;
    }

    return std::make_shared<UdpHandler>(udp_port, stop_flag_, pot0, pot1);
}

std::shared_ptr<IHandler> HandlerFactory::CreateCanHandler() const {
    const auto can_if = config_.GetVal<std::string>("CAN", "interface");
    if (can_if.empty()) {
        std::cerr << "CANインターフェース名が空です．" << std::endl;
        return nullptr;
    }
    std::cout << "CAN interface: " << can_if << std::endl;
    return std::make_shared<CanHandler>(can_if, stop_flag_);
}

std::shared_ptr<TimeSeriesStorage> HandlerFactory::FindStorageByName(
    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages,
    const std::string& name) const {
    for (const auto& [storage_name, storage_ptr] : storages) {
        if (storage_name == name) {
            return storage_ptr;
        }
    }
    return nullptr;
}

}  // namespace winch
