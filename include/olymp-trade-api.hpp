/*
* olymptrade-api-cpp - C ++ API client for olymptrade
*
* Copyright (c) 2019 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef OLYMP_TRADE_API_HPP_INCLUDED
#define OLYMP_TRADE_API_HPP_INCLUDED

#include <olymp-trade-api-common.hpp>
#include <server_ws.hpp>
#include <nlohmann/json.hpp>
#include <xtime.hpp>
#include <future>
#include <thread>
#include <atomic>
#include <chrono>
#include <limits>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <queue>
#include <iostream>

namespace olymp_trade {
    using namespace olymp_trade_common;

    template<class CANDLE_TYPE = Candle>
    class OlympTradeApi {
    public:
        using json = nlohmann::json;
        using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

        /// Типы События
        enum class EventType {
            NEW_TICK,                   /**< Получен новый тик */
            HISTORICAL_DATA_RECEIVED,   /**< Получены исторические данные */
        };

    private:

        std::mutex request_future_mutex;
        std::vector<std::future<void>> request_future;
        std::atomic<bool> is_request_future_shutdown = ATOMIC_VAR_INIT(false);

        /** \brief Очистить список запросов
         */
        void clear_request_future() {
            std::lock_guard<std::mutex> lock(request_future_mutex);
            size_t index = 0;
            while(index < request_future.size()) {
                try {
                    if(request_future[index].valid()) {
                        std::future_status status = request_future[index].wait_for(std::chrono::milliseconds(0));
                        if(status == std::future_status::ready) {
                            request_future[index].get();
                            request_future.erase(request_future.begin() + index);
                            continue;
                        }
                    }
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: clear_request_future, what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: clear_request_future()" << std::endl;
                }
                ++index;
            }
        }

        std::future<void> server_future;
        std::atomic<bool> is_command_server_stop;

        std::atomic<bool> is_cout_log;

        std::mutex server_mutex;
        std::shared_ptr<WsServer> server;

        std::mutex current_connection_mutex;
        std::shared_ptr<WsServer::Connection> current_connection;

        std::atomic<bool> is_connected; /**< Флаг установленного соединения */
        std::atomic<bool> is_open_connect;
        std::atomic<bool> is_error;

        /* все для расчета смещения времени */
        const uint32_t array_offset_timestamp_size = 256;
        std::array<xtime::ftimestamp_t, 256> array_offset_timestamp;    /**< Массив смещения метки времени */
        uint8_t index_array_offset_timestamp = 0;                       /**< Индекс элемента массива смещения метки времени */
        uint32_t index_array_offset_timestamp_count = 0;
        xtime::ftimestamp_t last_offset_timestamp_sum = 0;
        std::atomic<double> offset_timestamp = ATOMIC_VAR_INIT(0.0);    /**< Смещение метки времени */
        std::atomic<bool> is_autoupdate_logger_offset_timestamp;

        /** \brief Обновить смещение метки времени
         *
         * Данный метод использует оптимизированное скользящее среднее
         * для выборки из 256 элеметов для нахождения смещения метки времени сервера
         * относительно времени компьютера
         * \param offset смещение метки времени
         */
        inline void update_offset_timestamp(const xtime::ftimestamp_t &offset) {
            if(index_array_offset_timestamp_count != array_offset_timestamp_size) {
                array_offset_timestamp[index_array_offset_timestamp] = offset;
                index_array_offset_timestamp_count = (uint32_t)index_array_offset_timestamp + 1;
                last_offset_timestamp_sum += offset;
                offset_timestamp = last_offset_timestamp_sum / (xtime::ftimestamp_t)index_array_offset_timestamp_count;
                ++index_array_offset_timestamp;
                return;
            }
            /* находим скользящее среднее смещения метки времени сервера относительно компьютера */
            last_offset_timestamp_sum = last_offset_timestamp_sum +
                (offset - array_offset_timestamp[index_array_offset_timestamp]);
            array_offset_timestamp[index_array_offset_timestamp++] = offset;
            offset_timestamp = last_offset_timestamp_sum/
                (xtime::ftimestamp_t)array_offset_timestamp_size;
        }
        /* */

        std::mutex sub_uid_mutex;
        std::string sub_uid;

        /* параметры аккаунта */
        std::mutex account_mutex;
        std::string currency;
        std::atomic<double> balance_demo = ATOMIC_VAR_INIT(0.0);
        std::atomic<double> balance_real = ATOMIC_VAR_INIT(0.0);
        std::atomic<uint64_t> account_id_demo = ATOMIC_VAR_INIT(0);
        std::atomic<uint64_t> account_id_real = ATOMIC_VAR_INIT(0);
        std::atomic<bool> is_demo = ATOMIC_VAR_INIT(true);

        std::mutex array_bets_mutex;    /**< Мьютекс для блокировки array_bets */
        std::map<std::string, Bet> array_bets;
        std::map<uint64_t, std::string> bet_id_to_uuid;
        std::mutex broker_bet_id_to_uuid_mutex;
        std::map<uint64_t, std::string> broker_bet_id_to_uuid;

        std::mutex bets_id_counter_mutex;
        uint64_t bets_id_counter = 0;   /**< Счетчик номера сделок, открытых через API */

        /* все про валютные пары */
        class SymbolSpec {
        public:
            xtime::timestamp_t min_timestamp = 0;
            xtime::timestamp_t time_open = 0;
            xtime::timestamp_t time_close = 0;
            uint32_t min_duration = 0;
            uint32_t max_duration = 0;
            double winperc = 0;
            double min_amount = 0;
            double max_amount = 0;
            bool is_active = false;
            bool is_locked = true;
            bool is_locked_trading = true;
            uint32_t precision = 0;

            SymbolSpec() {};
        };
        std::mutex symbols_spec_mutex;
        std::map<std::string, SymbolSpec> symbols_spec;

        /* для потока котировок */
        std::mutex map_candles_mutex;
        std::map<std::string, std::map<xtime::timestamp_t, CANDLE_TYPE>> map_candles;

        std::mutex hist_candles_mutex;
        std::vector<CANDLE_TYPE> hist_candles;
        std::atomic<bool> is_hist_candles = ATOMIC_VAR_INIT(false);
        std::atomic<bool> is_error_hist_candles = ATOMIC_VAR_INIT(false);

        std::string format(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            std::vector<char> v(1024);
            while (true)
            {
                va_list args2;
                va_copy(args2, args);
                int res = vsnprintf(v.data(), v.size(), fmt, args2);
                if ((res >= 0) && (res < static_cast<int>(v.size())))
                {
                    va_end(args);
                    va_end(args2);
                    return std::string(v.data());
                }
                size_t size;
                if (res < 0)
                    size = v.size() * 2;
                else
                    size = static_cast<size_t>(res) + 1;
                v.clear();
                v.resize(size);
                va_end(args2);
            }
        }

        inline std::string get_uuid() {
            uint64_t timestamp1000 = xtime::get_ftimestamp() * 1000.0 + 0.5;
            std::string temp(CBase36::encodeInt(timestamp1000));
            temp += CBase36::randomString(10,11);
            std::transform(temp.begin(), temp.end(),temp.begin(), ::toupper);
            return temp;
        }

    public:

        inline std::string get_test_uuid() {return get_uuid();};

    private:

        /** \brief Отправить сообщение
         * \param message Сообщение
         */
        void send(const std::string &message) {
            std::lock_guard<std::mutex> lock(current_connection_mutex);
            if(!current_connection) return;
            current_connection->send(message, [&](const SimpleWeb::error_code &ec) {
                if(ec) {
                    if(is_cout_log) std::cout << "OlympTradeApi Server: Error sending message. " <<
                    // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                    "Error: " << ec << ", error message: " << ec.message() << std::endl;
                }
            });
        }

        int async_open_bo(
                const std::string &symbol_name,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                const double open_timestamp_offset,
                const bool is_demo_account,
                uint64_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            if(contract_type != ContractType::BUY && contract_type != ContractType::SELL) return INVALID_CONTRACT_TYPE;
            if((duration % xtime::SECONDS_IN_MINUTE) != 0) return INVALID_DURATION;
            {
                std::lock_guard<std::mutex> lock(symbols_spec_mutex);
                auto it_spec = symbols_spec.find(symbol_name);
                if(it_spec == symbols_spec.end()) return INVALID_SYMBOL;
                if(amount < it_spec->second.min_amount) return INVALID_AMOUNT;
                if(amount > it_spec->second.max_amount) return INVALID_AMOUNT;
                if(duration < it_spec->second.min_duration) return INVALID_DURATION;
                if(duration > it_spec->second.max_duration) return INVALID_DURATION;
                if(it_spec->second.is_locked) return SYMBOL_LOCK;
            }
            std::string uuid = get_uuid();
            json j = json::array();
            j[0]["e"] = 23;
            j[0]["t"] = 2;
            j[0]["uuid"] = uuid;
            json j_data = json::array();
            j_data[0]["amount"] = (double)((uint64_t)(amount * 100.0 + 0.5)) / 100.0;  //atof(format("%.2f", amount).c_str());
            if(contract_type == ContractType::BUY) j_data[0]["dir"] = "up";
            else if(contract_type == ContractType::SELL) j_data[0]["dir"] = "down";
            j_data[0]["pair"] = symbol_name;
            j_data[0]["cat"] = "digital";
            j_data[0]["pos"] = 0;
            j_data[0]["source"] = "platform";
            if(is_demo_account) {
                j_data[0]["account_id"] = 0;
                j_data[0]["group"] = "demo";
            } else {
                j_data[0]["account_id"] = (uint64_t)account_id_real;
                j_data[0]["group"] = "real";
            }
            j_data[0]["timestamp"] = (uint64_t)((get_server_timestamp() + open_timestamp_offset) * 1000.0 + 0.5);
            j_data[0]["duration"] = duration;
            j[0]["d"] = j_data;

            Bet bet;
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                bet.api_bet_id = bets_id_counter;
                api_bet_id = bet.api_bet_id;
                ++bets_id_counter;
            }
            //bet.broker_bet_id = 0;
            bet.symbol_name = symbol_name;
            bet.note = note;
            bet.contract_type = contract_type;
            bet.duration = duration;
            //bet.opening_timestamp = 0;
            //bet.closing_timestamp = 0;
            bet.send_timestamp = get_server_timestamp();
            bet.amount = amount;
            bet.is_demo_account = is_demo_account;
            bet.bet_status = BetStatus::UNKNOWN_STATE;

            {
                std::lock_guard<std::mutex> lock(array_bets_mutex);
                array_bets[uuid] = bet;
                bet_id_to_uuid[bet.api_bet_id] = uuid;
            }

            send(j.dump());

            /* запускаем асинхронное открытие сделки */
            if(callback != nullptr) {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,[&,
                        uuid,
                        callback] {
                    if(callback != nullptr) callback(bet);
                    /* ждем в цикле, пока сделка не закроется */
                    BetStatus last_bet_status = BetStatus::UNKNOWN_STATE;
                    while(!is_request_future_shutdown) {
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        Bet bet;
                        {
                            std::lock_guard<std::mutex> lock(array_bets_mutex);
                            auto it_array_bets = array_bets.find(uuid);
                            if(it_array_bets == array_bets.end()) {
                                bet.bet_status = BetStatus::CHECK_ERROR;
                            } else {
                                if(it_array_bets->second.bet_status == last_bet_status) continue;
                                last_bet_status = it_array_bets->second.bet_status;
                                const uint64_t stop_timestamp = it_array_bets->second.send_timestamp + it_array_bets->second.duration + xtime::SECONDS_IN_MINUTE;
                                const uint64_t server_timestamp = get_server_timestamp();
                                if(server_timestamp > stop_timestamp) {
                                    bet.bet_status = BetStatus::CHECK_ERROR;
                                } else {
                                    bet = it_array_bets->second;
                                }
                            }
                        }
                        callback(bet);
                        if(bet.bet_status != BetStatus::WAITING_COMPLETION &&
                            bet.bet_status != BetStatus::UNKNOWN_STATE) break;
                    };
                });
            }
            clear_request_future();
            return OK;
        };

        bool parse_candle_history(json &j) {
            if(j.find("data")!= j.end() && j["data"].is_array()) {
                std::vector<CANDLE_TYPE> temp_candles;
                bool is_data = false;
                try {
                    for(size_t i = 0; i < j["data"].size(); ++i) {
                        const double open = j["data"][i]["open"];
                        const double high = j["data"][i]["high"];
                        const double low = j["data"][i]["low"];
                        const double close = j["data"][i]["close"];
                        xtime::timestamp_t timestamp = j["data"][i]["time"];
                        temp_candles.push_back(CANDLE_TYPE(open, high, low, close, timestamp));
                    }
                    is_data = true;
                }
                catch(const json::parse_error& e) {
                    std::cerr << "OlympTradeApi::parse_candle_history json::parse_error, what: " << e.what()
                       << " exception_id: " << e.id << std::endl;
                }
                catch(json::out_of_range& e) {
                    std::cerr << "OlympTradeApi::parse_candle_history json::out_of_range, what:" << e.what()
                       << " exception_id: " << e.id << std::endl;
                }
                catch(json::type_error& e) {
                    std::cerr << "OlympTradeApi::parse_candle_history json::type_error, what:" << e.what()
                       << " exception_id: " << e.id << std::endl;
                }
                catch(...) {
                    std::cerr << "OlympTradeApi::parse_candle_history json error" << std::endl;
                }
                if(is_data) {
                    std::lock_guard<std::mutex> lock(hist_candles_mutex);
                    hist_candles = temp_candles;
                    is_hist_candles = true;
                    return true;
                }
            }
            return false;
        }

        bool parse_user(json &j) {
            if(j.find("data")!= j.end() && !j["data"].is_array()) {
                if(j["data"]["group"] == "demo") {
                    is_demo = true;
                    return true;
                } else
                if(j["data"]["group"] == "real") {
                    is_demo = false;
                    return true;
                }
            }
            return false;
        }

        bool parse_platform(json &j) {
            if(j.find("deal") != j.end()) {
                {
                    std::lock_guard<std::mutex> lock(account_mutex);
                    if(j["currency"] != nullptr) currency = j["currency"]["name"];
                    if(j["user"]["balance_demo"] != nullptr) balance_demo = (double)j["user"]["balance_demo"];
                    if(j["user"]["balance"] != nullptr) balance_real = (double)j["user"]["balance"];
                    if(j["user"]["is_demo"] != nullptr) is_demo = j["user"]["is_demo"];
                }

                {
                    std::lock_guard<std::mutex> lock(symbols_spec_mutex);
                    auto it = j["pairs"].find("pairs");
                    if(it != j["pairs"].end()) {
                        for(auto& element : it->items()) {
                            //std::cout << element.key() << " : " << element.value() << "\n";
                            symbols_spec[element.key()].is_locked = element.value()["locked"];
                            symbols_spec[element.key()].is_active = element.value()["active"];
                            symbols_spec[element.key()].is_locked_trading = element.value()["locked_trading"];

                            symbols_spec[element.key()].winperc = element.value()["winperc"];
                            symbols_spec[element.key()].time_open = element.value()["time_open"];
                            symbols_spec[element.key()].time_open = element.value()["time_close"];

                            symbols_spec[element.key()].min_duration = element.value()["min_duration"];
                            symbols_spec[element.key()].max_duration = element.value()["max_duration"];
                            symbols_spec[element.key()].precision = element.value()["precision"];
                            symbols_spec[element.key()].max_amount = element.value()["max_amount"];
                            symbols_spec[element.key()].min_amount = element.value()["min_amount"];
                        }
                    }
                    if(j["pairs_available"] != nullptr) {
                        for(size_t i = 0; i < j["pairs_available"].size(); ++i) {
                            std::string symbol_name = j["pairs_available"][i]["id"];
                            symbols_spec[symbol_name].min_timestamp = j["pairs_available"][i]["from"];
                        }
                    }
                }
            } else return false;
            return true;
        }

        bool parse_olymptrade(json &j) {
            if(j.is_array()) {
                for(size_t i = 0; i < j.size(); ++i) {
                    /* обрабатываем котировку */
                    if(j[i]["e"] == 1) {
                        //std::cout << "1" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            const std::string symbol_name = j[i]["d"][l]["p"];
                            const xtime::ftimestamp_t timestamp = j[i]["d"][l]["t"];

                            const xtime::ftimestamp_t pc_time = xtime::get_ftimestamp();
                            const xtime::ftimestamp_t offset_time = timestamp - pc_time;
                            update_offset_timestamp(offset_time);

                            const xtime::timestamp_t bar_timestamp = xtime::get_first_timestamp_minute(timestamp);
                            const double price = j[i]["d"][l]["q"];

                            std::lock_guard<std::mutex> lock(map_candles_mutex);
                            auto it_symbol = map_candles.find(symbol_name);
                            if(it_symbol == map_candles.end()) {
                                map_candles[symbol_name][bar_timestamp] = CANDLE_TYPE(price,price,price,price,bar_timestamp);
                            } else
                            if(map_candles[symbol_name].find(bar_timestamp) == map_candles[symbol_name].end()) {
                                map_candles[symbol_name][bar_timestamp] = CANDLE_TYPE(price,price,price,price,bar_timestamp);
                            } else {
                                auto &candle = map_candles[symbol_name][bar_timestamp];
                                candle.close = price;
                                if(price > candle.high) candle.high = price;
                                else if(price < candle.low) candle.low = price;
                            }
                        }
                    } else
                    /* обрабатываем новый бар */
                    if(j[i]["e"] == 2) {
                        //std::cout << "2" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            const std::string symbol_name = j[i]["d"][l]["p"];
                            const xtime::timestamp_t timestamp = j[i]["d"][l]["t"];
                            const xtime::timestamp_t bar_timestamp = xtime::get_first_timestamp_minute(timestamp);
                            const double open = j[i]["d"][l]["open"];
                            const double high = j[i]["d"][l]["high"];
                            const double low = j[i]["d"][l]["low"];
                            const double close = j[i]["d"][l]["close"];
                            std::lock_guard<std::mutex> lock(map_candles_mutex);
                            map_candles[symbol_name][bar_timestamp] = CANDLE_TYPE(open,high,low,close,bar_timestamp);
                        }
                    } else
                    /* обрабатываем исторические данные */
                    if(j[i]["e"] == 4 || j[i]["e"] == 3) {
                        //std::cout << "4" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            if(j[i]["d"][l]["sub_uid"] != nullptr) {
                                std::lock_guard<std::mutex> lock(sub_uid_mutex);
                                sub_uid = j[i]["d"][l]["sub_uid"];
                            }
                            if(j[i]["d"][l]["tf"] != 60) continue;
                            const std::string symbol_name = j[i]["d"][l]["p"];
                            for(size_t n = 0; n < j[i]["d"][l]["candles"].size(); ++n) {
                                const xtime::timestamp_t timestamp = j[i]["d"][l]["candles"][n]["t"];
                                const xtime::timestamp_t bar_timestamp = xtime::get_first_timestamp_minute(timestamp);
                                const double open = j[i]["d"][l]["candles"][n]["open"];
                                const double high = j[i]["d"][l]["candles"][n]["high"];
                                const double low = j[i]["d"][l]["candles"][n]["low"];
                                const double close = j[i]["d"][l]["candles"][n]["close"];
                                std::lock_guard<std::mutex> lock(map_candles_mutex);
                                map_candles[symbol_name][bar_timestamp] = CANDLE_TYPE(open,high,low,close,bar_timestamp);
                            }
                        }
                    } else
                    /* обработаем сообщение - удачное открытие сделки */
                    if(j[i]["e"] == 23 && j[i]["t"] == 3) {
                        std::string uuid = j[i]["uuid"];
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            std::lock_guard<std::mutex> lock(array_bets_mutex);
                            auto it_array_bets = array_bets.find(uuid);
                            if(it_array_bets == array_bets.end()) continue;
                            it_array_bets->second.broker_bet_id = j[i]["d"][l]["id"];
                            {
                                std::lock_guard<std::mutex> lock(broker_bet_id_to_uuid_mutex);
                                broker_bet_id_to_uuid[it_array_bets->second.broker_bet_id] = uuid;
                            }
                            it_array_bets->second.amount = j[i]["d"][l]["amount"];
                            it_array_bets->second.payout = j[i]["d"][l]["winperc"];
                            if(j[i]["d"][l]["demo"] != nullptr) it_array_bets->second.is_demo_account = j[i]["d"][l]["demo"];
                            it_array_bets->second.opening_timestamp = j[i]["d"][l]["time_open"];
                            if(j[i]["d"][l]["status"] == "proceed") {
                                it_array_bets->second.bet_status = BetStatus::WAITING_COMPLETION;
                            } else
                            if(j[i]["d"][l]["status"] == "cancel") {
                                it_array_bets->second.bet_status = BetStatus::LOSS;
                            } else {
                                it_array_bets->second.bet_status = BetStatus::OPENING_ERROR;
                            }
                        }
                    } else
                    if(j[i]["e"] == 26) {
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            const uint64_t broker_bet_id = j[i]["d"][l]["id"];
                            std::string uuid;
                            {
                                std::lock_guard<std::mutex> lock(broker_bet_id_to_uuid_mutex);
                                auto it_broker_bet_id = broker_bet_id_to_uuid.find(broker_bet_id);
                                if(it_broker_bet_id == broker_bet_id_to_uuid.end()) continue;
                                uuid = it_broker_bet_id->second;
                            }

                            std::lock_guard<std::mutex> lock(array_bets_mutex);
                            auto it_array_bets = array_bets.find(uuid);
                            if(it_array_bets == array_bets.end()) continue;
                            if(it_array_bets->second.broker_bet_id != broker_bet_id) continue;

                            if(j[i]["d"][l]["status"] == "win") {
                                it_array_bets->second.bet_status = BetStatus::WIN;
                            } else
                            if(j[i]["d"][l]["status"] == "loose") {
                                it_array_bets->second.bet_status = BetStatus::LOSS;
                            } else {
                                it_array_bets->second.bet_status = BetStatus::CHECK_ERROR;
                            }
                            if(j[i]["d"][l]["time_close"] != nullptr) it_array_bets->second.closing_timestamp = j[i]["d"][l]["time_close"];
                        }
                    } else
                    /* обновим баланс
                     * j[i]["e"] == 51 - не понятно про что это, видимо бонусы
                     * 52 - демо
                     * 50 - реальный депозит
                     */
                    if(j[i]["e"] == 50 || j[i]["e"] == 52) {
                        //std::cout << "50-52" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            if(j[i]["d"][l]["account_id"] != nullptr && j[i]["d"][l]["account_id"] > 0) {
                                if(j[i]["d"][l]["value"] != nullptr) balance_real = (double)j[i]["d"][l]["value"];
                                account_id_real = (uint64_t)j[i]["d"][l]["account_id"];
                            } else {
                                if(j[i]["d"][l]["value"] != nullptr) balance_demo = (double)j[i]["d"][l]["value"];
                            }
                        }
                    } else
                    /* обновим состояние валютных пар */
                    if(j[i]["e"] == 70) {
                        //std::cout << "70" << std::endl;
                        for(auto& element : j[i]["d"]) {
                            std::string symbol_name = element["id"];
                            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
                            auto &symbol_spec = symbols_spec[symbol_name];
                            symbol_spec.is_locked = element["locked"];
                            symbol_spec.is_active = element["active"];
                            symbol_spec.winperc = element["winperc"];
                            symbol_spec.time_open = element["time_open"];
                            symbol_spec.time_open = element["time_close"];
                            symbol_spec.min_duration = element["min_duration"];
                            symbol_spec.max_duration = element["max_duration"];
                            symbol_spec.precision = element["precision"];
                            symbol_spec.max_amount = element["max_amount"];
                            symbol_spec.min_amount = element["min_amount"];
                        }
                    } else
                    /* обновим проценты выплат */
                    if(j[i]["e"] == 72) {
                        //std::cout << "72" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            std::string symbol_name = j[i]["d"][l]["pair"];
                            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
                            symbols_spec[symbol_name].winperc = j[i]["d"][l]["winperc"];
                        }
                    } else
                    /* запускаем поток еще раз */
                    if(j[i]["e"] == 95 && j[i]["t"] == 3) {
                        //std::cout << "95 t 3" << std::endl;
                    } else
                    if(j[i]["e"] == 120) {
                        //std::cout << "120" << std::endl;
                        for(size_t l = 0; l < j[i]["d"].size(); ++l) {
                            std::string group = j[i]["d"][l]["group"];
                            if(j[i]["d"][l]["group"] == "real") {
                                account_id_real = (uint64_t)j[i]["d"][l]["account_id"];
                                is_demo = false;
                            } else
                            if(j[i]["d"][l]["group"] == "demo") {
                                is_demo = true;
                            }
                        }
                    }

                }
            } else return false;
            return true;
        }

        void init_main_thread(
                const uint32_t port,
                const std::vector<std::string> &symbol_list,
                const uint32_t number_bars = 1440) {
            is_command_server_stop = false;
            is_cout_log = true;
            is_connected = false;
            is_error = false;
            server_future = std::async(std::launch::async,[&, port]() {
                while(!is_command_server_stop) {
                    //WsServer server;
                    is_open_connect = false;
                    {
                        std::lock_guard<std::mutex> lock(server_mutex);
                        server = std::make_shared<WsServer>();
                        server->config.port = port;
                        auto &olymptrade = server->endpoint["^/olymptrade-api/?$"];

                        /* принимаем сообщения */
                        olymptrade.on_message = [&](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message) {
                            auto out_message = in_message->string();
                            //if(is_cout_log) std::cout << "OlympTradeApi Server: Message received: \"" << out_message << "\" from " << connection.get() << std::endl;
                            //std::cout << "get meesage" << std::endl;
                            try {
                                json j = json::parse(in_message->string());

                                while(true) {
                                    if(parse_olymptrade(j)) break;
                                    if(parse_platform(j)) break;
                                    if(parse_user(j)) break;
                                    if(parse_candle_history(j)) break;
                                    if(j["connection_status"] == "ok") {
                                        is_connected = true;
                                        is_error = false;
                                        break;
                                    } else
                                    if(j["connection_status"] == "error") {
                                        is_error = true;
                                        is_connected = false;
                                        break;
                                    } else
                                    if(j["candle-history"] == "error") {
                                        is_error_hist_candles = true;
                                        break;
                                    }
                                    break;
                                }
                            }
                            catch(const json::parse_error& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "OlympTradeApi json::parse_error, what: " << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(json::out_of_range& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "OlympTradeApi json::out_of_range, what:" << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(json::type_error& e) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "OlympTradeApi json::type_error, what:" << e.what()
                                   << " exception_id: " << e.id << " message: "
                                   << std::endl << temp << std::endl;
                            }
                            catch(...) {
                                is_error = true;
                                std::string temp;
                                if(out_message.size() > 128) {
                                    temp = out_message.substr(0,128);
                                    temp += "...";
                                } else {
                                    temp = out_message;
                                }
                                std::cerr << "OlympTradeApi json error," << " message: "
                                   << std::endl << temp << std::endl;
                            }
                        };
                        olymptrade.on_open = [&](std::shared_ptr<WsServer::Connection> connection) {
                            {
                                std::lock_guard<std::mutex> lock(current_connection_mutex);
                                current_connection = connection;
                            }
                            if(is_cout_log) std::cout << "OlympTradeApi Server: Opened connection: " << connection.get() << std::endl;
                            is_open_connect = true;
                        };

                        // See RFC 6455 7.4.1. for status codes
                        olymptrade.on_close = [&](std::shared_ptr<WsServer::Connection> connection, int status, const std::string & /*reason*/) {
                            {
                                std::lock_guard<std::mutex> lock(current_connection_mutex);
                                if(current_connection.get() == connection.get()) {
                                    is_connected = false;
                                    current_connection.reset();
                                }
                            }
                            if(is_cout_log) std::cout << "OlympTradeApi Server: Closed connection: " << connection.get() << " with status code: " << status << std::endl;
                        };
                        // Can modify handshake response headers here if needed
                        olymptrade.on_handshake = [](std::shared_ptr<WsServer::Connection> /*connection*/, SimpleWeb::CaseInsensitiveMultimap & /*response_header*/) {
                            return SimpleWeb::StatusCode::information_switching_protocols; // Upgrade to websocket
                        };

                        // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                        olymptrade.on_error = [&](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
                            is_error = true;
                            if(is_cout_log) std::cout << "OlympTradeApi Server: Error in connection " << connection.get() << ". "
                            << "Error: " << ec << ", error message: " << ec.message() << std::endl;
                        };
                    }

                    server->start([&](unsigned short port) {
                        if(is_cout_log) std::cout << "server run" << std::endl;
                    });
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            });
        }

    public:

        OlympTradeApi(const uint32_t port) {
            std::vector<std::string> symbol_list;
            const uint32_t number_bars = 0;
            init_main_thread(port, symbol_list, number_bars);
        }

        ~OlympTradeApi() {
            is_command_server_stop = true;
            is_request_future_shutdown = true;
            {
                std::lock_guard<std::mutex> lock(server_mutex);
                if(server) server->stop();
            }
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                for(size_t i = 0; i < request_future.size(); ++i) {
                    if(request_future[i].valid()) {
                        try {
                            request_future[i].wait();
                            request_future[i].get();
                        }
                        catch(const std::exception &e) {
                            std::cerr << "Error: ~OlympTradeApi(), what: " << e.what() << std::endl;
                        }
                        catch(...) {
                            std::cerr << "Error: ~OlympTradeApi()" << std::endl;
                        }
                    }
                }
            }
            if(server_future.valid()) {
                try {
                    server_future.wait();
                    server_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: ~OlympTradeApi(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: ~OlympTradeApi()" << std::endl;
                }
            }
        }

        /** \brief Очистить массив сделок
         */
        void clear_bets_array() {
            {
                std::lock_guard<std::mutex> lock(bets_id_counter_mutex);
                std::lock_guard<std::mutex> lock2(array_bets_mutex);
                array_bets.clear();
                bet_id_to_uuid.clear();
                bets_id_counter = 0;
            }
            std::lock_guard<std::mutex> lock(broker_bet_id_to_uuid_mutex);
            broker_bet_id_to_uuid.clear();
        }

        /** \brief Получтить ставку
         * \param bet Класс ставки, сюда будут загружены все параметры ставки
         * \param api_bet_id Уникальный номер ставки, который возвращает метод async_open_bo_sprint
         * \return Код ошибки или 0 в случае успеха
         */
        int get_bet(Bet &bet, const uint64_t api_bet_id) {
            std::lock_guard<std::mutex> lock(array_bets_mutex);
            auto &it_bet_id_to_uuid = bet_id_to_uuid.find(api_bet_id);
            if(it_bet_id_to_uuid == bet_id_to_uuid.end()) return DATA_NOT_AVAILABLE;
            std::string uuid = it_bet_id_to_uuid->second;
            auto &it_array_bets = array_bets.find(uuid);
            if(it_array_bets == array_bets.end()) return DATA_NOT_AVAILABLE;
            bet = it_array_bets->second;
            return OK;
        }

        inline uint64_t get_account_id_real() {
            return account_id_real;
        }

        /** \brief Проверить, является ли аккаунт Demo
         * \return Вернет true если demo аккаунт
         */
        inline bool demo_account() {
            return is_demo;
        }

        /** \brief Получить метку времени сервера
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return метка времени сервера
         */
        inline xtime::ftimestamp_t get_server_timestamp() {
            return xtime::get_ftimestamp() + offset_timestamp;
        }

        /** \brief Проверить соединение
         * \return Вернет true, если соединение установлено
         */
        inline bool connected() {
            return is_connected;
        }

        /** \brief Подождать соединение
         *
         * Данный метод ждет, пока не установится соединение
         * \return вернет true, если соединение есть, иначе произошла ошибка
         */
        inline bool wait() {
            static xtime::timestamp_t timestamp_start = 0;
            while(!is_error && !is_connected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if(is_open_connect) {
                    if(timestamp_start == 0) timestamp_start = xtime::get_timestamp();
                    if(((int64_t)xtime::get_timestamp() - (int64_t)timestamp_start) >
                        (int64_t)xtime::SECONDS_IN_MINUTE) {
                        std::lock_guard<std::mutex> lock(server_mutex);
                        if(server) server->stop();
                        timestamp_start = 0;
                    }
                }
                if(is_command_server_stop) return false;
            }
            return is_connected;
        }

        /** \brief Получить список имен символов/валютных пар
         * \return список имен символов/валютных пар
         */
        std::vector<std::string> get_symbol_list() {
            std::vector<std::string> symbol_list(symbols_spec.size());
            uint32_t index = 0;
            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
            for(auto &symbol : symbols_spec) {
                symbol_list[index] = symbol.first;
                ++index;
            }
            return symbol_list;
        }

        /** \brief Проверить наличие символа у брокера
         * \param symbol_name Имя символа
         * \return Вернет true, если символ есть у брокера
         */
        bool check_symbol(const std::string &symbol_name) {
            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
            if(symbols_spec.find(symbol_name) == symbols_spec.end()) return false;
            return true;
        }

        /** \brief Подписаться на котировки
         *
         * \param symbol_list Список имен символов/валютных пар
         * \param number_candles Количество баров истории
         * \return Вернет true, если подключение есть и сообщения были переданы
         */
        bool subscribe_quotes_stream(
                const std::vector<std::string> &symbol_list,
                const uint32_t number_candles) {
            if(!is_connected) return is_connected;
            for(auto &symbol : symbol_list) {
                if(check_symbol(symbol)) {
                    json j;
                    j["cmd"] = "subscribe";
                    j["symbol"] = symbol;
                    j["to_timestamp"] =
                        xtime::get_first_timestamp_minute(get_server_timestamp()) -
                        number_candles * xtime::SECONDS_IN_MINUTE;
                    send(j.dump());
                }
            }
            return true;
        }

        /** \brief Установить демо счет или реальный
         * \param is_demo Демо счет, если true
         * \return Вернет true в случае успеха
         */
        bool set_demo_account(const bool is_demo_account) {
            if(!is_connected) return is_connected;
            if(is_demo_account == is_demo) return true;
            json j;
            j["cmd"] = "set-money-group";
            if(is_demo_account) {
                j["group"] = "demo";
                j["account_id"] = 0;
            } else {
                j["group"] = "real";
                j["account_id"] = (uint64_t)account_id_real;
            }
            send(j.dump());
            return true;
        }

        /** \brief Получить баланс счета
         * \return Баланс аккаунта
         */
        inline double get_balance() {
            if(!is_connected) return 0.0;
            return is_demo ? balance_demo : balance_real;
        }

        /** \brief Получить бар
         * \param symbol_name Имя символа
         * \return Бар
         */
        inline CANDLE_TYPE get_candle(const std::string &symbol_name) {
            if(!is_connected) return CANDLE_TYPE();
            std::lock_guard<std::mutex> lock(map_candles_mutex);
            if(map_candles.find(symbol_name) == map_candles.end()) return CANDLE_TYPE();
            if(map_candles[symbol_name].size() == 0) return CANDLE_TYPE();
            auto it_candle = map_candles[symbol_name].end();
            --it_candle;
            return it_candle->second;
        }

        /** \brief Получить процент выплаты
         * \param symbol_name Имя символа
         * \return Процент выплаты (от 0 до 1)
         */
        inline double get_payout(const std::string &symbol_name) {
            if(!is_connected) return 0.0;
            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
            auto it_spec = symbols_spec.find(symbol_name);
            if(it_spec == symbols_spec.end()) return 0.0;
            if(it_spec->second.is_locked) return 0.0;
            return it_spec->second.winperc  / 100.0;
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param api_bet_id Уникальный номер сделки внутри данной библиотеки
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                const double open_timestamp_offset,
                const bool is_demo_account,
                uint64_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            return async_open_bo(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                open_timestamp_offset,
                is_demo_account,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param api_bet_id Уникальный номер сделки внутри данной библиотеки
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                uint64_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {
            return async_open_bo(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                0.0,
                is_demo,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param symbol Символ
         * \param note Заметка сделки
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param open_timestamp_offset Смещение времени открытия
         * \param is_demo_account Торговать демо аккаунт
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const std::string &symbol,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                std::function<void(const Bet &bet)> callback = nullptr) {
            uint64_t api_bet_id = 0;
            std::string note;
            return async_open_bo(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                0.0,
                is_demo,
                api_bet_id,
                callback);
        }


        /** \brief Получить минимальную метку времени выбраного символа
         *
         * \param symbol_name Имя символа
         * \return Вернет метку времени, если символ есть и нет ошибок подключения
         */
        xtime::timestamp_t get_min_timestamp_symbol(const std::string &symbol_name) {
            if(!is_connected) return 0;
            std::lock_guard<std::mutex> lock(symbols_spec_mutex);
            auto it_spec = symbols_spec.find(symbol_name);
            if(it_spec == symbols_spec.end()) return 0;
            return it_spec->second.min_timestamp;
        }

        /** \brief Получить исторические данные
         * \param symbol_name Имя символа
         * \param date_start Начальная дата
         * \param date_stop Конечная дата
         * \param timeframe Таймфрейм
         * \param candles Свечи
         * \return Код ошибки
         */
        int get_historical_data(
                const std::string &symbol_name,
                xtime::timestamp_t date_start,
                xtime::timestamp_t date_stop,
                const uint32_t timeframe,
                std::vector<CANDLE_TYPE> &candles) {
            if(!is_connected) return AUTHORIZATION_ERROR;
            uint32_t limit = 0;
            if(timeframe == 60) {
                date_start = xtime::get_first_timestamp_minute(date_start);
                date_stop = xtime::get_first_timestamp_minute(date_stop);
                limit = ((date_stop - date_start) / xtime::SECONDS_IN_MINUTE) + 1;
            }
            json j;
            j["cmd"] = "candle-history";
            j["pair"] = symbol_name;
            j["size"] = timeframe;
            j["from"] = date_start;
            j["to"] = date_stop;
            j["limit"] = limit;
            is_error_hist_candles = false;
            {
                std::lock_guard<std::mutex> lock(hist_candles_mutex);
                hist_candles.clear();
                is_hist_candles = false;
            }
            send(j.dump());
            uint32_t tick = 0;
            while(true) {
                if(is_error_hist_candles) return DATA_NOT_AVAILABLE;
                {
                    std::lock_guard<std::mutex> lock(hist_candles_mutex);
                    if(is_hist_candles) {
                        candles = hist_candles;
                        return OK;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if(is_command_server_stop) return STRANGE_PROGRAM_BEHAVIOR;
                ++tick;
                if(tick >= xtime::SECONDS_IN_MINUTE) break;
            }
            return DATA_NOT_AVAILABLE;
        }

        /** \brief Получить абсолютный размер ставки и процент выплат
         *
         * Проценты выплат варьируются обычно от 0 до 1.0, где 1.0 соответствует 100% выплате брокера
         * \param amount Размер ставки бинарного опциона
         * \param payout Процент выплат
         * \param symbol_name Имя валютной пары
         * \param duration Длительность опциона в секундах
         * \param balance Размер депозита
         * \param winrate Винрейт
         * \param attenuator Коэффициент ослабления Келли
         * \return Состояние выплаты (0 в случае успеха, иначе см. PayoutCancelType)
         */
        int get_amount(
                double &amount,
                double &payout,
                const std::string &symbol_name,
                const uint32_t duration,
                const double balance,
                const double winrate,
                const double attenuator) {
            amount = 0;
            payout = 0;

            if((duration % xtime::SECONDS_IN_MINUTE) != 0) return INVALID_DURATION;
            {
                std::lock_guard<std::mutex> lock(symbols_spec_mutex);
                auto it_spec = symbols_spec.find(symbol_name);
                if(it_spec == symbols_spec.end()) return INVALID_SYMBOL;
                if(duration < it_spec->second.min_duration) return INVALID_DURATION;
                if(duration > it_spec->second.max_duration) return INVALID_DURATION;
                if(it_spec->second.is_locked) return SYMBOL_LOCK;
                payout = it_spec->second.winperc / 100.0;
                const double coeff = 1.0 + payout;
                if(winrate <= (1.0 / coeff)) return TOO_LITTLE_WINRATE;
                const double rate = ((coeff * winrate - 1.0) / payout) * attenuator;
                amount = balance * rate;
                if(amount < it_spec->second.min_amount ||
                    amount > it_spec->second.max_amount) {
                    amount = 0;
                    return INVALID_AMOUNT;
                }
            }
            return OK;
        }

    };
}

#endif // OLYMP-TRADE-API_HPP_INCLUDED
