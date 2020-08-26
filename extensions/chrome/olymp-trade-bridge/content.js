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

var symbols_array = [];
var socket;
var socket_array = [];
var api_socket;

var port;

var is_socket = false;
var is_last_socket = false;

var is_api_socket = false;
var is_last_api_socket = false;

var is_error = false;
var is_last_error = false;

var is_unsubscribe = false;

function getUuid() {
    return(Date.now().toString(36)+Math.random().toString(36).substr(2,12)).toUpperCase()
}

function injected_main() {
	console.log("Olymp Trade Bridge launched");
	
	var broker_domain = document.domain;

	// функция для запуска потока котировок
	function quotes_stream(new_socket, symbol_name, to_timestamp) {
		is_unsubscribe = false;
		new_socket = new WebSocket("wss://" + broker_domain + "/ds/v6"), new_socket.onopen = function() {
            console.log("Соединение " + symbol_name + " установлено."), 
			/* для версии v5 
			new_socket.send('[{"t":1,"e":105,"d":[{"source":"platform"}]}]'); 
			new_socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');
			new_socket.send('[{"t":2,"e":4,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60}]}]');
			new_socket.send('[{"t":2,"e":3,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60,"to":' + to_timestamp + ',"solid":true}]}]');
			*/
			// для версии v6
			new_socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');
			new_socket.send('[{"t":2,"e":4,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60}]}]');
			new_socket.send('[{"t":2,"e":3,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60,"to":' + to_timestamp + ',"solid":true}]}]');
			
        }, new_socket.onclose = function(t) {
			// заново открываем соединение
            if(is_api_socket && !is_unsubscribe) {
				quotes_stream(new_socket, symbol_name, to_timestamp);
			}
			t.wasClean ? console.log("Соединение " + symbol_name + " закрыто чисто") : console.log("Обрыв соединения"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
        },  new_socket.onmessage = function(t) {
			if(is_unsubscribe) new_socket.close();
			else if(is_api_socket) {
				// сначала фильтруем сигналы, отделяем только поток котировок
				var arr = JSON.parse(t.data);
				if(arr[0].e == 1 || arr[0].e == 2 || arr[0].e == 3 || arr[0].e == 4) {
					api_socket.send(t.data);
				}
			} else new_socket.close(); 	
			// console.log("Получены данные " + symbol_name + ": " + t.data);
        },  new_socket.onerror = function(t) {
			console.log("Ошибка " + symbol_name + ": " + t.message);
        }
	}
	
	function close_all_quotes_stream() {
		console.log("Закрытие потока котировок");
		is_unsubscribe = true;
		/*
		try {
			socket_array.forEach(function(item, index, array) {
				//if(socket_array[index]) socket_array[index].close();
				item.close();
			});
			console.log("Поток котировок закрыт");
		}
		catch(error) {
			console.log("Ошибка закрытия потока котировок " + error);
		}
		*/
	}
	
	function connect_broker() {
        socket = new WebSocket("wss://" + broker_domain + "/ds/v6"), socket.onopen = function() {
            console.log("Соединение установлено."), 
			/* для версии v5 
			socket.send('[{"t":1,"e":105,"d":[{"source":"platform"}]}]'); 
			socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');
			*/
			// для версии v6
			socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[125,127,128,51,50,52,120]}]'); // подписались на потоки
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[200,201]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[111]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[230,231]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[230,231]}]');
			socket.send('[{"t":2,"e":133,"uuid":"' + getUuid() + '"}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[141]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[110]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[140]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[241]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[60,61]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[70,73,72]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[100]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[22,20,21,26]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[42]}]');
			socket.send('[{"t":2,"e":98,"uuid":"' + getUuid() + '","d":[80]}]');
			
        }, socket.onclose = function(t) {
			is_socket = false;
            if(is_api_socket) {
				api_socket.send('{"connection_status":"reconnecting"}');
				connect_broker(); 
			}
			t.wasClean ? console.log("Соединение закрыто чисто") : console.log("Обрыв соединения"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
        }, socket.onmessage = function(t) {
			// отправляем сообщение, что соединение установлено
			if(!is_socket && is_api_socket) {
				api_socket.send('{"connection_status":"ok"}');
			}
			is_socket = true;
			
			if(is_api_socket) {
				api_socket.send(t.data);
			}		
			// console.log("Получены данные" + t.data);
        }, socket.onerror = function(t) {
			is_socket = false;
			if(is_api_socket) {
				api_socket.send('{"connection_status":"error"}');
			}
			console.log("Ошибка " + t.message);
        }
    }
	
	function connect_api() {
        api_socket = new WebSocket("ws://localhost:" + port + "/olymptrade-api"), 
		api_socket.onopen = function() {
			is_api_socket = true;
			var rt = new XMLHttpRequest;
			var upload_user_values = '{"list":["deals","options","pairs","pairs_available","risk_free_deals","accounts","analytics","avatar","balance","bonuses","data","duo_auth_state","jivo_settings","money_group","payment_systems","politics","promo","service_levels","session","stocksup","vip_status_amount","sound_packs","kyc"]}';
			rt.open("POST", "https://api.olymptrade.com/v4/user/values", !0), 
			rt.setRequestHeader('Content-Type', 'application/json'),
			// rt.setRequestHeader('Referer', 'https://olymptrade.com/platform'),
			// rt.setRequestHeader('DNT', '1'),
			rt.setRequestHeader('Accept', 'application/json, text/plain, */*'),
			rt.setRequestHeader('X-App-Version', '14965'),
			rt.setRequestHeader('X-Request-Type', 'Api-Request'),
			rt.setRequestHeader('X-Request-Project', 'bo'),
			rt.setRequestHeader('X-Requested-With', 'XMLHttpRequest'),
			rt.withCredentials = true, // ответ зачем rt.withCredentials = true тут: https://coderoad.ru/14221722/Set-Cookie-%D0%B2-%D0%B1%D1%80%D0%B0%D1%83%D0%B7%D0%B5%D1%80%D0%B5-%D1%81-%D0%B7%D0%B0%D0%BF%D1%80%D0%BE%D1%81%D0%BE%D0%BC-Ajax-%D1%87%D0%B5%D1%80%D0%B5%D0%B7-CORS
			rt.send(upload_user_values), 
			rt.onreadystatechange = function() {
				if (4 == rt.readyState) {
					if (200 != rt.status) {
						console.log(rt.status + ": " + rt.statusText);
						api_socket.send('{"connection_status":"error"}');
						is_error = true;
					} else {
						if(is_api_socket) {
							//console.log("https://api.olymptrade.com/v4/user/values rt.responseText out: " + rt.responseText);
							api_socket.send(rt.responseText);
							connect_broker();
						}
					}
				}
			}
			
			console.log("Соединение с сервером API установлено.");
        }, api_socket.onclose = function(t) {
			is_api_socket = false;
			/* закрываем соединение с брокером */
			if(is_socket) socket.close();
			close_all_quotes_stream();
			is_socket = false;
			/* пробуем переподключиться*/
            connect_api(); 
			t.wasClean ? console.log("Соединение с сервером API закрыто чисто") : console.log("Обрыв соединения с сервером API"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
        }, api_socket.onmessage = function(t) {
            // console.log("Получены данные от сервера API: " + t.data); 
			var api_message = JSON.parse(t.data);
			
			/* обрабатываем команды API: подписаться на поток котировок */
			if(api_message.cmd == "subscribe") {
				var symbol = api_message.symbol;
				var to_timestamp = api_message.to_timestamp;
				var idx = symbols_array.indexOf(symbol);
				if(idx == -1) {
					symbols_array.push(symbol);
				} else {
					symbols_array[idx] = symbol;
				}
				if(is_socket) {
					quotes_stream(socket_array[idx], symbol, to_timestamp);
				}
			} else
			/* обрабатываем команды API: отписаться от потока котировок */
			if(api_message.cmd == "unsubscribe") {
				close_all_quotes_stream();
			} else
			/* поменять тип счета */
			if(api_message.cmd == "set-money-group") {
				var group = api_message.group;
				var account_id = api_message.account_id;
				var rt = new XMLHttpRequest;
				var upload = '{"group":"' + group + '","account_id":' + account_id + '}';
				console.log("json_upload " + upload);
				
				rt.open("POST", "https://api.olymptrade.com/v4/user/set-money-group", !0); 
				rt.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				rt.setRequestHeader('Accept', 'application/json, text/plain, */*');
				rt.setRequestHeader('X-Request-Type', 'Api-Request');
				rt.setRequestHeader('X-Request-Project', 'bo');
				rt.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
				rt.withCredentials = true;
				rt.send(upload); 
				rt.onreadystatechange = function() {
					if (4 == rt.readyState) {
						if (200 != rt.status) {
							api_socket.send('{"connection_status":"error"}');
							console.log(rt.status + ": " + rt.statusText);
							is_error = true;
						} else {
							console.log("rt.responseText " + rt.responseText);
							api_socket.send(rt.responseText);
						}
					}
				}
			} else
			if(api_message.cmd == "get-amount-limits") {
				var group = api_message.group;
				var account_id = api_message.account_id;
				var rt = new XMLHttpRequest;
				var upload = '{"account_id":' + account_id + '}';
				//console.log("json_upload " + upload);
				
				rt.open("POST", "https://api.olymptrade.com/v1/cabinet/amount-limits", !0); 
				rt.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				rt.setRequestHeader('Accept', 'application/json, text/plain, */*');
				rt.setRequestHeader('X-Request-Type', 'Api-Request');
				rt.setRequestHeader('X-Request-Project', 'bo');
				rt.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
				rt.withCredentials = true;
				rt.send(upload);
				rt.onreadystatechange = function() {
					if (4 == rt.readyState) {
						if (200 != rt.status) {
							api_socket.send('{"connection_status":"error"}');
							console.log(rt.status + ": " + rt.statusText);
							is_error = true;
						} else {
							console.log("rt.responseText " + rt.responseText);
							api_socket.send(rt.responseText);
						}
					}
				}
			} else
				/* загрузить исторические данные */
			if(api_message.cmd == "candle-history") {
				var size = api_message.size;
				var pair = api_message.pair;
				var from = api_message.from;
				var to = api_message.to;
				var limit = api_message.limit;
				var rt = new XMLHttpRequest;
				var upload = '{"pair":"' + pair + '","size":' + size + ',"from":'+ from + ',"to":' + to + ',"limit":' + limit + '}'
				console.log("upload: " + upload);
				
				rt.open("POST", "https://api.olymptrade.com/v3/cabinet/candle-history", !0); 
				rt.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				rt.setRequestHeader('Accept', 'application/json, text/plain, */*');
				rt.setRequestHeader('X-Request-Type', 'Api-Request');
				rt.setRequestHeader('X-Request-Project', 'bo');
				rt.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
				rt.withCredentials = true;
				rt.send(upload); 
				rt.onreadystatechange = function() {
					if (4 == rt.readyState) {
						if (500 == rt.status) {
							api_socket.send('{"candle-history":{"data":[]}}');
						} else
						if (200 != rt.status) {
							api_socket.send('{"candle-history":"error"}');
							console.log(rt.status + ": " + rt.statusText);
							is_error = true;
						} else {
							console.log('{"candle-history":'+rt.responseText+'}');
							api_socket.send('{"candle-history":'+rt.responseText+'}');
						}
					}
				}
			} else
			/* если была получена не коамнда API, просто отсылаем данные */
			if(is_socket) {
				socket.send(t.data);
			}
        }, api_socket.onerror = function(t) {
            console.log("Ошибка (сервер API) " + t.message);
			if(is_socket) socket.close();
			close_all_quotes_stream();
			is_api_socket = false;
        }
    }
	
	chrome.storage.local.get("olymptradeapiwsport", function(result) {
		//var currentColor = 
		if(result.olymptradeapiwsport) {
			port = result.olymptradeapiwsport; 
		} else {
			port = 8080;
		}
		console.log('port: ' + port);
		connect_api();
    });
	
	chrome.storage.onChanged.addListener(function(changes, namespace) {
		for (var key in changes) {
			var storageChange = changes[key];
			console.log('Storage key "%s" in namespace "%s" changed. ' +
			'Old value was "%s", new value is "%s".',
			key,
			namespace,
			storageChange.oldValue,
			storageChange.newValue);
			if(key == "olymptradeapiwsport") {
				port = storageChange.newValue;
				if(is_api_socket) {
					is_api_socket.close();
					connect_api();
				}
				console.log('port: ' + port);
			}
		}
	});
/*
	var rt = new XMLHttpRequest;
	console.log("broker_domain " + broker_domain);
    rt.open("GET", "https://" + broker_domain + "/platform/state", !0), rt.send(), rt.onreadystatechange = function() {
		console.log("Пробуем GET!");
        if (4 == rt.readyState) {
            if (200 != rt.status) {
				console.log(rt.status + ": " + rt.statusText);
				is_error = true;
			} else {
				//console.log("Получены данные" + rt.responseText), 
				mes = JSON.parse(rt.responseText), 
				console.log(mes);
				//connect_api();
            }
        }
    }
	https://api.olymptrade.com/v1/cabinet/amount-limits
	{"data":[]}
	*/
}

function update_second() { 
	//console.log("СЕКУНДОЧШКУ А! " + getUuid());
}

setInterval(update_second, 1000);
//setInterval(update_10_second, 10000);// запускать функцию каждую секунду

function try_again() {
	console.log("try_again!");
	injected_main()
}

try_again();