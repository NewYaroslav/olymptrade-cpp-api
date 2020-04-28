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
    olymp_trade::OlympTradeApi<> olymptrade(8080);
    /* ждем получения настроек */
    if(!olymptrade.wait()) return -1;

    /* тестируем работу uuid */
    std::cout << "uuid: " << olymptrade.get_test_uuid() << std::endl;
    std::cout << "account_id_real: " << olymptrade.get_account_id_real() << std::endl;
    std::cout << "is_demo: " << olymptrade.demo_account() << std::endl;

    olymptrade.set_demo_account(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    olymptrade.set_demo_account(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_demo, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;

    {
        std::cout << "open bo ETHUSD" << std::endl;
        std::cout << "ETHUSD payout " << olymptrade.get_payout("ETHUSD") << std::endl;

        double a = 0, p = 0;
        int err = olymptrade.get_amount(a, p, "ETHUSD", 180, olymptrade.get_balance(), 0.58, 0.4);
        std::cout << "get_amount err " << err << " a: " << a << " p: " << p << std::endl;

        err = olymptrade.open_bo("ETHUSD", 30.0, olymp_trade::BUY, 180,
            [&](const olymp_trade::Bet &bet) {
            std::cout << "callback ETHUSD" << std::endl;
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
            } else
            if(bet.bet_status == olymp_trade::BetStatus::STANDOFF) {
                std::cout << "STANDOFF" << std::endl;
            }
        });
        if(err != olymp_trade::OK) std::cout << err << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    {
        std::cout << "open bo EURUSD" << std::endl;
        int err = olymptrade.open_bo("EURUSD", 30.0, olymp_trade::BUY, 180,
            [&](const olymp_trade::Bet &bet) {
            std::cout << "callback EURUSD" << std::endl;
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
            } else
            if(bet.bet_status == olymp_trade::BetStatus::STANDOFF) {
                std::cout << "STANDOFF" << std::endl;
            }
        });
        if(err != olymp_trade::OK) std::cout << err << std::endl;
    }

    /* выводим поток котировок */
    while(true) {
        std::cout
            << "d: " << olymptrade.demo_account()
            << " b: " << olymptrade.get_balance()
            << " server time: " << xtime::get_str_date_time_ms(olymptrade.get_server_timestamp())
            << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    return 0;
}


