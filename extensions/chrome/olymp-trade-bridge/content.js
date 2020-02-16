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

//function send_data()  {
//	socket.send('[{"t":2,"e":23,"d":[{"amount":'+amount+',"dir":"'+trend+'","pair":"'+pair+'","pos":0,"source":"platform","group":"'+group+'","duration":'+time+"}]}]");
//}

function getUuid() {
    return(Date.now().toString(36)+Math.random().toString(36).substr(2,12)).toUpperCase()
}

function injected_main() {
	console.log("Olymp Trade Bridge launched");
	
	var broker_domain = document.domain;

	// функция для запуска потока котировок
	function quotes_stream(new_socket, symbol_name, to_timestamp) {
		new_socket = new WebSocket("wss://" + broker_domain + "/ds/v5"), new_socket.onopen = function() {
            console.log("Соединение " + symbol_name + " установлено."), 
			new_socket.send('[{"t":1,"e":105,"d":[{"source":"platform"}]}]'); 
			new_socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');
			new_socket.send('[{"t":2,"e":4,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60}]}]');
			new_socket.send('[{"t":2,"e":3,"uuid":"' + getUuid() + '","d":[{"p":"' + symbol_name + '","tf":60,"to":' + to_timestamp + ',"solid":true}]}]');
        }, new_socket.onclose = function(t) {
			// заново открываем соединение
            if(is_api_socket) {
				quotes_stream(new_socket, symbol_name, to_timestamp);
			}
			t.wasClean ? console.log("Соединение " + symbol_name + " закрыто чисто") : console.log("Обрыв соединения"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
        },  new_socket.onmessage = function(t) {
			if(is_api_socket) {
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
		socket_array.forEach(function(item, index, array) {
			if(socket_array[index]) socket_array[index].close();
		});
	}
	
	function connect_broker() {
        socket = new WebSocket("wss://" + broker_domain + "/ds/v5"), socket.onopen = function() {
            console.log("Соединение установлено."), 
			socket.send('[{"t":1,"e":105,"d":[{"source":"platform"}]}]'); 
			socket.send('[{"t":2,"e":90,"uuid":"' + getUuid() + '"}]');

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
			//console.log("broker_domain " + broker_domain);
			rt.open("GET", "https://" + broker_domain + "/platform/state", !0), rt.send(), rt.onreadystatechange = function() {
				if (4 == rt.readyState) {
					if (200 != rt.status) {
						console.log(rt.status + ": " + rt.statusText);
						api_socket.send('{"connection_status":"error"}');
						is_error = true;
					} else {
						if(is_api_socket) {
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
			/* поменять тип счета */
			if(api_message.cmd == "set-money-group") {
				var group = api_message.group;
				var account_id = api_message.account_id;
				var rt = new XMLHttpRequest;
				var upload = '{"group":"' + group + '","account_id":' + account_id + '}';
				console.log("json_upload " + upload);
				
				rt.open("POST", "https://api.olymptrade.com/v4/user/set-money-group", !0), 
				rt.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
				rt.setRequestHeader('Accept', 'application/json, text/plain, */*');
				rt.setRequestHeader('X-Request-Type', 'Api-Request');
				rt.setRequestHeader('X-Request-Project', 'bo');
				rt.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
				rt.send(upload), 
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
			/* если была получена не коамнда API, просто отсылаем данные */
			if(is_socket) {
				socket.send(t.data);
			}
        }, api_socket.onerror = function(t) {
            console.log("Ошибка (сервер API) " + t.message);
			if(is_socket) socket.close();
			is_api_socket = false;
        }
    }
	
	//connect_api();
	
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
	*/
}

function update_second() { 
	//console.log("СЕКУНДОЧШКУ А! " + getUuid());
	//console.log("СЕКУНДОЧШКУ А! " + Date.now());
	//console.log("СЕКУНДОЧШКУ А! " + Date.now().toString(36));
	//console.log("СЕКУНДОЧШКУ А! " + Math.random().toString(36).substr(2,12));
}

function update_10_second() { 
	console.log("СЕКУНДОЧШКУ А 10!");
	
	if(is_socket) {
		//socket.send('[{"t":2,"e":23,"uuid":"' + getUuid() + '","d":[{"amount":300,"dir":"up","pair":"Bitcoin","cat":"digital","pos":0,"source":"platform","account_id":0,"group":"demo","timestamp":' + Date.now() + ',"duration":180}]}]');
	}

}

setInterval(update_second, 1000);
setInterval(update_10_second, 10000);// запускать функцию каждую секунду

function try_again() {
	console.log("try_again!");
	injected_main()
}

try_again();