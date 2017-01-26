<!doctype html>
<html>
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width,shrink-to-fit=no,user-scalable=no,initial-scale=1.0,maximum-scale=1.0,minimum-scale=1.0">

	<title>WiFi Settings - ESP8266 Remote Terminal</title>

	<link rel="stylesheet" href="/css/app.css">
	<script src="/js/app.js"></script>
</head>
<body class="page-wifi">

<img src="/img/loader.gif" alt="Loading…" id="loader">

<h1 onclick="location.href='/'">WiFi settings</h1>

<div class="Box">
	<table>
		<tr>
			<th>WiFi mode:</th>
			<td>%WiFiMode%</td>
		</tr>
		<tr>
			<th></th>
			<td>%WiFiapwarn%</td>
		</tr>
		<tr>
			<th><label for="channel">AP channel:</label></th>
			<td>
				<form action="/wifi/setchannel" method="GET">
					<input name="ch" id="channel" type="number" step=1 min=1 max=14 value="%WiFiChannel%">&nbsp;
					<input type="submit" value="Set" class="narrow btn-green">
				</form>
			</td>
		</tr>
		<tr>
			<th><label for="channel">AP name:</label></th>
			<td>
				<form action="/wifi/setname" method="GET">
					<input name="name" type="text" value="%APName%">&nbsp;
					<input type="submit" value="Set" class="narrow btn-green">
				</form>
			</td>
		</tr>
	</table>
</div>

<div class="Box" id="ap-box">
	<h2>Select AP to join</h2>
	<div id="ap-loader">Scanning<span class="anim-dots">.</span></div>
	<div id="ap-list" style="display:none"></div>
</div>

<script>
	_root = window.location.host;
	wifiInit({curSSID: '%currSsid%'});
</script>
</body>
</html>
