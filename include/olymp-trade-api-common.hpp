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
#ifndef OLYMP_TRADE_API_COMMON_HPP_INCLUDED
#define OLYMP_TRADE_API_COMMON_HPP_INCLUDED

#include <iostream>
#include <xtime.hpp>
#include "base36.h"
#include <mutex>
#include <sstream>

namespace olymp_trade_common {

    /** \brief Класс для хранения бара
     */
    class Candle {
    public:
        double open;
        double high;
        double low;
        double close;
        double volume;
        xtime::timestamp_t timestamp;

        Candle() :
            open(0),
            high(0),
            low (0),
            close(0),
            volume(0),
            timestamp(0) {
        };

        Candle(
                const double &_open,
                const double &_high,
                const double &_low,
                const double &_close,
                const uint64_t &_timestamp) :
            open(_open),
            high(_high),
            low (_low),
            close(_close),
            volume(0),
            timestamp(_timestamp) {
        }

        Candle(
                const double &_open,
                const double &_high,
                const double &_low,
                const double &_close,
                const double &_volume,
                const uint64_t &_timestamp) :
            open(_open),
            high(_high),
            low (_low),
            close(_close),
            volume(_volume),
            timestamp(_timestamp) {
        }
    };

    /// Состояния сделки
    enum class BetStatus {
        UNKNOWN_STATE,
        OPENING_ERROR,
        CHECK_ERROR,        /**< Ошибка проверки результата сделки */
        WAITING_COMPLETION,
        WIN,
        LOSS,
    };

    /** \brief Класс для хранения информации по сделке
     */
    class Bet {
    public:
        uint64_t api_bet_id = 0;
        uint64_t broker_bet_id = 0;
        std::string symbol_name;
        std::string note;
        int contract_type = 0;                      /**< Тип контракта BUY или SELL */
        uint32_t duration = 0;                      /**< Длительность контракта в секундах */
        xtime::ftimestamp_t send_timestamp = 0;     /**< Метка времени начала контракта */
        xtime::ftimestamp_t opening_timestamp = 0;  /**< Метка времени начала контракта */
        xtime::ftimestamp_t closing_timestamp = 0;  /**< Метка времени конца контракта */
        double amount = 0;                          /**< Размер ставки в RUB или USD */
        double payout = 0;
        bool is_demo_account = false;               /**< Флаг демо аккаунта */
        BetStatus bet_status = BetStatus::UNKNOWN_STATE;

        Bet() {};
    };

    /// Типы События
    enum class EventType {
        NEW_TICK,                   /**< Получен новый тик */
        HISTORICAL_DATA_RECEIVED,   /**< Получены исторические данные */
    };

    enum ContractType {
        BUY = 1,
        SELL = -1,
    };

    /// Варианты состояния ошибок
    enum ErrorType {
        OK = 0,                             ///< Ошибки нет
        AUTHORIZATION_ERROR = -2,           ///< Ошибка авторизации
        CONTENT_ENCODING_NOT_SUPPORT = -3,  ///< Тип кодирования контента не поддерживается
        DECOMPRESSOR_ERROR = -4,            ///< Ошибка декомпрессии
        JSON_PARSER_ERROR = -5,             ///< Ошибка парсера JSON
        NO_ANSWER = -6,                     ///< Нет ответа
        INVALID_ARGUMENT = -7,              ///< Неверный аргумент метода класса
        STRANGE_PROGRAM_BEHAVIOR = -8,      ///< Странное поведение программы (т.е. такого не должно быть, очевидно проблема в коде)
        BETTING_QUEUE_IS_FULL = -9,         ///< Очередь ставок переполнена
        ERROR_RESPONSE = -10,               ///< Сервер брокера вернул ошибку
        NO_DATA_IN_RESPONSE = -11,
        ALERT_RESPONSE = -12,               ///< Сервер брокера вернул предупреждение
        DATA_NOT_AVAILABLE = -13,           ///< Данные не доступны
        PARSER_ERROR = -14,                 ///< Ошибка парсера ответа от сервера
        INVALID_DURATION = -15,
        INVALID_CONTRACT_TYPE = -16,
        INVALID_AMOUNT = -17,
        INVALID_SYMBOL = -18,
        SYMBOL_LOCK = -19,
        TOO_LITTLE_WINRATE = -20,
    };

    class PrintThread: public std::ostringstream {
    private:
        static inline std::mutex _mutexPrint;

    public:
        PrintThread() = default;

        ~PrintThread() {
            std::lock_guard<std::mutex> guard(_mutexPrint);
            std::cout << this->str();
        }
    };

    /** \brief Напечатать линию
     * \param message Сообщение
     */
    void print_line(std::string message) {
		static uint32_t message_size = 0;
		if(message_size > 0) {
			for(size_t i = 0; i < message_size; ++i) {
				PrintThread{} << " ";
			}
			PrintThread{} << "\r";
		}
		std::cout.width(message.size());
		PrintThread{} << message << "\r";
		message_size = message.size();
	}

	/* обработать все аргументы */
    bool process_arguments(
        const int argc,
        char **argv,
        std::function<void(
            const std::string &key,
            const std::string &value)> f) noexcept {
        if(argc <= 1) return false;
        bool is_error = true;
        for(int i = 1; i < argc; ++i) {
            std::string key = std::string(argv[i]);
            if(key.size() > 0 && (key[0] == '-' || key[0] == '/')) {
                uint32_t delim_offset = 0;
                if(key.size() > 2 && (key.substr(2) == "--") == 0) delim_offset = 1;
                std::string value;
                if((i + 1) < argc) value = std::string(argv[i + 1]);
                is_error = false;
                f(key.substr(delim_offset), value);
            }
        }
        return !is_error;
    }

    /** \brief Разобрать строку списка
     *
     * \param value Список
     * \param elemet_list Элементы списка
     */
    void parse_list(std::string value, std::vector<std::string> &elemet_list) noexcept {
        if(value.back() != ',') value += ",";
        std::size_t start_pos = 0;
        while(true) {
            std::size_t found_beg = value.find_first_of(",", start_pos);
            if(found_beg != std::string::npos) {
                std::size_t len = found_beg - start_pos;
                if(len > 0)
                    elemet_list.push_back(value.substr(start_pos, len));
                start_pos = found_beg + 1;
            } else break;
        }
    }
}

#endif // OLYMP_TRADE_API_COMMON_HPP_INCLUDED
