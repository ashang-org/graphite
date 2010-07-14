#include "GrFace.h"
#include "VMScratch.h"
#include <string.h>
#include "Segment.h"
#include "XmlTraceLog.h"

using namespace org::sil::graphite::v2;

GrFace::~GrFace()
{
    delete[] m_glyphs2;
    delete[] m_silfs;
    m_glyphs2 = NULL;
    m_silfs = NULL;
}


bool GrFace::readGlyphs()
{
    size_t lHead, lLoca, lGlyf, lHmtx, lHHea, lGloc, lGlat, lMaxp;
    const void *pHead, *pHHea, *pLoca, *pGlyf, *pHmtx, *pGloc, *pGlat, *pMaxp;
    if ((pHead = getTable(tagHead, &lHead)) == NULL) return false;
    if ((pHHea = getTable(tagHhea, &lHHea)) == NULL) return false;
    if ((pLoca = getTable(tagLoca, &lLoca)) == NULL) return false;
    if ((pGlyf = getTable(tagGlyf, &lGlyf)) == NULL) return false;
    if ((pHmtx = getTable(tagHmtx, &lHmtx)) == NULL) return false;
    if ((pGloc = getTable(tagGloc, &lGloc)) == NULL) return false;
    if ((pGlat = getTable(tagGlat, &lGlat)) == NULL) return false;
    if ((pMaxp = getTable(tagMaxp, &lMaxp)) == NULL) return false;
    m_numGlyphs = TtfUtil::GlyphCount(pMaxp);
    m_upem = TtfUtil::DesignUnits(pHead);
    // m_glyphidx = new unsigned short[m_numGlyphs];        // only need this if doing occasional glyph reads

    int version = swap32(*((uint32 *)pGloc));
    if (version != 0x00010000) return false;
    if (lGloc < 6) return false;
    unsigned short locFlags = swap16(((uint16 *)pGloc)[2]);
    m_numAttrs = swap16(((uint16 *)pGloc)[3]);
    int nGlyphs;
    if (locFlags)
        nGlyphs = (lGloc - 10) / 4;
    else
        nGlyphs = (lGloc - 8) / 2;
    if (nGlyphs > m_numGlyphs) m_numGlyphs = nGlyphs;
    
    m_glyphs2 = new GlyphFace [m_numGlyphs];
#ifndef DISABLE_TRACING
    if (XmlTraceLog::get().active())
    {
        XmlTraceLog::get().openElement(ElementGlyphs);
        XmlTraceLog::get().addAttribute(AttrNum, nGlyphs);
    }
#endif
    for (int i = 0; i < m_numGlyphs; i++)
    {
        int nLsb, xMin, yMin, xMax, yMax, glocs, gloce;
        unsigned int nAdvWid;
        Position pos(0, 0);
        Rect boundingBox(pos, pos);
        GlyphFace *g;
        size_t locidx = TtfUtil::LocaLookup(i, pLoca, lLoca, pHead);
        void *pGlyph = TtfUtil::GlyfLookup(pGlyf, locidx);
        if (TtfUtil::HorMetrics(i, pHmtx, lHmtx, pHHea, nLsb, nAdvWid))
            pos = Position(nAdvWid, 0);
        if (TtfUtil::GlyfBox(pGlyph, xMin, yMin, xMax, yMax))
            boundingBox = Rect(Position(xMin, yMin), Position(xMax - xMin, yMax - yMin));
        g = new(m_glyphs2 + i) GlyphFace(pos, boundingBox);
#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().openElement(ElementGlyphFace);
            XmlTraceLog::get().addAttribute(AttrGlyphId, i);
            XmlTraceLog::get().addAttribute(AttrAdvanceX, g->theAdvance().x);
            XmlTraceLog::get().addAttribute(AttrAdvanceY, g->theAdvance().y);
        }
#endif
        if (i < nGlyphs)
        {
            if (locFlags & 1)
            {
                glocs = swap32(((uint32 *)pGloc)[2+i]);
                gloce = swap32(((uint32 *)pGloc)[3+i]);
            }
            else
            {
                glocs = swap16(((uint16 *)pGloc)[4+i]);
                gloce = swap16(((uint16 *)pGloc)[5+i]);
            }
            g->readAttrs(pGlat, glocs, gloce, m_numAttrs);
        }
#ifndef DISABLE_TRACING
        XmlTraceLog::get().closeElement(ElementGlyphFace);
#endif
    }
#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementGlyphs);
#endif
    return true;
}

bool GrFace::readGraphite()
{
    char *pSilf;
    size_t lSilf;
    if ((pSilf = (char *)getTable(tagSilf, &lSilf)) == NULL) return false;
    uint32 version;
    uint32 compilerVersion = 0; // wasn't set before GTF version 3
    uint32 offset32Pos = 2;
    version = swap32(*(uint32 *)pSilf);
    if (version < 0x00020000) return false;
    if (version >= 0x00030000)
    {
        compilerVersion = swap32(((uint32 *)pSilf)[1]);
        m_numSilf = swap16(((uint16 *)pSilf)[4]);
        offset32Pos = 3;
    }
    else
        m_numSilf = swap16(((uint16 *)pSilf)[2]);

#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().openElement(ElementSilf);
            XmlTraceLog::get().addAttribute(AttrMajor, version >> 16);
            XmlTraceLog::get().addAttribute(AttrMinor, version & 0xFFFF);
            XmlTraceLog::get().addAttribute(AttrCompilerMajor, compilerVersion >> 16);
            XmlTraceLog::get().addAttribute(AttrCompilerMinor, compilerVersion & 0xFFFF);
            XmlTraceLog::get().addAttribute(AttrNum, m_numSilf);
            if (m_numSilf == 0)
                XmlTraceLog::get().warning("No Silf subtables!");
        }
#endif

    m_silfs = new Silf[m_numSilf];
    for (int i = 0; i < m_numSilf; i++)
    {
        const uint32 offset = swap32(((uint32 *)pSilf)[offset32Pos + i]);
        uint32 next;
        if (i == m_numSilf - 1)
            next = lSilf;
        else
            next = swap32(((uint32 *)pSilf)[offset32Pos + 1 + i]);
        if (offset > lSilf || next > lSilf)
        {
#ifndef DISABLE_TRACING
            XmlTraceLog::get().error("Invalid table %d offset %d length %u", i, offset, lSilf);
            XmlTraceLog::get().closeElement(ElementSilf);
#endif
            return false;
        }
        if (!m_silfs[i].readGraphite((void *)((char *)pSilf + offset), next - offset, m_numGlyphs, version))
        {
#ifndef DISABLE_TRACING
            if (XmlTraceLog::get().active())
            {
                XmlTraceLog::get().error("Error reading Graphite subtable %d", i);
                XmlTraceLog::get().closeElement(ElementSilfSub); // for convenience
                XmlTraceLog::get().closeElement(ElementSilf);
            }
#endif
            return false;
        }
    }

#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementSilf);
#endif
    return true;
}

void GrFace::runGraphite(Segment *seg, const Silf *aSilf) const
{
    VMScratch vms;

    aSilf->runGraphite(seg, this, &vms);
}

const Silf *GrFace::chooseSilf(uint32 script) const
{
    if (m_numSilf == 0)
        return NULL;
    else if (m_numSilf == 1 || script == 0)
        return m_silfs;
    else // do more work here
        return m_silfs;
}

uint16 GrFace::getGlyphMetric(uint16 gid, uint8 metric) const
{
    switch ((enum metrics)metric)
    {
        case kgmetAscent : return m_ascent;
        case kgmetDescent : return m_descent;
        default: return glyph(gid)->getMetric(metric);
    }
}