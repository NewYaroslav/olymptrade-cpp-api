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

    /* получаем массив символов */
    std::cout << "symbols: " << olymptrade.get_symbol_list().size() << std::endl;

    olymptrade.set_demo_account(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_real, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;
    olymptrade.set_demo_account(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_demo, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;
    olymptrade.set_demo_account(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "set_real, d: " << olymptrade.demo_account() << " b: " << olymptrade.get_balance() << std::endl;

    std::cout << "request_amount_limits, err code: " << olymptrade.request_amount_limits() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "check_limit, code: " << olymptrade.check_limit() << std::endl;
    std::cout << "limit response: " << olymptrade.get_limit_data().response << std::endl;
    std::system("pause");
    return 0;
}


