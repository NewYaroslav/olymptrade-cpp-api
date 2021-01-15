#include <iostream>
#include "olymp-trade-api.hpp"
#include "xtime.hpp"
#include <fstream>
#include <dir.h>
#include <stdlib.h>

#define BUILD_VER 1.0

using json = nlohmann::json;


int main() {
    std::cout << "test" << std::endl;

    olymp_trade::OlympTradeApi<> olymptrade(8080);

    olymptrade.start();
    std::cout << "start!" << std::endl;
    /* ждем получения настроек */
    olymptrade.wait();
    std::cout << "wait end" << std::endl;
    //if(!olymptrade.wait()) return -1;

    /* тестируем работу uuid */
    std::cout << "account_id_real: " << olymptrade.get_account_id_real() << std::endl;
    std::cout << "balance: " << olymptrade.get_balance() << std::endl;
    std::cout << "email:   " << olymptrade.get_email() << std::endl;
    std::cout << "currency: " << olymptrade.get_currency() << std::endl;
    std::cout << "user id: " << olymptrade.get_user_id() << std::endl;
    std::cout << "is_demo: " << olymptrade.demo_account() << std::endl;

    olymptrade.request_amount_limits();
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    /* вернет код, отличный от нуля, если брокер ввел лимиты */
    std::cout << "check_limit, code: " << olymptrade.check_limit() << std::endl;

    /* метод get_limit_data возвращает структуру лимита,
     * которая содержит подробности введенных ограничений */
    std::cout << "limit response: " << olymptrade.get_limit_data().response << std::endl;

    std::system("pause");
    return 0;
}


