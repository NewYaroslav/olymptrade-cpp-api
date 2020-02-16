#include <iostream>
#include "olymp-trade-api.hpp"
#include "xtime.hpp"
#include <fstream>
#include <dir.h>
#include <stdlib.h>

#define BUILD_VER 1.0

using json = nlohmann::json;


int main() {
    std::cout << "start!" << std::endl;
    for(uint32_t i = 0; i < 2; ++i) {
        std::cout << "new connect" << std::endl;
        olymp_trade::OlympTradeApi<> olymptrade(8080);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
    std::cout << "next" << std::endl << std::endl << std::endl;
    olymp_trade::OlympTradeApi<> olymptrade(8080);
    /* ждем получения настроек */
    if(!olymptrade.wait()) return -1;

    /* тестируем работу uuid */
    std::cout << "uuid: " << olymptrade.get_test_uuid() << std::endl;
    std::cout << "account_id_real: " << olymptrade.get_account_id_real() << std::endl;
    std::cout << "is_demo: " << olymptrade.demo_account() << std::endl;

    /* получаем массив символов */
    std::cout << "symbols: " << olymptrade.get_symbol_list().size() << std::endl;

    olymptrade.set_demo_account(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_real, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;
    olymptrade.set_demo_account(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_demo, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;

    int err = olymptrade.open_bo("ETHUSD", 30.0, olymp_trade::BUY, 60,
        [&](const olymp_trade::Bet &bet) {
        if(bet.bet_status == olymp_trade::BetStatus::UNKNOWN_STATE) {
            std::cout << "UNKNOWN_STATE" << std::endl;
        } else
        if(bet.bet_status == olymp_trade::BetStatus::OPENING_ERROR) {
            std::cout << "OPENING_ERROR" << std::endl;
        } else
        if(bet.bet_status == olymp_trade::BetStatus::WAITING_COMPLETION) {
            std::cout << "WAITING_COMPLETION" << std::endl;
        } else
        if(bet.bet_status == olymp_trade::BetStatus::CHECK_ERROR) {
            std::cout << "CHECK_ERROR" << std::endl;
        } else
        if(bet.bet_status == olymp_trade::BetStatus::WIN) {
            std::cout << "WIN" << std::endl;
        } else
        if(bet.bet_status == olymp_trade::BetStatus::LOSS) {
            std::cout << "LOSS" << std::endl;
        }
    });
    if(err != olymp_trade::OK) std::cout << err << std::endl;

    /* подпишемся на поток */
    std::vector<std::string> symbol_list = {"LTCUSD", "Bitcoin", "ZECUSD"};
    olymptrade.subscribe_quotes_stream(symbol_list, 60);

    /* выводим поток котировок */
    while(true) {
        olymp_trade::Candle candle = olymptrade.get_candle("LTCUSD");
        std::cout << "LTCUSD, p: " << olymptrade.get_payout("LTCUSD") << "  c: " << candle.close << " t: " << candle.timestamp << std::endl;
        std::cout
            << "d: " << olymptrade.demo_account()
            << " b: " << olymptrade.get_balance()
            << " server time: " << xtime::get_str_date_time_ms(olymptrade.get_server_timestamp())
            << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}


