var bids;
var asks;
var status;
var bdiv;
var adiv;
var conn;
var xsl;
var xsl_proc;

function clr()
{
	bids = {};
	asks = {};
}

function pub()
{
	var s1b = document.getElementById("s1b");
	var s1a = document.getElementById("s1a");

	s1b.innerHtml = '';
	s1a.innerHtml = '';
	for (var b in bids) {
		s1b.innerHtml += '<p>' + b + ' ' + bids[b] + '</p>';
	}
	for (var a in asks) {
		s1a.innerHtml += '<p>' + a + ' ' + asks[a] + '</p>';
	}
}

function b(p, q)
{
	bids[p] = q;
}

function a(p, q)
{
	asks[p] = q;
}

function msg_evt(evt)
{
	if (xsl_proc) {
		var dp = new DOMParser();
		var evt_xml = dp.parseFromString(evt.data, "text/xml");
		var bfrag = xsl_proc.transformToFragment(evt_xml, document);
		var afrag = xsl_proc.transformToFragment(evt_xml, document);

		bdiv.innerHTML = "";
		bdiv.appendChild(bfrag);

		adiv.innerHTML = "";
		adiv.appendChild(afrag);
	}
	status.innerHTML = "processed messages: " + ++conn.msgno;
	return;
}

function blub_ws()
{
	status = document.getElementById("status");
	if (window.WebSocket === undefined) {
		status.innerHTML = 'Sockets not supported';
		status.className = 'fail';
		return;
	}
	// open a new conn, uses global 'conn' object
	if (conn.readyState === undefined || conn.readyState > 1) {
		var src = 'ws://www.unserding.org:8787'
		conn = new WebSocket(src);

		// callbacks
		conn.onopen = function(evt) {
			status.className = 'success';
			status.innerHTML = 'connected';
			conn.msgno = 0;
		};

		conn.onmessage = msg_evt;    

		conn.onclose = function(evt) {
			status.className = 'fail';
			status.innerHTML = 'disconnected';
		};
	}
	return;
}

function get_xsl()
{
	var req = new XMLHttpRequest();
	req.open("GET", "unsermarkt.xsl", false);
	req.send(null);
	if (req.readyState == 4 && req.status == 200) {
		if ((xsl = req.responseXML) == null) {
			var dp = new DOMParser();
			xsl = dp.parseFromString(req.responseText, "text/xml");
		}
		xsl_proc = new XSLTProcessor();
		xsl_proc.importStylesheet(xsl);
	}
	return;
}

function blub()
{
	status = document.getElementById("status");
	bdiv = document.getElementById("s1b");
	adiv = document.getElementById("s1a");
	conn = {};
	bids = {};
	asks = {};

	// obtain the xsl synchronously
	get_xsl();

	// call the actual initialiser
	blub_ws();
	return;
}
