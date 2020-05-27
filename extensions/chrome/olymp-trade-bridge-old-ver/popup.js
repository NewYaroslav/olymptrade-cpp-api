
function apply_settings() {
    var port = document.getElementById('port').value;
	console.log("port " + port);
	chrome.storage.local.set({"olymptradeapiwsport": port}, function() {
		console.log("Settings saved");
	});
 
    document.getElementById('port').innerHTML = port;
}


document.getElementById('ApplySettings').addEventListener('click', apply_settings);