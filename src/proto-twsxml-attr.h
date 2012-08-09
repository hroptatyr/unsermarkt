#if !defined INCLUDED_proto_twsxml_attr_h_
#define INCLUDED_proto_twsxml_attr_h_

typedef enum {
	/* must be first */
	TX_ATTR_UNK,
	/* alphabetic list of tags */
	TX_ATTR_ACCOUNT,
	TX_ATTR_ACTION,
	TX_ATTR_AUXPRICE,
	TX_ATTR_CURRENCY,
	TX_ATTR_EXCHANGE,
	TX_ATTR_LMTPRICE,
	TX_ATTR_OCAGROUP,
	TX_ATTR_ORDEREXCHANGE,
	TX_ATTR_ORDERID,
	TX_ATTR_ORDERTYPE,
	TX_ATTR_SECTYPE,
	TX_ATTR_SYMBOL,
	TX_ATTR_TIF,
	TX_ATTR_TOTALQUANTITY,
	TX_ATTR_TYPE,
	TX_ATTR_XMLNS,

} tws_xml_aid_t;

#endif	/* INCLUDED_proto_twsxml_attr_h_ */