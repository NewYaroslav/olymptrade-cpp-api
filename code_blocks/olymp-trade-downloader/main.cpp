#include <iostream>
#include "olymp-trade-api.hpp"
#include "xquotes_history.hpp"
#include "xtime.hpp"
#include <fstream>
#include <dir.h>
#include <stdlib.h>

#define PROGRAM_VERSION "1.0"
#define PROGRAM_DATE    "16.02.2020"

using json = nlohmann::json;

int main(int argc, char **argv) {
    std::cout << "olymp trade downloader" << std::endl;
    std::cout
        << "version: " << PROGRAM_VERSION
        << " date: " << PROGRAM_DATE
        << std::endl << std::endl;

    uint32_t port = 8080;
    std::vector<std::string> raw_list_symbol;
    std::string json_file;
    std::string path_store;
    std::string environmental_variable;
    bool is_use_day_off = false;
    bool is_use_current_day = false;

    if(!olymp_trade::process_arguments(
            argc,
            argv,
            [&](
                const std::string &key,
                const std::string &value){
        if(key == "json_file" || key == "jf") {
            json_file = value;
        } else
        if(key == "port" || key == "p") {
            port = atoi(value.c_str());
        } else
        if(key == "path_store" || key == "path_storage" || key == "ps") {
            path_store = value;
        } else
        if(key == "use_day_off" || key == "udo") {
            is_use_day_off = true;
        } else
        if(key == "not_use_day_off" || key == "nudo" ||
            key == "skip_day_off" || key == "sdo") {
            is_use_day_off = false;
        } else
        if(key == "use_current_day" || key == "ucd") {
            is_use_current_day = true;
        } else
        if(key == "not_use_current_day" || key == "nucd" ||
            key == "skip_current_day" || key == "scd") {
            is_use_current_day = false;
        } else
        if(key == "add_symbol" || key == "as") {
            olymp_trade::parse_list(value, raw_list_symbol);
        }
    })) {
        std::cerr << "Error! No parameters!" << std::endl;
        return EXIT_FAILURE;
    }

    /* Проверяем наличие файла настроек в формате JSON
     * и если файл найден, читаем настройки из файла
     */
    json auth_json;
    if(!json_file.empty()) {
        try {
            std::ifstream auth_file(json_file);
            auth_file >> auth_json;
            auth_file.close();

            if(auth_json["port"] != nullptr) port = auth_json["port"];

            if(auth_json["path_store"] != nullptr) path_store = auth_json["path_store"];
            else if(auth_json["path_storage"] != nullptr) path_store = auth_json["path_storage"];

            if(auth_json["environmental_variable"] != nullptr) environmental_variable = auth_json["environmental_variable"];

            if(auth_json["use_current_day"] != nullptr) is_use_current_day = auth_json["use_current_day"];
            else if(auth_json["skip_current_day"] != nullptr) is_use_current_day = !auth_json["skip_current_day"];

            if(auth_json["use_day_off"] != nullptr) is_use_day_off = auth_json["use_day_off"];
            else if(auth_json["skip_day_off"] != nullptr) is_use_day_off = !auth_json["skip_day_off"];

            if(auth_json["list_symbol"] != nullptr) {
                for(size_t i = 0; i < auth_json["list_symbol"].size(); ++i) {
                    std::string symbol_name = auth_json["list_symbol"];
                    raw_list_symbol.push_back(symbol_name);
                }
            }
        }
        catch (json::parse_error &e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            return EXIT_FAILURE;
        }
        catch (std::exception e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            return EXIT_FAILURE;
        }
        catch (...) {
            std::cerr << "json parser error!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    if(raw_list_symbol.size() == 0) {
        std::cerr << "Error, no symbols!" << std::endl;
        return EXIT_FAILURE;
    }

    /* если указана переменная окружения, то видоизменяем пути к файлам */
    if(environmental_variable.size() != 0) {
        const char* env_ptr = std::getenv(environmental_variable.c_str());
        if(env_ptr == NULL) {
            std::cerr << "Error, no environment variable!" << std::endl;
            return EXIT_FAILURE;
        }
        if(path_store.size() != 0) {
            path_store = std::string(env_ptr) + "\\" + path_store;
        } else path_store = std::string(env_ptr);
    }

    /* проверяем настройки */
    if(path_store.empty()) {
        std::cerr << "Error, parameter : path_store" << std::endl;
        return EXIT_FAILURE;
    }

    olymp_trade::OlympTradeApi<xquotes_common::Candle> olymptrade(port);
    if(!olymptrade.wait()) {
        std::cerr << "Error, olymptrade.wait()" << std::endl;
        return EXIT_FAILURE;
    }

    uint32_t index = 0;
    while(index < raw_list_symbol.size()) {
        if(olymptrade.check_symbol(raw_list_symbol[index])) {
            ++index;
            continue;
        }
        std::cerr << "Error, delete symbol: " << raw_list_symbol[index] << std::endl;
        raw_list_symbol.erase(raw_list_symbol.begin() + index);
    }

    if(raw_list_symbol.size() == 0) {
        std::cerr << "Error, no symbols!" << std::endl;
        return EXIT_FAILURE;
    }


    /* выводим параметры загрузки истории */
    std::cout << "download options:" << std::endl;
    std::cout << "port: " << port << std::endl;
    if(is_use_day_off) {
        std::cout << "download day off: true" << std::endl;
    } else {
        std::cout << "download day off: false" << std::endl;
    }
    if(is_use_current_day) {
        std::cout << "download current day: true" << std::endl;
    } else {
        std::cout << "download current day: false" << std::endl;
    }
    std::cout << "download symbols:" << std::endl;
    for(size_t s = 0; s < raw_list_symbol.size(); ++s) {
        std::cout << raw_list_symbol[s] << std::endl;
    }

    std::cout << std::endl;

    /* получаем конечную дату загрузки */
    xtime::timestamp_t timestamp = xtime::get_timestamp();
    if(!is_use_current_day) {
        timestamp = xtime::get_first_timestamp_day(timestamp) -
            xtime::SECONDS_IN_DAY;
    }

    /* создаем папку для записи котировок */
    bf::create_directory(path_store);

    /* создаем хранилища котировок */
    std::vector<std::shared_ptr<xquotes_history::QuotesHistory<>>> hists(raw_list_symbol.size());
    for(uint32_t symbol = 0;
        symbol < raw_list_symbol.size();
        ++symbol) {
        std::string file_name =
            path_store + "/" +
            raw_list_symbol[symbol] + ".qhs4";

        hists[symbol] = std::make_shared<xquotes_history::QuotesHistory<>>(
            file_name,
            xquotes_history::PRICE_OHLC,
            xquotes_history::USE_COMPRESSION);
    }

    /* скачиваем исторические данные котировок */
    for(uint32_t
        symbol = 0;
        symbol < raw_list_symbol.size();
        ++symbol) {
        /* получаем время уже загруженных даных
         * функция get_min_max_day_timestamp позволяет получить метки времени данных по дате,
          * т.е. переменные min_timestamp и max_timestampтолько содержат начало дня
         */
        int err = xquotes_common::OK;
        xtime::timestamp_t min_timestamp = 0, max_timestamp = 0;
        err = hists[symbol]->get_min_max_day_timestamp(min_timestamp, max_timestamp);

        if(err != xquotes_common::OK) {
            std::cout << "search for the starting date symbol: "
                << raw_list_symbol[symbol]
                << "..." << std::endl;
            min_timestamp = olymptrade.get_min_timestamp_symbol("Bitcoin");
            if(min_timestamp == 0) {
                std::cerr << "error getting the start date of the currency pair!" << std::endl;
                return EXIT_FAILURE;
            }
            std::cout << "download: " << raw_list_symbol[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                    << std::endl;
        } else {
            std::cout << "symbol: " << raw_list_symbol[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                    << std::endl;
            min_timestamp = max_timestamp;
            const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
            std::cout
                    << "download: " << raw_list_symbol[symbol]
                    << " start date: " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                    << " stop date: " << xtime::get_str_date(xtime::get_first_timestamp_day(stop_date))
                    << std::endl;
        }

        /* непосредственно здесь уже проходимся по датам и скачиваем данные */
        const xtime::timestamp_t stop_date = xtime::get_first_timestamp_day(timestamp);
        for(xtime::timestamp_t t = xtime::get_first_timestamp_day(min_timestamp); t <= stop_date; t += xtime::SECONDS_IN_DAY) {
            /* пропускаем данные, которые нет смысла загружать повторно */
            if(t < max_timestamp && hists[symbol]->check_timestamp(t)) continue;
            /* пропускаем выходной день, если указано его пропускать*/
            if(!is_use_day_off && xtime::is_day_off(t)) continue;

            /* теперь загружаем данные */
            std::vector<xquotes_common::Candle> candles_1;
            std::vector<xquotes_common::Candle> candles_2;
            std::vector<xquotes_common::Candle> candles_3;
            int err_candles = xquotes_common::OK;
            for(uint32_t a = 0; a < 10; ++a) {
                err_candles = olymptrade.get_historical_data(
                    raw_list_symbol[symbol],
                    t,
                    t + xtime::SECONDS_IN_MINUTE * 480 - xtime::SECONDS_IN_MINUTE,
                    xtime::SECONDS_IN_MINUTE,
                    candles_1);
                if(err_candles == xquotes_common::OK) break;
                std::cerr << "error receiving symbol data (1): " << raw_list_symbol[symbol] << " repeat..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err_candles != xquotes_common::OK) {
                std::cerr << "error receiving symbol data: " << raw_list_symbol[symbol]
                << " " << xtime::get_str_date_time(t) << " - "
                << xtime::get_str_date_time(t + xtime::SECONDS_IN_MINUTE * 480 - xtime::SECONDS_IN_MINUTE) << std::endl;
                return EXIT_FAILURE;
            }
            for(uint32_t a = 0; a < 10; ++a) {
                err_candles = olymptrade.get_historical_data(
                    raw_list_symbol[symbol],
                    t + xtime::SECONDS_IN_MINUTE * 480,
                    t + xtime::SECONDS_IN_MINUTE * 960 - xtime::SECONDS_IN_MINUTE,
                    xtime::SECONDS_IN_MINUTE,
                    candles_2);
                if(err_candles == xquotes_common::OK) break;
                std::cerr << "error receiving symbol data (2): " << raw_list_symbol[symbol] << " repeat..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err_candles != xquotes_common::OK) {
                std::cerr << "error receiving symbol data: " << raw_list_symbol[symbol]
                << " " << xtime::get_str_date_time(t + xtime::SECONDS_IN_MINUTE * 480) << " - "
                << xtime::get_str_date_time(t + xtime::SECONDS_IN_MINUTE * 960 - xtime::SECONDS_IN_MINUTE) << std::endl;
                return EXIT_FAILURE;
            }
            for(uint32_t a = 0; a < 10; ++a) {
                err_candles = olymptrade.get_historical_data(
                    raw_list_symbol[symbol],
                    t + xtime::SECONDS_IN_MINUTE * 960,
                    t + xtime::SECONDS_IN_MINUTE * 1440 - xtime::SECONDS_IN_MINUTE,
                    xtime::SECONDS_IN_MINUTE,
                    candles_3);
                if(err_candles == xquotes_common::OK) break;
                std::cerr << "error receiving symbol data (3): " << raw_list_symbol[symbol] << " repeat..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            if(err_candles != xquotes_common::OK) {
                std::cerr << "error receiving symbol data: " << raw_list_symbol[symbol]
                << " " << xtime::get_str_date_time(t + xtime::SECONDS_IN_MINUTE * 960) << " - "
                << xtime::get_str_date_time(t + xtime::SECONDS_IN_MINUTE * 1440 - xtime::SECONDS_IN_MINUTE) << std::endl;
                return EXIT_FAILURE;
            }

            /* подготавливаем данные */
            std::array<xquotes_common::Candle, xtime::MINUTES_IN_DAY> bars_inside_day;
            for(size_t i = 0; i < candles_1.size(); ++i) {
                bars_inside_day[xtime::get_minute_day(candles_1[i].timestamp)] = candles_1[i];
            }
            for(size_t i = 0; i < candles_2.size(); ++i) {
                bars_inside_day[xtime::get_minute_day(candles_2[i].timestamp)] = candles_2[i];
            }
            for(size_t i = 0; i < candles_3.size(); ++i) {
                bars_inside_day[xtime::get_minute_day(candles_3[i].timestamp)] = candles_3[i];
            }

            const size_t candles_size = candles_1.size() + candles_2.size() + candles_3.size();

            /* записываем данные */
            if(candles_size > 0) {
                err = hists[symbol]->write_candles(bars_inside_day, t);
                xtime::timestamp_t end_timestamp =
                    candles_3.size() > 0 ? candles_3.back().timestamp :
                    candles_2.size() > 0 ? candles_2.back().timestamp :
                    candles_1.size() > 0 ? candles_1.back().timestamp :
                    t;
                olymp_trade::print_line(
                        "write " +
                        raw_list_symbol[symbol] +
                        " " +
                        xtime::get_str_date(t) +
                        " - " + xtime::get_str_date_time(end_timestamp) +
                        " code " +
                        std::to_string(err));
            } else {
                olymp_trade::print_line(
                        "skip " +
                        raw_list_symbol[symbol] +
                        " " +
                        xtime::get_str_date(t));
            }
        }
        hists[symbol]->save();
        std::cout
                << "data writing to file completed: " << raw_list_symbol[symbol]
                << " " << xtime::get_str_date(xtime::get_first_timestamp_day(min_timestamp))
                << " - " << xtime::get_str_date(xtime::get_first_timestamp_day(timestamp))
                << std::endl << std::endl;
    } // for symbol
    return EXIT_SUCCESS;
}


