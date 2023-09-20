#ifndef MAPTOOL__MAP_GUG_H
#define MAPTOOL__MAP_GUG_H

#include <string>
#include <vector>
#include <tuple>
#include <iostream>
#include <fstream>
#include <cstdint>

#include "dummy_declarations.h"
#include <boost/thread/mutex.hpp>

//#include "rastermap.h"

struct EXPORT GUGHeader {
    float FileVersion;
    std::string MapInfo;
    std::string Title;
    unsigned int BkColor;
    std::string Gauges;

    GUGHeader() :
        FileVersion(0), MapInfo(), Title(), BkColor(0), Gauges()
        { };
    bool set_field(const std::string &key, const std::string &value);

    // Get the number of resolution steps (GUGMapInfo structures).
    unsigned int count_gauges();
};

struct EXPORT GUGMapInfo {
    std::string Type;
    std::string Path;
    std::string Ellipsoid;
    std::string Projection;
    double BaseMed;
    int Zone;
    double WorldOrgX;
    double WorldOrgY;
    double WPPX;
    double WPPY;
    double RADX;
    double RADY;
    unsigned int ImageWidth;
    unsigned int ImageHeight;

    double RADX_sin, RADX_cos, RADY_sin, RADY_cos;

    GUGMapInfo() :
        Type(), Path(), Ellipsoid(), Projection(), BaseMed(0), Zone(0),
        WorldOrgX(0), WorldOrgY(0), WPPX(0), WPPY(0), RADX(0), RADY(0),
        ImageWidth(0), ImageHeight(0),
        RADX_sin(0), RADX_cos(0), RADY_sin(0), RADY_cos(0)
        { };

    bool set_field(const std::string &key, const std::string &value);
    void complete_initialization();
    void Pixel_to_PCS(double x_px, double y_px,
                      double* x_pcs, double* y_pcs) const;
    void PCS_to_Pixel(double x_pcs, double y_pcs,
                      double* x_px, double* y_px) const;
};

class EXPORT GUGFile {
    public:
        explicit GUGFile(std::string fname);

        const std::string &Filename() const { return m_fname; };
        const GUGHeader &Header() const { return m_header; };
        unsigned int MapInfoCount() const { return m_mapinfos.size(); };
        const GUGMapInfo &MapInfo(unsigned int n) const;
        std::tuple<std::string, unsigned int> GupPath(unsigned int n) const;

        const std::string &RawDataString() const { return m_decoded_data; };
        unsigned int BestResolutionIndex() const;

    private:
        std::string m_fname;
        GUGHeader m_header;
        std::vector<GUGMapInfo> m_mapinfos;
        std::string m_decoded_data;

        void ParseINI(const std::string &data);
        std::string ResolvePath(const std::string &gup_name) const;

};

PACKED_STRUCT(
struct EXPORT GUPBitmapFileHdr {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
});

PACKED_STRUCT(
struct EXPORT GUPBitmapInfoHdr {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
});

PACKED_STRUCT(
struct GUPHeader {
    uint32_t unkn1;
    uint32_t unkn2;
    uint32_t compression;
    uint32_t tile_px_x;
    uint32_t tile_px_y;
    uint32_t unkn4;
});

PACKED_STRUCT(
struct GUPTileOffset {
    int64_t offset;
    int64_t length;
});

class EXPORT GUPImage {
    public:
        GUPImage(const std::string &fname, int index, int64_t foffset);
        GUPImage(const GUPImage &other);
        friend void swap(GUPImage &lhs, GUPImage &rhs) {
            using std::swap;  // Enable ADL.

            swap(lhs.m_file, rhs.m_file);
            swap(lhs.m_fname, rhs.m_fname);
            swap(lhs.m_findex, rhs.m_findex);
            swap(lhs.m_foffset, rhs.m_foffset);
            swap(lhs.m_bfh, rhs.m_bfh);
            swap(lhs.m_bih_buf, rhs.m_bih_buf);
            swap(lhs.m_bih, rhs.m_bih);
            swap(lhs.m_guphdr, rhs.m_guphdr);
            swap(lhs.m_tiles_x, rhs.m_tiles_x);
            swap(lhs.m_tiles_y, rhs.m_tiles_y);
            swap(lhs.m_tiles, rhs.m_tiles);
            swap(lhs.m_tile_index, rhs.m_tile_index);
            swap(lhs.m_topdown, rhs.m_topdown);
        }
        GUPImage& operator=(GUPImage other) {
            swap(*this, other);
            return *this;
        }

        PixelBuf LoadTile(long tx, long ty) const;
        std::string LoadCompressedTile(long tx, long ty) const;

        int AnnouncedWidth() const;
        int AnnouncedHeight() const;
        int RealWidth() const;
        int RealHeight() const;
        int TileWidth() const;
        int TileHeight() const;
        int NumTilesX() const;
        int NumTilesY() const;
        int64_t NextImageOffset() const;

    private:
        mutable std::ifstream m_file;
        std::string m_fname;
        int m_findex;
        int64_t m_foffset;

        GUPBitmapFileHdr m_bfh;
        std::string m_bih_buf;
        const struct GUPBitmapInfoHdr *m_bih;
        const GUPHeader *m_guphdr;

        unsigned int m_tiles_x, m_tiles_y, m_tiles;
        std::vector<GUPTileOffset> m_tile_index;
        bool m_topdown;

        void Init();
};


EXPORT GUPImage MakeGupImage(const std::string& path, unsigned int gup_image_idx);
EXPORT GUPImage MakeBestResolutionGupImage(const GUGFile &gugfile);


/** A map in GUG file format
 *
 * GUG files can in principle contain both normal topographic data and DEM
 * data. DEM's are currently not supported, though.
 *
 * @locking Concurrent `GetRegion` calls are enabled. Data access in
 * `GetRegion` is protected by a per-instance mutex `m_getregion_mutex`.
 * No external calls are made with this mutex held.
 */
class EXPORT GUGMap : public RasterMap {
    public:
        explicit GUGMap(const std::string &fname);
        ~GUGMap();

        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;

        // Get a specific area of the map.
        // pos: The topleft corner of the requested map area.
        // size: The dimensions of the requested map area.
        // The returned PixelBuf must have dimensions equal to size.
        virtual PixelBuf
        GetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const;

        virtual const std::string &GetFname() const;
        virtual const std::string &GetTitle() const;

        const GUGFile &GetGUGFile() const { return m_gugfile; };
        const GUPImage &GetGUPImage() const { return m_image; };
        const GUGHeader *GetGUGHeader() const { return m_gugheader; };
        const GUGMapInfo *GetGUGMapInfo() const { return m_gugmapinfo; };
    private:
        DISALLOW_COPY_AND_ASSIGN(GUGMap);

        mutable boost::mutex m_getregion_mutex;

        GUGFile m_gugfile;
        GUPImage m_image;
        const GUGHeader* m_gugheader;
        const GUGMapInfo* m_gugmapinfo;

        // sizes
        long m_tile_width, m_tile_height;
        long m_tiles_x, m_tiles_y;
        long m_width, m_height;
        long m_tilesize, m_tilesizergb;
};

#endif
