#ifndef I_HANDLER_HPP
#define I_HANDLER_HPP

namespace winch {

//! @brief ハンドラークラスの基底インターフェイス.
//! SerialPortHandler, UdpHandler, CanHandlerなどの共通インターフェイスを定義する.
class IHandler {
public:
    virtual ~IHandler() = default;

    //! @brief ハンドラーの初期化を行う.
    //! @return 初期化に成功したらtrue，失敗したらfalseを返す.
    virtual bool Initialize() = 0;

    //! @brief ハンドラーの更新処理を行う.
    //! 内部でwhileループを回すため，別スレッドで実行すること.
    virtual void Update() = 0;

    //! @brief ハンドラーの終了処理を行う.
    virtual void Finalize() = 0;
};

}  // namespace winch

#endif // I_HANDLER_HPP
