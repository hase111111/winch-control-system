#ifndef INPUT_HANDLER_HPP
#define INPUT_HANDLER_HPP

#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <utility>

#include "i_handler.hpp"
#include "time_series_storage.hpp"

namespace winch {

//! @brief 標準入力を監視し、終了コマンドを受け付けるクラス.
//! "exit"や"quit"が入力されたらstop_flagをtrueに設定する.
class InputHandler final : public IHandler {
public:
    //! @brief コンストラクタ.
    //! @param stop_flag 停止フラグへの参照（非const、このクラスが書き込む）
    //! @param storages データ保存用ストレージの(name, storage)ペアvector
    InputHandler(std::atomic_bool& stop_flag,
                 const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages);

    //! @brief 初期化処理（特に何もしない）.
    //! @return 常にtrueを返す
    bool Initialize() override;

    //! @brief 標準入力を監視する.
    //! "exit"または"quit"が入力されたらstop_flagをtrueに設定してループを抜ける.
    //! 内部でwhileループを回すため、別スレッドで実行すること.
    void Update() override;

    //! @brief 終了処理（特に何もしない）.
    void Finalize() override;

private:
    //! @brief コマンド一覧を表示する.
    void PrintHelp() const;

    std::atomic_bool& stop_flag_;
    std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>> storages_;
};

}  // namespace winch

#endif // INPUT_HANDLER_HPP
