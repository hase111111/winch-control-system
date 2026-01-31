#ifndef CAN_HANDLER_HPP
#define CAN_HANDLER_HPP

#include <atomic>

namespace winch {

class CanHandler final {
public:
    explicit CanHandler(std::atomic_bool& stop_flag);

    bool Initialize();
    
    //! @brief CAN通信の更新処理を行う.
    //! 内部でwhileループを回すため，別スレッドで実行すること.
    //! コンストラクタでこのクラスに渡すフラグを監視し，
    //! 終了フラグが立ったらループを抜ける.
    void Update();
    
    void Finalize();
    
private:
    std::atomic_bool& stop_flag_;
};

}  // namespace winch

#endif // CAN_HANDLER_HPP
