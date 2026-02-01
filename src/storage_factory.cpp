#include "storage_factory.hpp"

namespace winch {

std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>> 
StorageFactory::CreateDefaultStorages() {
    return {
        {"Serial Port Roadcell", std::make_shared<TimeSeriesStorage>()},
        {"UDP Potentiometer0", std::make_shared<TimeSeriesStorage>()},
        {"UDP Potentiometer1", std::make_shared<TimeSeriesStorage>()},
        {"CAN Motor0 Controll Value", std::make_shared<TimeSeriesStorage>()},
        {"CAN Motor1 Controll Value", std::make_shared<TimeSeriesStorage>()},
        {"CAN Motor0 Encoder", std::make_shared<TimeSeriesStorage>()},
        {"CAN Motor1 Encoder", std::make_shared<TimeSeriesStorage>()},
        {"Test Counter", std::make_shared<TimeSeriesStorage>()},
    };
}

}  // namespace winch
