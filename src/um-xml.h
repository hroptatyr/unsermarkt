/* abstract from the xml lib in use */

#if !defined INCLUDED_um_xml_h_
#define INCLUDED_um_xml_h_

#if defined USE_MXML || 1
# include <mxml.h>

typedef mxml_node_t *umx_node_t;

static inline umx_node_t
umx_make_doc(const char *ver)
{
	return mxmlNewXML(ver);
}

static inline void
umx_free_doc(umx_node_t node)
{
	mxmlDelete(node);
	return;
}

static inline umx_node_t
umx_add_node(umx_node_t parent, const char *name)
{
	return mxmlNewElement(parent, name);
}

static inline void
umx_add_attr(umx_node_t node, const char *name, const char *value)
{
	mxmlElementSetAttr(node, name, value);
	return;
}

/* serialiser */
static inline size_t
umx_seria(umx_node_t doc, char *buf, size_t bsz)
{
	return mxmlSaveString(doc, buf, bsz, MXML_NO_CALLBACK);
}
#endif	/* USE_MXML */

#endif	/* INCLUDED_um_xml_h_ */
