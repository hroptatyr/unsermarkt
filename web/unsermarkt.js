var bids;
var asks;
var bdiv;
var adiv;
var conn;
var div_status;
var div_unsermarkt;
var dom_parser;

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

function crea_div(instr)
{
	var res;
	var sym = instr.getAttribute("sym");
	var descr = instr.getAttribute("descr");
	var child_q;
	var child_t;

	res = document.createElement("div");
	res.id = sym;
	res.innerHTML += "<span class='sym'>" + sym + "</span>";
	res.innerHTML += "<span class='descr'>" + descr + "</span>";

	child_q = document.createElement("div");
	child_q.className = "quo_list";
	child_t = document.createElement("div");
	child_t.className = "tra_list";

	res.appendChild(child_q);
	res.appendChild(child_t);
	return res;
}

function trav_quotes(snip, xp, node)
{
	var xpres = XPathResult.ANY_TYPE;
	var iter_bids = snip.evaluate("quotes/b", snip, null, xpres, xp);
	var iter_asks = snip.evaluate("quotes/a", snip, null, xpres, xp);
}

function trav_trades(snip, xp, div)
{
	var xpres = XPathResult.ANY_TYPE;
	var iter_bids = snip.evaluate("trades/t", snip, null, xpres, xp);
}

function trav_instrs(snip)
{
	var xpres = XPathResult.ANY_TYPE;
	var iter = snip.evaluate("//instr", snip, null, xpres, null);

	for (var i = iter.iterateNext(); i; i = iter.iterateNext()) {
		var sym = i.getAttribute("sym");
		var div_sym = document.getElementById(sym);
		// append the fucker if not already
		if (div_sym == null) {
			div_sym = crea_div(i);
			div_unsermarkt.appendChild(div_sym);
		}
		trav_quotes(snip, i, div_sym);
		trav_trades(snip, i, div_sym);
	}
	return;
}

function msg_evt(evt)
{
	var evt_xml;

	div_status.className = "standby";
	div_status.innerHTML = "incoming message " + ++conn.msgno;

	evt_xml = dom_parser.parseFromString(evt.data, "text/xml");
	trav_instrs(evt_xml);

	div_status.className = "success";
	div_status.innerHTML = "connected";
	return;
}

function blub_ws(ws_svc)
{
	div_status = document.getElementById("status");
	div_unsermarkt = document.getElementById("unsermarkt");
	if (window.WebSocket === undefined) {
		div_status.innerHTML = 'Sockets not supported';
		div_status.className = 'fail';
		return;
	}
	// open a new conn, uses global 'conn' object
	if (conn.readyState === undefined || conn.readyState > 1) {
		conn = new WebSocket(ws_svc);

		// callbacks
		conn.onopen = function(evt) {
			div_status.className = 'success';
			div_status.innerHTML = 'connected';
			conn.msgno = 0;
		};

		conn.onmessage = msg_evt;    

		conn.onclose = function(evt) {
			div_status.className = 'fail';
			div_status.innerHTML = 'disconnected';
		};
		// create our global dom parser
		dom_parser = new DOMParser();
	}
	return;
}

function get_xsl()
{
	var req = new XMLHttpRequest();
	req.open("GET", "unsermarkt.xsl", false);
	req.setRequestHeader("Content-Type","text/xml");
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

function blub(ws_svc)
{
	status = document.getElementById("status");
	bdiv = document.getElementById("s1b");
	adiv = document.getElementById("s1a");
	conn = {};
	bids = {};
	asks = {};

	// call the actual initialiser
	blub_ws(ws_svc);
	return;
}
