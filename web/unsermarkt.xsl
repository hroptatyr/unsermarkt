<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="html"/>
  <xsl:param name="side"/>

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="quotes">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="b|a">
    <xsl:if test="name() = $side">
      <div class="d">
        <span class="p"><xsl:value-of select="@p"/></span>
        <span class="q"><xsl:value-of select="@q"/></span>
      </div>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
