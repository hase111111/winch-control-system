
#ifndef SERIAL_PORT_HANDLER_HPP
#define SERIAL_PORT_HANDLER_HPP

namespace winch {

class SerialPortHandler final {
    bool Initialize();
    void Finalize();
};

}  // namespace winch

#endif // SERIAL_PORT_HANDLER_HPP
