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

function make_instr_head(instr)
{
	var sym = instr.getAttribute("sym");
	var descr = instr.getAttribute("descr");
	var symsp = document.createElement("span");
	var descrsp = document.createElement("span");
	var res = document.createElement("div");

	res.className = "head";
	symsp.className = "sym";
	symsp.textContent = sym;
	descrsp.className = "descr";
	descrsp.textContent = descr;

	res.appendChild(symsp);
	res.appendChild(descrsp);
	return res;
}

function make_tick_div(tnode)
{
	var p = tnode.getAttribute("p");
	var q = tnode.getAttribute("q");
	var res = document.createElement("div");
	var ps = document.createElement("span");
	var qs = document.createElement("span");

	ps.className = "p";
	qs.className = "q";
	ps.textContent = p;
	qs.textContent = q;

	res.className = "tick";
	res.appendChild(ps);
	res.appendChild(qs);
	return res;
}

function make_tick_list(snip, cwd, expr, class)
{
	var xpres = XPathResult.ANY_TYPE;
	var iter = snip.evaluate(expr, cwd, null, xpres, null);
	var res = document.createElement("div");

	// this multi-class approach blows big donkey dong
	res.className = "tick_list";
	if (class) {
		res.className += " " + class;
	}
	for (var i = iter.iterateNext(); i; i = iter.iterateNext()) {
		var tdiv = make_tick_div(i);
		res.appendChild(tdiv);
	}
	return res;
}

function trav_instrs(snip)
{
	var xpres = XPathResult.ANY_TYPE;
	var iter = snip.evaluate("//instr", snip, null, xpres, null);

	for (var i = iter.iterateNext(); i; i = iter.iterateNext()) {
		var sym = i.getAttribute("sym");
		var old_sym = document.getElementById(sym);
		var btl, atl, ttl, head;
		var div_sym;

		// append the fucker if not already
		head = make_instr_head(i);
		// create the tick lists
		btl = make_tick_list(snip, i, "quotes/b", "bid_list");
		atl = make_tick_list(snip, i, "quotes/a", "ask_list");
		ttl = make_tick_list(snip, i, "trades/t", "tra_list");

		// assign class name and id
		div_sym = document.createElement("div");
		div_sym.id = sym;
		div_sym.className = "instr";
		// and glue them together
		div_sym.appendChild(head);
		div_sym.appendChild(btl);
		div_sym.appendChild(atl);
		div_sym.appendChild(ttl);

		// now replace old node
		if (old_sym) {
			div_unsermarkt.replaceChild(div_sym, old_sym);
		} else {
			div_unsermarkt.appendChild(div_sym);
		}
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
