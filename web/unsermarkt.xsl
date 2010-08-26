<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="html"/>

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="quotes">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="b|a">
    <div class="d">
      <span class="p"><xsl:value-of select="@p"/></span>
      <span class="q"><xsl:value-of select="@q"/></span>
    </div>
  </xsl:template>

</xsl:stylesheet>
