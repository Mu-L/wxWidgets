///////////////////////////////////////////////////////////////////////////////
// Name:        tests/image/image.cpp
// Purpose:     Test wxImage
// Author:      Francesco Montorsi
// Created:     2009-05-31
// Copyright:   (c) 2009 Francesco Montorsi
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "testprec.h"

#if wxUSE_IMAGE


#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/anidecod.h" // wxImageArray
#include "wx/bitmap.h"
#include "wx/cursor.h"
#include "wx/icon.h"
#include "wx/palette.h"
#include "wx/url.h"
#include "wx/log.h"
#include "wx/mstream.h"
#include "wx/zstream.h"
#include "wx/wfstream.h"
#include "wx/clipbrd.h"
#include "wx/dataobj.h"
#include "wx/utils.h"

// Check if we can use wxDIB::ConvertToBitmap(), which only exists for MSW and
// which assumes the target is little-endian (matching the file format)
#if defined(__WXMSW__) && wxUSE_WXDIB && wxBYTE_ORDER == wxLITTLE_ENDIAN
    #define CAN_LOAD_BITMAP_DIRECTLY

    #include "wx/msw/dib.h"
#endif

#include "testimage.h"

#include <memory>

#define CHECK_EQUAL_COLOUR_RGB(c1, c2) \
    CHECK( (int)c1.Red()   == (int)c2.Red() ); \
    CHECK( (int)c1.Green() == (int)c2.Green() ); \
    CHECK( (int)c1.Blue()  == (int)c2.Blue() )

#define CHECK_EQUAL_COLOUR_RGBA(c1, c2) \
    CHECK( (int)c1.Red()   == (int)c2.Red() ); \
    CHECK( (int)c1.Green() == (int)c2.Green() ); \
    CHECK( (int)c1.Blue()  == (int)c2.Blue() ); \
    CHECK( (int)c1.Alpha() == (int)c2.Alpha() )

struct testData {
    const char* file;
    wxBitmapType type;
    unsigned bitDepth;
} g_testfiles[] =
{
#if wxUSE_LIBWEBP
    { "horse.webp", wxBITMAP_TYPE_WEBP, 24 },
#endif // wxUSE_LIBWEBP
    { "horse.ico", wxBITMAP_TYPE_ICO, 4 },
    { "horse.xpm", wxBITMAP_TYPE_XPM, 8 },
    { "horse.png", wxBITMAP_TYPE_PNG, 24 },
    { "horse.ani", wxBITMAP_TYPE_ANI, 24 },
    { "horse.bmp", wxBITMAP_TYPE_BMP, 8 },
    { "horse.cur", wxBITMAP_TYPE_CUR, 1 },
    { "horse.gif", wxBITMAP_TYPE_GIF, 8 },
    { "horse.jpg", wxBITMAP_TYPE_JPEG, 24 },
    { "horse.pcx", wxBITMAP_TYPE_PCX, 8 },
    { "horse.pnm", wxBITMAP_TYPE_PNM, 24 },
    { "horse.tga", wxBITMAP_TYPE_TGA, 8 },
    { "horse.tif", wxBITMAP_TYPE_TIFF, 8 }
};


class ImageHandlersInit
{
public:
    ImageHandlersInit();

private:
    static bool ms_initialized;

    wxDECLARE_NO_COPY_CLASS(ImageHandlersInit);
};

bool ImageHandlersInit::ms_initialized = false;

ImageHandlersInit::ImageHandlersInit()
{
    if ( ms_initialized )
        return;

    ms_initialized = true;

    // the formats we're going to test:
#if wxUSE_LIBWEBP
    wxImage::AddHandler(new wxWEBPHandler);
#endif // wxUSE_LIBWEBP
    wxImage::AddHandler(new wxICOHandler);
    wxImage::AddHandler(new wxXPMHandler);
    wxImage::AddHandler(new wxPNGHandler);
    wxImage::AddHandler(new wxANIHandler);
    wxImage::AddHandler(new wxBMPHandler);
    wxImage::AddHandler(new wxCURHandler);
#if wxUSE_GIF
    wxImage::AddHandler(new wxGIFHandler);
#endif // wxUSE_GIF
    wxImage::AddHandler(new wxJPEGHandler);
    wxImage::AddHandler(new wxPCXHandler);
    wxImage::AddHandler(new wxPNMHandler);
    wxImage::AddHandler(new wxTGAHandler);
#if wxUSE_LIBTIFF
    wxImage::AddHandler(new wxTIFFHandler);
#endif // wxUSE_LIBTIFF
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::LoadFromFile", "[image]")
{
    wxImage img;
    for (unsigned int i=0; i<WXSIZEOF(g_testfiles); i++)
    {
        const wxString file(g_testfiles[i].file);
        INFO("Loading " << file);
        CHECK(img.LoadFile(file));
    }

    CHECK(img.LoadFile("image/bitfields.bmp", wxBITMAP_TYPE_BMP));
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::LoadFromSocketStream", "[image]")
{
    // This test doesn't work any more even using the IP address below as the
    // HTTP server now redirects everything to HTTPs, so skip it for now unless
    // a test URL pointing to a PNG image is defined.
    wxString urlStr;
    if ( !wxGetEnv("WX_TEST_IMAGE_URL_PNG", &urlStr) )
        return;

    wxSocketInitializer socketInit;

    wxURL url(urlStr);
    REQUIRE( url.GetError() == wxURL_NOERR );

    std::unique_ptr<wxInputStream> in_stream(url.GetInputStream());
    REQUIRE( in_stream );
    REQUIRE( in_stream->IsOk() );

    wxImage img;

    // NOTE: it's important to inform wxImage about the type of the image being
    //       loaded otherwise it will try to autodetect the format, but that
    //       requires a seekable stream!
    CHECK( img.LoadFile(*in_stream, wxBITMAP_TYPE_PNG) );
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::LoadFromZipStream", "[image]")
{
    for (unsigned int i=0; i<WXSIZEOF(g_testfiles); i++)
    {
        SECTION(std::string("Testing file ") + g_testfiles[i].file)
        {
            switch (g_testfiles[i].type)
            {
                case wxBITMAP_TYPE_XPM:
                case wxBITMAP_TYPE_GIF:
                case wxBITMAP_TYPE_PCX:
                case wxBITMAP_TYPE_TGA:
                case wxBITMAP_TYPE_TIFF:
                continue;       // skip testing those wxImageHandlers which cannot
                                // load data from non-seekable streams

                default:
                    ; // proceed
            }

            // compress the test file on the fly:
            wxMemoryOutputStream memOut;
            {
                wxFileInputStream file(g_testfiles[i].file);
                REQUIRE(file.IsOk());

                wxZlibOutputStream compressFilter(memOut, 5, wxZLIB_GZIP);
                REQUIRE(compressFilter.IsOk());

                file.Read(compressFilter);
                REQUIRE(file.GetLastError() == wxSTREAM_EOF);
            }

            // now fetch the compressed memory to wxImage, decompressing it on the fly; this
            // allows us to test loading images from non-seekable streams other than socket streams
            wxMemoryInputStream memIn(memOut);
            REQUIRE(memIn.IsOk());
            wxZlibInputStream decompressFilter(memIn, wxZLIB_GZIP);
            REQUIRE(decompressFilter.IsOk());

            wxImage img;

            // NOTE: it's important to inform wxImage about the type of the image being
            //       loaded otherwise it will try to autodetect the format, but that
            //       requires a seekable stream!
            CHECK( img.LoadFile(decompressFilter, g_testfiles[i].type) );
        }
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::SizeImage", "[image]")
{
   // Test the wxImage::Size() function which takes a rectangle from source and
   // places it in a new image at a given position. This test checks, if the
   // correct areas are chosen, and clipping is done correctly.

   // our test image:
   static const char * xpm_orig[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "     .....",
      " ++++@@@@.",
      " +...   @.",
      " +.@@++ @.",
      " +.@ .+ @.",
      ".@ +. @.+ ",
      ".@ ++@@.+ ",
      ".@   ...+ ",
      ".@@@@++++ ",
      ".....     "
   };
   // the expected results for all tests:
   static const char * xpm_l_t[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "...   @.BB",
      ".@@++ @.BB",
      ".@ .+ @.BB",
      " +. @.+ BB",
      " ++@@.+ BB",
      "   ...+ BB",
      "@@@++++ BB",
      "...     BB",
      "BBBBBBBBBB",
      "BBBBBBBBBB"
   };
   static const char * xpm_t[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " +...   @.",
      " +.@@++ @.",
      " +.@ .+ @.",
      ".@ +. @.+ ",
      ".@ ++@@.+ ",
      ".@   ...+ ",
      ".@@@@++++ ",
      ".....     ",
      "BBBBBBBBBB",
      "BBBBBBBBBB"
   };
   static const char * xpm_r_t[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB +...   ",
      "BB +.@@++ ",
      "BB +.@ .+ ",
      "BB.@ +. @.",
      "BB.@ ++@@.",
      "BB.@   ...",
      "BB.@@@@+++",
      "BB.....   ",
      "BBBBBBBBBB",
      "BBBBBBBBBB"
   };
   static const char * xpm_l[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "   .....BB",
      "+++@@@@.BB",
      "...   @.BB",
      ".@@++ @.BB",
      ".@ .+ @.BB",
      " +. @.+ BB",
      " ++@@.+ BB",
      "   ...+ BB",
      "@@@++++ BB",
      "...     BB"
   };
   static const char * xpm_r[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB     ...",
      "BB ++++@@@",
      "BB +...   ",
      "BB +.@@++ ",
      "BB +.@ .+ ",
      "BB.@ +. @.",
      "BB.@ ++@@.",
      "BB.@   ...",
      "BB.@@@@+++",
      "BB.....   "
   };
   static const char * xpm_l_b[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBB",
      "BBBBBBBBBB",
      "   .....BB",
      "+++@@@@.BB",
      "...   @.BB",
      ".@@++ @.BB",
      ".@ .+ @.BB",
      " +. @.+ BB",
      " ++@@.+ BB",
      "   ...+ BB"
   };
   static const char * xpm_b[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBB",
      "BBBBBBBBBB",
      "     .....",
      " ++++@@@@.",
      " +...   @.",
      " +.@@++ @.",
      " +.@ .+ @.",
      ".@ +. @.+ ",
      ".@ ++@@.+ ",
      ".@   ...+ "
   };
   static const char * xpm_r_b[] = {
      "10 10 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBB",
      "BBBBBBBBBB",
      "BB     ...",
      "BB ++++@@@",
      "BB +...   ",
      "BB +.@@++ ",
      "BB +.@ .+ ",
      "BB.@ +. @.",
      "BB.@ ++@@.",
      "BB.@   ..."
   };
   static const char * xpm_sm[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "     .....",
      " ++++@@@",
      " +...   ",
      " +.@@++ ",
      " +.@ .+ ",
      ".@ +. @.",
      ".@ ++@@.",
      ".@   ..."
   };
   static const char * xpm_gt[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "     .....BB",
      " ++++@@@@.BB",
      " +...   @.BB",
      " +.@@++ @.BB",
      " +.@ .+ @.BB",
      ".@ +. @.+ BB",
      ".@ ++@@.+ BB",
      ".@   ...+ BB",
      ".@@@@++++ BB",
      ".....     BB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_l_t[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "...   @.BBBB",
      ".@@++ @.BBBB",
      ".@ .+ @.BBBB",
      " +. @.+ BBBB",
      " ++@@.+ BBBB",
      "   ...+ BBBB",
      "@@@++++ BBBB",
      "...     BBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_l[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "   .....BBBB",
      "+++@@@@.BBBB",
      "...   @.BBBB",
      ".@@++ @.BBBB",
      ".@ .+ @.BBBB",
      " +. @.+ BBBB",
      " ++@@.+ BBBB",
      "   ...+ BBBB",
      "@@@++++ BBBB",
      "...     BBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_l_b[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "   .....BBBB",
      "+++@@@@.BBBB",
      "...   @.BBBB",
      ".@@++ @.BBBB",
      ".@ .+ @.BBBB",
      " +. @.+ BBBB",
      " ++@@.+ BBBB",
      "   ...+ BBBB",
      "@@@++++ BBBB",
      "...     BBBB"
   };
   static const char * xpm_gt_l_bb[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "   .....BBBB",
      "+++@@@@.BBBB",
      "...   @.BBBB",
      ".@@++ @.BBBB",
      ".@ .+ @.BBBB",
      " +. @.+ BBBB",
      " ++@@.+ BBBB",
      "   ...+ BBBB"
   };
   static const char * xpm_gt_t[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " +...   @.BB",
      " +.@@++ @.BB",
      " +.@ .+ @.BB",
      ".@ +. @.+ BB",
      ".@ ++@@.+ BB",
      ".@   ...+ BB",
      ".@@@@++++ BB",
      ".....     BB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_b[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "     .....BB",
      " ++++@@@@.BB",
      " +...   @.BB",
      " +.@@++ @.BB",
      " +.@ .+ @.BB",
      ".@ +. @.+ BB",
      ".@ ++@@.+ BB",
      ".@   ...+ BB",
      ".@@@@++++ BB",
      ".....     BB"
   };
   static const char * xpm_gt_bb[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "     .....BB",
      " ++++@@@@.BB",
      " +...   @.BB",
      " +.@@++ @.BB",
      " +.@ .+ @.BB",
      ".@ +. @.+ BB",
      ".@ ++@@.+ BB",
      ".@   ...+ BB"
   };
   static const char * xpm_gt_r_t[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB +...   @.",
      "BB +.@@++ @.",
      "BB +.@ .+ @.",
      "BB.@ +. @.+ ",
      "BB.@ ++@@.+ ",
      "BB.@   ...+ ",
      "BB.@@@@++++ ",
      "BB.....     ",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_r[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB     .....",
      "BB ++++@@@@.",
      "BB +...   @.",
      "BB +.@@++ @.",
      "BB +.@ .+ @.",
      "BB.@ +. @.+ ",
      "BB.@ ++@@.+ ",
      "BB.@   ...+ ",
      "BB.@@@@++++ ",
      "BB.....     ",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_r_b[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BB     .....",
      "BB ++++@@@@.",
      "BB +...   @.",
      "BB +.@@++ @.",
      "BB +.@ .+ @.",
      "BB.@ +. @.+ ",
      "BB.@ ++@@.+ ",
      "BB.@   ...+ ",
      "BB.@@@@++++ ",
      "BB.....     "
   };
   static const char * xpm_gt_r_bb[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BB     .....",
      "BB ++++@@@@.",
      "BB +...   @.",
      "BB +.@@++ @.",
      "BB +.@ .+ @.",
      "BB.@ +. @.+ ",
      "BB.@ ++@@.+ ",
      "BB.@   ...+ "
   };
   static const char * xpm_gt_rr_t[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBB +...   ",
      "BBBB +.@@++ ",
      "BBBB +.@ .+ ",
      "BBBB.@ +. @.",
      "BBBB.@ ++@@.",
      "BBBB.@   ...",
      "BBBB.@@@@+++",
      "BBBB.....   ",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_rr[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBB     ...",
      "BBBB ++++@@@",
      "BBBB +...   ",
      "BBBB +.@@++ ",
      "BBBB +.@ .+ ",
      "BBBB.@ +. @.",
      "BBBB.@ ++@@.",
      "BBBB.@   ...",
      "BBBB.@@@@+++",
      "BBBB.....   ",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB"
   };
   static const char * xpm_gt_rr_b[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBB     ...",
      "BBBB ++++@@@",
      "BBBB +...   ",
      "BBBB +.@@++ ",
      "BBBB +.@ .+ ",
      "BBBB.@ +. @.",
      "BBBB.@ ++@@.",
      "BBBB.@   ...",
      "BBBB.@@@@+++",
      "BBBB.....   "
   };
   static const char * xpm_gt_rr_bb[] = {
      "12 12 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBBBBBBBBBB",
      "BBBB     ...",
      "BBBB ++++@@@",
      "BBBB +...   ",
      "BBBB +.@@++ ",
      "BBBB +.@ .+ ",
      "BBBB.@ +. @.",
      "BBBB.@ ++@@.",
      "BBBB.@   ..."
   };
   static const char * xpm_sm_ll_tt[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " .+ @.BB",
      ". @.+ BB",
      "+@@.+ BB",
      " ...+ BB",
      "@++++ BB",
      ".     BB",
      "BBBBBBBB",
      "BBBBBBBB"
   };
   static const char * xpm_sm_ll_t[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      ".   @.BB",
      "@++ @.BB",
      " .+ @.BB",
      ". @.+ BB",
      "+@@.+ BB",
      " ...+ BB",
      "@++++ BB",
      ".     BB"
   };
   static const char * xpm_sm_ll[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " .....BB",
      "+@@@@.BB",
      ".   @.BB",
      "@++ @.BB",
      " .+ @.BB",
      ". @.+ BB",
      "+@@.+ BB",
      " ...+ BB"
   };
   static const char * xpm_sm_ll_b[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBB",
      "BBBBBBBB",
      " .....BB",
      "+@@@@.BB",
      ".   @.BB",
      "@++ @.BB",
      " .+ @.BB",
      ". @.+ BB"
   };
   static const char * xpm_sm_l_tt[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      ".@ .+ @.",
      " +. @.+ ",
      " ++@@.+ ",
      "   ...+ ",
      "@@@++++ ",
      "...     ",
      "BBBBBBBB",
      "BBBBBBBB"
   };
   static const char * xpm_sm_l_t[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "...   @.",
      ".@@++ @.",
      ".@ .+ @.",
      " +. @.+ ",
      " ++@@.+ ",
      "   ...+ ",
      "@@@++++ ",
      "...     "
   };
   static const char * xpm_sm_l[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "   .....",
      "+++@@@@.",
      "...   @.",
      ".@@++ @.",
      ".@ .+ @.",
      " +. @.+ ",
      " ++@@.+ ",
      "   ...+ "
   };
   static const char * xpm_sm_l_b[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBB",
      "BBBBBBBB",
      "   .....",
      "+++@@@@.",
      "...   @.",
      ".@@++ @.",
      ".@ .+ @.",
      " +. @.+ "
   };
   static const char * xpm_sm_tt[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " +.@ .+ ",
      ".@ +. @.",
      ".@ ++@@.",
      ".@   ...",
      ".@@@@+++",
      ".....   ",
      "BBBBBBBB",
      "BBBBBBBB"
   };
   static const char * xpm_sm_t[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      " +...   ",
      " +.@@++ ",
      " +.@ .+ ",
      ".@ +. @.",
      ".@ ++@@.",
      ".@   ...",
      ".@@@@+++",
      ".....   "
   };
   static const char * xpm_sm_b[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBB",
      "BBBBBBBB",
      "     ...",
      " ++++@@@",
      " +...   ",
      " +.@@++ ",
      " +.@ .+ ",
      ".@ +. @."
   };
   static const char * xpm_sm_r_tt[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB +.@ .",
      "BB.@ +. ",
      "BB.@ ++@",
      "BB.@   .",
      "BB.@@@@+",
      "BB..... ",
      "BBBBBBBB",
      "BBBBBBBB"
   };
   static const char * xpm_sm_r_t[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB +... ",
      "BB +.@@+",
      "BB +.@ .",
      "BB.@ +. ",
      "BB.@ ++@",
      "BB.@   .",
      "BB.@@@@+",
      "BB..... "
   };
   static const char * xpm_sm_r[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BB     .",
      "BB ++++@",
      "BB +... ",
      "BB +.@@+",
      "BB +.@ .",
      "BB.@ +. ",
      "BB.@ ++@",
      "BB.@   ."
   };
   static const char * xpm_sm_r_b[] = {
      "8 8 5 1", "B c Black", "  c #00ff00", ". c #0000ff", "+ c #7f7f7f", "@ c #FF0000",
      "BBBBBBBB",
      "BBBBBBBB",
      "BB     .",
      "BB ++++@",
      "BB +... ",
      "BB +.@@+",
      "BB +.@ .",
      "BB.@ +. "
   };

   // this table defines all tests
   struct SizeTestData
   {
      int w, h, dx, dy;                // first parameters for Size()
      const char **ref_xpm;            // expected result
   } sizeTestData[] =
   {
      { 10, 10,  0,  0, xpm_orig},      // same size, same position
      { 12, 12,  0,  0, xpm_gt},       // target larger, same position
      {  8,  8,  0,  0, xpm_sm},       // target smaller, same position
      { 10, 10, -2, -2, xpm_l_t},      // same size, move left up
      { 10, 10, -2,  0, xpm_l},        // same size, move left
      { 10, 10, -2,  2, xpm_l_b},      // same size, move left down
      { 10, 10,  0, -2, xpm_t},        // same size, move up
      { 10, 10,  0,  2, xpm_b},        // same size, move down
      { 10, 10,  2, -2, xpm_r_t},      // same size, move right up
      { 10, 10,  2,  0, xpm_r},        // same size, move right
      { 10, 10,  2,  2, xpm_r_b},      // same size, move right down
      { 12, 12, -2, -2, xpm_gt_l_t},   // target larger, move left up
      { 12, 12, -2,  0, xpm_gt_l},     // target larger, move left
      { 12, 12, -2,  2, xpm_gt_l_b},   // target larger, move left down
      { 12, 12, -2,  4, xpm_gt_l_bb},  // target larger, move left down
      { 12, 12,  0, -2, xpm_gt_t},     // target larger, move up
      { 12, 12,  0,  2, xpm_gt_b},     // target larger, move down
      { 12, 12,  0,  4, xpm_gt_bb},    // target larger, move down
      { 12, 12,  2, -2, xpm_gt_r_t},   // target larger, move right up
      { 12, 12,  2,  0, xpm_gt_r},     // target larger, move right
      { 12, 12,  2,  2, xpm_gt_r_b},   // target larger, move right down
      { 12, 12,  2,  4, xpm_gt_r_bb},  // target larger, move right down
      { 12, 12,  4, -2, xpm_gt_rr_t},  // target larger, move right up
      { 12, 12,  4,  0, xpm_gt_rr},    // target larger, move right
      { 12, 12,  4,  2, xpm_gt_rr_b},  // target larger, move right down
      { 12, 12,  4,  4, xpm_gt_rr_bb}, // target larger, move right down
      {  8,  8, -4, -4, xpm_sm_ll_tt}, // target smaller, move left up
      {  8,  8, -4, -2, xpm_sm_ll_t},  // target smaller, move left up
      {  8,  8, -4,  0, xpm_sm_ll},    // target smaller, move left
      {  8,  8, -4,  2, xpm_sm_ll_b},  // target smaller, move left down
      {  8,  8, -2, -4, xpm_sm_l_tt},  // target smaller, move left up
      {  8,  8, -2, -2, xpm_sm_l_t},   // target smaller, move left up
      {  8,  8, -2,  0, xpm_sm_l},     // target smaller, move left
      {  8,  8, -2,  2, xpm_sm_l_b},   // target smaller, move left down
      {  8,  8,  0, -4, xpm_sm_tt},    // target smaller, move up
      {  8,  8,  0, -2, xpm_sm_t},     // target smaller, move up
      {  8,  8,  0,  2, xpm_sm_b},     // target smaller, move down
      {  8,  8,  2, -4, xpm_sm_r_tt},  // target smaller, move right up
      {  8,  8,  2, -2, xpm_sm_r_t},   // target smaller, move right up
      {  8,  8,  2,  0, xpm_sm_r},     // target smaller, move right
      {  8,  8,  2,  2, xpm_sm_r_b},   // target smaller, move right down
   };

   const wxImage src_img(xpm_orig);
   for ( unsigned i = 0; i < WXSIZEOF(sizeTestData); i++ )
   {
       SizeTestData& st = sizeTestData[i];
       wxImage
           actual(src_img.Size(wxSize(st.w, st.h), wxPoint(st.dx, st.dy), 0, 0, 0)),
           expected(st.ref_xpm);

       // to check results with an image viewer uncomment this:
       //actual.SaveFile(wxString::Format("imagetest-%02d-actual.png", i), wxBITMAP_TYPE_PNG);
       //expected.SaveFile(wxString::Format("imagetest-%02d-exp.png", i), wxBITMAP_TYPE_PNG);

       wxINFO_FMT("Resize test #%u: (%d, %d), (%d, %d)",
                   i, st.w, st.h, st.dx, st.dy);
       CHECK_THAT( actual, RGBSameAs(expected) );
   }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::CompareLoadedImage", "[image]")
{
    wxImage expected8("horse.xpm");
    REQUIRE( expected8.IsOk() );

    wxImage expected24("horse.png");
    REQUIRE( expected24.IsOk() );

    for (size_t i=0; i<WXSIZEOF(g_testfiles); i++)
    {
        if ( !(g_testfiles[i].bitDepth == 8 || g_testfiles[i].bitDepth == 24))
        {
            continue;
        }

        wxImage actual(g_testfiles[i].file);

        if ( actual.GetSize() != expected8.GetSize() )
        {
            continue;
        }


        wxINFO_FMT("Compare test '%s' for loading", g_testfiles[i].file);
        wxImage & expected = g_testfiles[i].bitDepth == 8 ? expected8 : expected24;
        int tolerance = 0;
        switch (g_testfiles[i].type)
        {
            case wxBITMAP_TYPE_JPEG:
            case wxBITMAP_TYPE_WEBP:
                tolerance = 42; // lossy formats can have a substantial difference
                // this value has been chosen to be okay for the existing JPEG sample
                break;
            default:
                tolerance = 0; // all other formats must match exactly
        }
        CHECK_THAT( actual, RGBSimilarTo( expected, tolerance ) );
    }

}

enum
{
    wxIMAGE_HAVE_ALPHA = (1 << 0),
    wxIMAGE_HAVE_PALETTE = (1 << 1),
    wxIMAGE_HAVE_DELTA_RLE_BITMAP = (1 << 2)
};

static
void CompareImage(const wxImageHandler& handler, const wxImage& image,
    int properties = 0, const wxImage *compareTo = nullptr)
{
    wxBitmapType type = handler.GetType();

    const bool testPalette = (properties & wxIMAGE_HAVE_PALETTE) != 0;
    /*
    This is getting messy and should probably be transformed into a table
    with image format features before it gets hairier.
    */
    if ( testPalette
        && ( !(type == wxBITMAP_TYPE_BMP
                || type == wxBITMAP_TYPE_GIF
                || type == wxBITMAP_TYPE_ICO
                || type == wxBITMAP_TYPE_PNG)
            || type == wxBITMAP_TYPE_XPM) )
    {
        return;
    }

    const bool testAlpha = (properties & wxIMAGE_HAVE_ALPHA) != 0;
    if (testAlpha
        && !(type == wxBITMAP_TYPE_PNG || type == wxBITMAP_TYPE_TGA
            || type == wxBITMAP_TYPE_TIFF) )
    {
        // don't test images with alpha if this handler doesn't support alpha
        return;
    }

    if (type == wxBITMAP_TYPE_JPEG /* skip lossy JPEG */
        || type == wxBITMAP_TYPE_WEBP /* skip lossy WebP */)
    {
        return;
    }

    wxMemoryOutputStream memOut;
    if ( !image.SaveFile(memOut, type) )
    {
        // Unfortunately we can't know if the handler just doesn't support
        // saving images, or if it failed to save.
        return;
    }

    unsigned bitsPerPixel = testPalette ? 8 : (testAlpha ? 32 : 24);
    wxINFO_FMT("Compare test '%s (%d-bit)' for saving",
               handler.GetExtension(), bitsPerPixel);

    wxMemoryInputStream memIn(memOut);
    REQUIRE(memIn.IsOk());

    wxImage actual(memIn);
    REQUIRE(actual.IsOk());

    const wxImage *expected = compareTo ? compareTo : &image;
    CHECK_THAT(actual, RGBSameAs(*expected));

#if wxUSE_PALETTE
    REQUIRE(actual.HasPalette()
        == (testPalette || type == wxBITMAP_TYPE_XPM));
#endif

    REQUIRE( actual.HasAlpha() == testAlpha);

    if (!testAlpha)
    {
        return;
    }

    wxINFO_FMT("Compare alpha test '%s' for saving", handler.GetExtension());
    CHECK_THAT(actual, RGBSameAs(*expected));
}

static void SetAlpha(wxImage *image)
{
    image->SetAlpha();

    unsigned char *ptr = image->GetAlpha();
    const int width = image->GetWidth();
    const int height = image->GetHeight();
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            ptr[y*width + x] = (x*y) & wxIMAGE_ALPHA_OPAQUE;
        }
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::CompareSavedImage", "[image]")
{
    wxImage expected24("horse.png");
    REQUIRE( expected24.IsOk() );
    REQUIRE( !expected24.HasAlpha() );

    wxImage expected8 = expected24.ConvertToGreyscale();

#if wxUSE_PALETTE
    unsigned char greys[256];
    for (int i = 0; i < 256; ++i)
    {
        greys[i] = i;
    }
    wxPalette palette(256, greys, greys, greys);
    expected8.SetPalette(palette);
#endif // #if wxUSE_PALETTE

    expected8.SetOption(wxIMAGE_OPTION_BMP_FORMAT, wxBMP_8BPP_PALETTE);

    // Create an image with alpha based on the loaded image
    wxImage expected32(expected24);

    SetAlpha(&expected32);

    const wxList& list = wxImage::GetHandlers();
    for ( wxList::compatibility_iterator node = list.GetFirst();
        node; node = node->GetNext() )
    {
        wxImageHandler *handler = (wxImageHandler *) node->GetData();

#if wxUSE_PALETTE
        CompareImage(*handler, expected8, wxIMAGE_HAVE_PALETTE);
#endif
        CompareImage(*handler, expected24);
        CompareImage(*handler, expected32, wxIMAGE_HAVE_ALPHA);
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::SavePNG", "[image]")
{
    wxImage expected24("horse.png");
    REQUIRE( expected24.IsOk() );
#if wxUSE_PALETTE
    REQUIRE( !expected24.HasPalette() );
#endif // #if wxUSE_PALETTE

    wxImage expected8 = expected24.ConvertToGreyscale();

    /*
    horse.png converted to greyscale should be saved without a palette.
    */
    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG), expected8);

    /*
    But if we explicitly ask for trying to save with a palette, it should work.
    */
    expected8.SetOption(wxIMAGE_OPTION_PNG_FORMAT, wxPNG_TYPE_PALETTE);

    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG),
        expected8, wxIMAGE_HAVE_PALETTE);


    REQUIRE( expected8.LoadFile("horse.gif") );
#if wxUSE_PALETTE
    REQUIRE( expected8.HasPalette() );
#endif // #if wxUSE_PALETTE

    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG),
        expected8, wxIMAGE_HAVE_PALETTE);

    /*
    Add alpha to the image in such a way that there will still be a maximum
    of 256 unique RGBA combinations. This should result in a saved
    PNG image still being palettised and having alpha.
    */
    expected8.SetAlpha();

    int x, y;
    const int width = expected8.GetWidth();
    const int height = expected8.GetHeight();
    for (y = 0; y < height; ++y)
    {
        for (x = 0; x < width; ++x)
        {
            expected8.SetAlpha(x, y, expected8.GetRed(x, y));
        }
    }

    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG),
        expected8, wxIMAGE_HAVE_ALPHA|wxIMAGE_HAVE_PALETTE);

    /*
    Now change the alpha of the first pixel so that we can't save palettised
    anymore because there will be 256+1 entries which is beyond PNGs limit
    of 256 entries.
    */
    expected8.SetAlpha(0, 0, 1);

    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG),
        expected8, wxIMAGE_HAVE_ALPHA);

    /*
    Even if we explicitly ask for saving palettised it should not be done.
    */
    expected8.SetOption(wxIMAGE_OPTION_PNG_FORMAT, wxPNG_TYPE_PALETTE);
    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_PNG),
        expected8, wxIMAGE_HAVE_ALPHA);

}

static void TestPNGDescription(const wxString& description)
{
    wxImage image("horse.png");

    image.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, description);
    wxMemoryOutputStream memOut;
    REQUIRE(image.SaveFile(memOut, wxBITMAP_TYPE_PNG));

    wxMemoryInputStream memIn(memOut);
    REQUIRE(image.LoadFile(memIn));

    CHECK(image.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION) == description);
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::PNGDescription", "[image]")
{
    // Test writing a description and reading it back.
    TestPNGDescription("Providing the PNG a pneumatic puma as a present");

    // Test writing and reading a description again but with non-ASCII characters.
    TestPNGDescription("Тестирование 테스트 一 二 三");

    // Test writing and reading a description again but with a long description.
    TestPNGDescription(wxString(wxT('a'), 256)
        + wxString(wxT('b'), 256)
        + wxString(wxT('c'), 256));
}

#if wxUSE_LIBTIFF
static void TestTIFFImage(const wxString& option, int value,
    const wxImage *compareImage = nullptr)
{
    INFO("Using option " << option << "=" << value);

    wxImage image;
    if (compareImage)
    {
        image = *compareImage;
    }
    else
    {
        (void) image.LoadFile("horse.png");
    }
    REQUIRE( image.IsOk() );

    wxMemoryOutputStream memOut;
    image.SetOption(option, value);

    REQUIRE(image.SaveFile(memOut, wxBITMAP_TYPE_TIFF));

    wxMemoryInputStream memIn(memOut);
    REQUIRE(memIn.IsOk());

    wxImage savedImage(memIn);
    REQUIRE(savedImage.IsOk());

    CHECK( savedImage.HasOption(option) );
    CHECK( savedImage.GetOptionInt(option) == value );
    CHECK( savedImage.HasAlpha() == image.HasAlpha() );
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::SaveTIFF", "[image]")
{
    TestTIFFImage(wxIMAGE_OPTION_TIFF_BITSPERSAMPLE, 1);
    TestTIFFImage(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL, 1);
    TestTIFFImage(wxIMAGE_OPTION_TIFF_PHOTOMETRIC, 0/*PHOTOMETRIC_MINISWHITE*/);
    TestTIFFImage(wxIMAGE_OPTION_TIFF_PHOTOMETRIC, 1/*PHOTOMETRIC_MINISBLACK*/);

    wxImage alphaImage("horse.png");
    REQUIRE( alphaImage.IsOk() );
    SetAlpha(&alphaImage);

    // RGB with alpha
    TestTIFFImage(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL, 4, &alphaImage);

    // Grey with alpha
    TestTIFFImage(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL, 2, &alphaImage);

    // B/W with alpha
    alphaImage.SetOption(wxIMAGE_OPTION_TIFF_BITSPERSAMPLE, 1);
    TestTIFFImage(wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL, 2, &alphaImage);
}
#endif // wxUSE_LIBTIFF

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::ReadCorruptedTGA", "[image]")
{
    static unsigned char corruptTGA[18+1+3] =
    {
        0,
        0,
        10, // RLE compressed image.
        0, 0,
        0, 0,
        0,
        0, 0,
        0, 0,
        1, 0, // Width is 1.
        1, 0, // Height is 1.
        24, // Bits per pixel.
        0,

        0xff, // Run length (repeat next pixel 127+1 times).
        0xff, 0xff, 0xff // One 24-bit pixel.
    };

    wxMemoryInputStream memIn(corruptTGA, WXSIZEOF(corruptTGA));
    REQUIRE(memIn.IsOk());

    wxImage tgaImage;
    REQUIRE( !tgaImage.LoadFile(memIn) );


    /*
    Instead of repeating a pixel 127+1 times, now tell it there will
    follow 127+1 uncompressed pixels (while we only should have 1 in total).
    */
    corruptTGA[18] = 0x7f;
    REQUIRE( !tgaImage.LoadFile(memIn) );
}

#if wxUSE_GIF

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::SaveAnimatedGIF", "[image]")
{
#if wxUSE_PALETTE
    wxImage image("horse.gif");
    REQUIRE( image.IsOk() );

    wxImageArray images;
    images.push_back(image);
    for (int i = 0; i < 4-1; ++i)
    {
        images.push_back( images[i].Rotate90() );

        images[i+1].SetPalette(images[0].GetPalette());
    }

    wxMemoryOutputStream memOut;
    REQUIRE( wxGIFHandler().SaveAnimation(images, &memOut) );

    wxGIFHandler handler;
    wxMemoryInputStream memIn(memOut);
    REQUIRE(memIn.IsOk());
    const int imageCount = handler.GetImageCount(memIn);
    CHECK( imageCount == 4 );

    for (int i = 0; i < imageCount; ++i)
    {
        wxFileOffset pos = memIn.TellI();
        REQUIRE( handler.LoadFile(&image, memIn, true, i) );
        memIn.SeekI(pos);

        wxINFO_FMT("Compare test for GIF frame number %d failed", i);
        CHECK_THAT(image, RGBSameAs(images[i]));
    }
#endif // #if wxUSE_PALETTE
}

static void TestGIFComment(const wxString& comment)
{
    wxImage image("horse.gif");

    image.SetOption(wxIMAGE_OPTION_GIF_COMMENT, comment);
    wxMemoryOutputStream memOut;
    REQUIRE(image.SaveFile(memOut, wxBITMAP_TYPE_GIF));

    wxMemoryInputStream memIn(memOut);
    REQUIRE( image.LoadFile(memIn) );

    CHECK( image.GetOption(wxIMAGE_OPTION_GIF_COMMENT) == comment );
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::GIFComment", "[image]")
{
    // Test reading a comment.
    wxImage image("horse.gif");
    CHECK( image.GetOption(wxIMAGE_OPTION_GIF_COMMENT) ==
           "  Imported from GRADATION image: gray" );


    // Test writing a comment and reading it back.
    TestGIFComment("Giving the GIF a gifted giraffe as a gift");


    // Test writing and reading a comment again but with a long comment.
    TestGIFComment(wxString(wxT('a'), 256)
        + wxString(wxT('b'), 256)
        + wxString(wxT('c'), 256));


    // Test writing comments in an animated GIF and reading them back.
    REQUIRE( image.LoadFile("horse.gif") );

#if wxUSE_PALETTE
    wxImageArray images;
    int i;
    for (i = 0; i < 4; ++i)
    {
        if (i)
        {
            images.push_back( images[i-1].Rotate90() );
            images[i].SetPalette(images[0].GetPalette());
        }
        else
        {
            images.push_back(image);
        }

        images[i].SetOption(wxIMAGE_OPTION_GIF_COMMENT,
            wxString::Format("GIF comment for frame #%d", i+1));

    }


    wxMemoryOutputStream memOut;
    REQUIRE( wxGIFHandler().SaveAnimation(images, &memOut) );

    wxGIFHandler handler;
    wxMemoryInputStream memIn(memOut);
    REQUIRE(memIn.IsOk());
    const int imageCount = handler.GetImageCount(memIn);
    for (i = 0; i < imageCount; ++i)
    {
        wxFileOffset pos = memIn.TellI();
        REQUIRE( handler.LoadFile(&image, memIn, true /*verbose?*/, i) );

        CHECK( image.GetOption(wxIMAGE_OPTION_GIF_COMMENT) ==
               wxString::Format("GIF comment for frame #%d", i+1) );
        memIn.SeekI(pos);
    }
#endif //wxUSE_PALETTE
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::BadGIF", "[image][gif][error]")
{
    wxImage image("image/bad_truncated.gif");
    REQUIRE( image.IsOk() );

    CHECK( image.GetSize() == wxSize(1200, 800) );
}

#endif // wxUSE_GIF

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::DibPadding", "[image]")
{
    /*
    There used to be an error with calculating the DWORD aligned scan line
    pitch for a BMP/ICO resulting in buffer overwrites (with at least MSVC9
    Debug this gave a heap corruption assertion when saving the mask of
    an ICO). Test for it here.
    */
    wxImage image("horse.gif");
    REQUIRE( image.IsOk() );

    image = image.Scale(99, 99);

    wxMemoryOutputStream memOut;
    REQUIRE( image.SaveFile(memOut, wxBITMAP_TYPE_ICO) );
}

static void CompareBMPImage(const wxString& file1, const wxString& file2)
{
    wxImage image1(file1);
    REQUIRE( image1.IsOk() );

    wxImage image2(file2);
    REQUIRE( image2.IsOk() );

    INFO("Comparing " << file1 << " and " << file2);
    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_BMP), image1, 0, &image2);
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::BMPFlippingAndRLECompression", "[image]")
{
    CompareBMPImage("image/horse_grey.bmp", "image/horse_grey_flipped.bmp");

    CompareBMPImage("image/horse_rle8.bmp", "image/horse_grey.bmp");
    CompareBMPImage("image/horse_rle8.bmp", "image/horse_rle8_flipped.bmp");

    CompareBMPImage("image/horse_rle4.bmp", "image/horse_rle4_flipped.bmp");

    CompareBMPImage("image/rle8-delta-320x240.bmp",
                    "image/rle8-delta-320x240-expected.bmp");
    CompareBMPImage("image/rle4-delta-320x240.bmp",
                    "image/rle8-delta-320x240-expected.bmp");
}

// If possible, check loading some BMP files by comparing loading them directly
// and via wxImage.
#ifdef CAN_LOAD_BITMAP_DIRECTLY

static wxImage ImageFromBMPFile(const wxString& filename)
{
    INFO("Loading pixel data from " << filename);
    wxFile file(filename);

    // Read the BMP file header
    const size_t hdrLen = 14;
    char hdr[hdrLen];
    ssize_t cbRead = file.Read(hdr, hdrLen);
    REQUIRE((size_t)cbRead == hdrLen);
    REQUIRE(hdr[0] == 'B');
    REQUIRE(hdr[1] == 'M');
    wxUint32 ofstDeclared = *(const wxUint32*)&hdr[10];

    // Now read the data
    size_t dataLen = file.Length() - hdrLen;
    std::vector<unsigned char> buf(dataLen);
    cbRead = file.Read(&buf[0], dataLen);
    REQUIRE((size_t)cbRead == dataLen);

    // Check there is no gap in the file between the end of the header
    // and the start of the pixel data. If there is, we can't tell
    // wxDIB::ConvertToBitmap() about it because we need to test its
    // code path where bits==nullptr, so we just have to fail the test
    const BITMAPINFO* pbmi = reinterpret_cast<const BITMAPINFO*>(&buf[0]);
    wxUint32 numColors = 0;
    if ( pbmi->bmiHeader.biCompression == BI_BITFIELDS )
    {
        if ( pbmi->bmiHeader.biSize == sizeof(BITMAPINFOHEADER) )
            numColors = 3;
    }
    else
    {
        if ( pbmi->bmiHeader.biClrUsed )
            numColors = pbmi->bmiHeader.biClrUsed;
        else if ( pbmi->bmiHeader.biBitCount <= 8 )
            numColors = 1 << pbmi->bmiHeader.biBitCount;
    }
    wxUint32 cbColorTable = numColors * sizeof(RGBQUAD);
    size_t ofstComputed = hdrLen + pbmi->bmiHeader.biSize + cbColorTable;
    REQUIRE(ofstComputed == ofstDeclared);

    // All good; now make an image out of it
    AutoHBITMAP ahbmp(wxDIB::ConvertToBitmap(pbmi));
    wxDIB dib(ahbmp);
    REQUIRE(dib.IsOk());

    return dib.ConvertToImage();
}

// Compare the results of loading a BMP using:
//  1. Windows' own conversion (GDI CreateDIBitmap() via wxDIB::ConvertToBitmap())
//  2. wxBMPHandler via wxImage::LoadFile()
//  3. wxBMPFileHandler via wxBitmap::LoadFile()
static void CompareBMPImageLoad(const wxString& filename, int properties = 0)
{
    INFO("Comparing loading methods for " << filename);

    wxImage image1 = ImageFromBMPFile(filename);
    REQUIRE( image1.IsOk() );

    wxImage image2(filename);
    REQUIRE( image2.IsOk() );

    wxBitmap bitmap3(filename, wxBITMAP_TYPE_BMP);
    REQUIRE( bitmap3.IsOk() );
    wxImage image3(bitmap3.ConvertToImage());
    REQUIRE( image3.IsOk() );

    // It is impossible (in general) for both of these
    // tests to pass on delta-RLE bitmaps. See #23638
    if ( !(properties & wxIMAGE_HAVE_DELTA_RLE_BITMAP) )
    {
        INFO("wxDIB::ConvertToBitmap vs wxImage::LoadFile");
        CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_BMP),
            image1, properties, &image2);
    }

    INFO("wxBitmap::LoadFile vs wxImage::LoadFile");
    CompareImage(*wxImage::FindHandler(wxBITMAP_TYPE_BMP),
        image3, properties, &image2);
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::BMPLoadMethod", "[image][bmp]")
{
    CompareBMPImageLoad("image/bitfields.bmp");
    // We would check the alpha on this one, but at the moment it fails
    // because of the way CompareImage() saves and reloads images,
    // which for BMPs does not preserve the alpha channel
    CompareBMPImageLoad("image/bitfields-alpha.bmp"/*, wxIMAGE_HAVE_ALPHA*/);
    CompareBMPImageLoad("image/horse_grey.bmp");
    CompareBMPImageLoad("image/horse_rle8.bmp");
    CompareBMPImageLoad("image/horse_rle4.bmp");
    if (!wxIsRunningUnderWine())
        CompareBMPImageLoad("image/rgb16-3103.bmp");
    CompareBMPImageLoad("image/rgb32-7187.bmp");
    CompareBMPImageLoad("image/rle8-delta-320x240.bmp",
        wxIMAGE_HAVE_DELTA_RLE_BITMAP);
    CompareBMPImageLoad("image/rle4-delta-320x240.bmp",
        wxIMAGE_HAVE_DELTA_RLE_BITMAP);
}

#endif // CAN_LOAD_BITMAP_DIRECTLY

static int
FindMaxChannelDiff(const wxImage& i1, const wxImage& i2)
{
    if ( i1.GetWidth() != i2.GetWidth() )
        return false;

    if ( i1.GetHeight() != i2.GetHeight() )
        return false;

    const unsigned char* p1 = i1.GetData();
    const unsigned char* p2 = i2.GetData();
    const int numBytes = i1.GetWidth()*i1.GetHeight()*3;
    int maxDiff = 0;
    for ( int n = 0; n < numBytes; n++, p1++, p2++ )
    {
        const int diff = std::abs(*p1 - *p2);
        if ( diff > maxDiff )
            maxDiff = diff;
    }

    return maxDiff;
}

// Note that we accept up to one pixel difference, this happens because of
// different rounding behaviours in different compiler versions
// even under the same architecture, see the example in
// http://thread.gmane.org/gmane.comp.lib.wxwidgets.devel/151149/focus=151154

static void
ASSERT_IMAGE_EQUAL_TO_FILE(const wxImage& image, const wxString& file)
{
    // This environment variable can be set to 1 to save the images instead of
    // checking that the results match the existing files.
    wxString value;
    if ( wxGetEnv("WX_TEST_SAVE_SCALED_IMAGES", &value) && value == "1" )
    {
        INFO("Failed to save \"" << file << "\"");
        CHECK( image.SaveFile(file) );
    }
    else
    {
        const wxImage imageFromFile(file);
        if ( imageFromFile.IsOk() )
        {
            INFO("Wrong \"" << file << "\" " << Catch::StringMaker<wxImage>::convert(image));
            CHECK(FindMaxChannelDiff(imageFromFile, image) <= 1);
        }
        else
        {
            FAIL("Failed to load \"" << file << "\"");
        }
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::ScaleCompare", "[image]")
{
    wxImage original;
    REQUIRE(original.LoadFile("horse.bmp"));

    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale( 50,  50, wxIMAGE_QUALITY_BICUBIC),
                               "image/horse_bicubic_50x50.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(100, 100, wxIMAGE_QUALITY_BICUBIC),
                               "image/horse_bicubic_100x100.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(150, 150, wxIMAGE_QUALITY_BICUBIC),
                               "image/horse_bicubic_150x150.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(300, 300, wxIMAGE_QUALITY_BICUBIC),
                               "image/horse_bicubic_300x300.png");

    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale( 50,  50, wxIMAGE_QUALITY_BOX_AVERAGE),
                               "image/horse_box_average_50x50.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(100, 100, wxIMAGE_QUALITY_BOX_AVERAGE),
                               "image/horse_box_average_100x100.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(150, 150, wxIMAGE_QUALITY_BOX_AVERAGE),
                               "image/horse_box_average_150x150.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(300, 300, wxIMAGE_QUALITY_BOX_AVERAGE),
                               "image/horse_box_average_300x300.png");

    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale( 50,  50, wxIMAGE_QUALITY_BILINEAR),
                               "image/horse_bilinear_50x50.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(100, 100, wxIMAGE_QUALITY_BILINEAR),
                               "image/horse_bilinear_100x100.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(150, 150, wxIMAGE_QUALITY_BILINEAR),
                               "image/horse_bilinear_150x150.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(original.Scale(300, 300, wxIMAGE_QUALITY_BILINEAR),
                               "image/horse_bilinear_300x300.png");

    // Test scaling symmetric image
    const static char* cross_xpm[] =
    {
        "9 9 5 1",
        "   c None",
        "r  c #FF0000",
        "g  c #00FF00",
        "b  c #0000FF",
        "w  c #FFFFFF",
        "    r    ",
        "    g    ",
        "    b    ",
        "    w    ",
        "rgbw wbgr",
        "    w    ",
        "    b    ",
        "    g    ",
        "    r    "
    };

    wxImage imgCross(cross_xpm);
    ASSERT_IMAGE_EQUAL_TO_FILE(imgCross.Scale(256, 256, wxIMAGE_QUALITY_BILINEAR),
                               "image/cross_bilinear_256x256.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(imgCross.Scale(256, 256, wxIMAGE_QUALITY_BICUBIC),
                               "image/cross_bicubic_256x256.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(imgCross.Scale(256, 256, wxIMAGE_QUALITY_BOX_AVERAGE),
                               "image/cross_box_average_256x256.png");
    ASSERT_IMAGE_EQUAL_TO_FILE(imgCross.Scale(256, 256, wxIMAGE_QUALITY_NEAREST),
                               "image/cross_nearest_neighb_256x256.png");
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::CreateBitmapFromCursor", "[image]")
{
#if !defined __WXOSX_IPHONE__ && !defined __WXDFB__ && !defined __WXX11__

    wxImage image("image/wx.png");
    wxCursor cursor(image);
    wxBitmap bitmap(cursor);

#if defined(__WXGTK__)
    // cursor to bitmap could fail depending on windowing system and cursor (gdk-cursor-get-image)
    if ( !bitmap.IsOk() )
        return;
#endif

    wxImage result = bitmap.ConvertToImage();

    // on Windows the cursor is always scaled to 32x32px (96 DPI)
    // on macOS the resulting bitmap size depends on the DPI
    if ( image.GetSize() == result.GetSize() )
    {
        CHECK_THAT(image, RGBASimilarTo(result, 2));
    }
    else
    {
        wxVector<wxPoint> coords;
        coords.push_back(wxPoint(14, 10)); // blue square
        coords.push_back(wxPoint(8, 22));  // red square
        coords.push_back(wxPoint(26, 18)); // yellow square
        coords.push_back(wxPoint(25, 5));  // empty / tranparent

        for ( size_t i = 0; i < coords.size(); ++i )
        {
            wxPoint const& p1 = coords[i];
            wxPoint p2 = wxPoint(p1.x * (result.GetWidth() / (double)image.GetWidth()), p1.y * (result.GetHeight() / (double)image.GetHeight()));

#if defined(__WXMSW__)
            // when the cursor / result image is larger than the source image, the original image is centered in the result image
            if ( result.GetWidth() > image.GetWidth() )
                p2.x = (result.GetWidth() / 2) + (p1.x - (image.GetWidth() / 2));
            if ( result.GetHeight() > image.GetHeight() )
                p2.y = (result.GetHeight() / 2) + (p1.y - (image.GetHeight() / 2));
#endif

            wxColour cSrc(image.GetRed(p1.x, p1.y), image.GetGreen(p1.x, p1.y), image.GetBlue(p1.x, p1.y), image.GetAlpha(p1.x, p1.y));
            wxColour cRes(result.GetRed(p2.x, p2.y), result.GetGreen(p2.x, p2.y), result.GetBlue(p2.x, p2.y), result.GetAlpha(p2.x, p2.y));

            CHECK_EQUAL_COLOUR_RGBA(cRes, cSrc);
        }
    }
#endif
}

// This function assumes that the file is malformed in a way that it cannot
// be loaded but that doesn't fail a wxCHECK.
static void LoadMalformedImage(const wxString& file, wxBitmapType type)
{
    // If the file doesn't exist, it's a test bug.
    // (The product code would fail but for the wrong reason.)
    INFO("Checking that image exists: " << file);
    REQUIRE(wxFileExists(file));

    // Load the image, expecting a failure.
    wxImage image;
    INFO("Loading malformed image: " << file);
    REQUIRE(!image.LoadFile(file, type));
}

// This function assumes that the file is malformed in a way that wxImage
// fails a wxCHECK when trying to load it.
static void LoadMalformedImageWithException(const wxString& file,
                                            wxBitmapType type)
{
    // If the file doesn't exist, it's a test bug.
    // (The product code would fail but for the wrong reason.)
    INFO("Checking that image exists: " << file);
    REQUIRE(wxFileExists(file));

    wxImage image;
    INFO("Loading malformed image: " << file);
#ifdef __WXDEBUG__
    REQUIRE_THROWS(image.LoadFile(file, type));
#else
    REQUIRE(!image.LoadFile(file, type));
#endif
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::BMP", "[image][bmp]")
{
    SECTION("Load malformed bitmaps")
    {
        LoadMalformedImage("image/8bpp-colorsused-negative.bmp",
                           wxBITMAP_TYPE_BMP);
        LoadMalformedImage("image/8bpp-colorsused-large.bmp",
                           wxBITMAP_TYPE_BMP);
        LoadMalformedImage("image/badrle4.bmp", wxBITMAP_TYPE_BMP);
        LoadMalformedImage("image/width-times-height-overflow.bmp", wxBITMAP_TYPE_BMP);
    }
    wxImage image;
    SECTION("32bpp alpha")
    {
        REQUIRE(image.LoadFile("image/32bpp_rgb.bmp", wxBITMAP_TYPE_BMP));
        REQUIRE_FALSE(image.GetAlpha());

        // alpha is preserved for ICO
        REQUIRE(image.LoadFile("image/32bpp_rgb.ico", wxBITMAP_TYPE_ICO));
        const unsigned char* alpha = image.GetAlpha();
        REQUIRE(alpha);
        REQUIRE(alpha[0] == 0x80);

        // alpha is ignored for ICO if it is fully transparent
        REQUIRE(image.LoadFile("image/32bpp_rgb_a0.ico", wxBITMAP_TYPE_ICO));
        REQUIRE_FALSE(image.GetAlpha());

        REQUIRE(image.LoadFile("image/rgb32bf.bmp", wxBITMAP_TYPE_BMP));
        REQUIRE_FALSE(image.GetAlpha());
        REQUIRE(image.LoadFile("image/rgba32.bmp", wxBITMAP_TYPE_BMP));
        alpha = image.GetAlpha();
        REQUIRE(alpha);
        REQUIRE(alpha[0] == 0x80);
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::Paste", "[image][paste]")
{
    const static char* squares_xpm[] =
    {
        "9 9 7 1",
        "   c None",
        "y  c #FFFF00",
        "r  c #FF0000",
        "g  c #00FF00",
        "b  c #0000FF",
        "o  c #FF6600",
        "w  c #FFFFFF",
        "rrrrwgggg",
        "rrrrwgggg",
        "rrrrwgggg",
        "rrrrwgggg",
        "wwwwwwwww",
        "bbbbwoooo",
        "bbbbwoooo",
        "bbbbwoooo",
        "bbbbwoooo"
    };

    const static char* toggle_equal_size_xpm[] =
    {
        "9 9 2 1",
        "   c None",
        "y  c #FFFF00",
        "y y y y y",
        " y y y y ",
        "y y y y y",
        " y y y y ",
        "y y y y y",
        " y y y y ",
        "y y y y y",
        " y y y y ",
        "y y y y y",
    };

    const static char* transparent_image_xpm[] =
    {
        "5 5 2 1",
        "   c None", // Mask
        "y  c #FFFF00",
        "     ",
        "     ",
        "     ",
        "     ",
        "     ",
    };

    const static char* light_image_xpm[] =
    {
        "5 5 2 1",
        "   c None",
        "y  c #FFFF00",
        "yyyyy",
        "yyyyy",
        "yyyyy",
        "yyyyy",
        "yyyyy",
    };

    const static char* black_image_xpm[] =
    {
        "5 5 2 1",
        "   c #000000",
        "y  c None", // Mask
        "     ",
        "     ",
        "     ",
        "     ",
        "     ",
    };

    // Execute AddHandler() just once.
    static bool s_registeredHandler = false;
    if ( !s_registeredHandler )
    {
        wxImage::AddHandler(new wxPNGHandler());
        s_registeredHandler = true;
    }

    SECTION("Paste same size image")
    {
        wxImage actual(squares_xpm);
        wxImage paste(toggle_equal_size_xpm);
        wxImage expected(toggle_equal_size_xpm);
        actual.Paste(paste, 0, 0);
        CHECK_THAT(actual, RGBSameAs(expected));

        // Without alpha using "compose" doesn't change anything.
        actual.Paste(paste, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste larger image")
    {
        const static char* toggle_larger_size_xpm[] =
        {
            "13 13 2 1",
            "   c None",
            "y  c #FFFF00",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
            " y y y y y y ",
            "y y y y y y y",
        };

        wxImage actual(squares_xpm);
        wxImage paste(toggle_larger_size_xpm);
        wxImage expected(toggle_equal_size_xpm);
        actual.Paste(paste, -2, -2);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste smaller image")
    {
        const static char* toggle_smaller_size_xpm[] =
        {
            "5 5 2 1",
            "   c None",
            "y  c #FFFF00",
            "y y y",
            " y y ",
            "y y y",
            " y y ",
            "y y y",
        };

        const static char* expected_xpm[] =
        {
            "9 9 7 1",
            "   c None",
            "y  c #FFFF00",
            "r  c #FF0000",
            "g  c #00FF00",
            "b  c #0000FF",
            "o  c #FF6600",
            "w  c #FFFFFF",
            "rrrrwgggg",
            "rrrrwgggg",
            "rry y ygg",
            "rr y y gg",
            "wwy y yww",
            "bb y y oo",
            "bby y yoo",
            "bbbbwoooo",
            "bbbbwoooo"
        };

        wxImage actual(squares_xpm);
        wxImage paste(toggle_smaller_size_xpm);
        wxImage expected(expected_xpm);
        actual.Paste(paste, 2, 2);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste beyond top left corner")
    {
        const static char* expected_xpm[] =
        {
            "9 9 7 1",
            "   c None",
            "y  c #FFFF00",
            "r  c #FF0000",
            "g  c #00FF00",
            "b  c #0000FF",
            "o  c #FF6600",
            "w  c #FFFFFF",
            "oooowgggg",
            "oooowgggg",
            "oooowgggg",
            "oooowgggg",
            "wwwwwwwww",
            "bbbbwoooo",
            "bbbbwoooo",
            "bbbbwoooo",
            "bbbbwoooo"
        };

        wxImage actual(squares_xpm);
        wxImage paste(squares_xpm);
        wxImage expected(expected_xpm);
        actual.Paste(paste, -5, -5);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste beyond top right corner")
    {
        const static char* expected_xpm[] =
        {
            "9 9 7 1",
            "   c None",
            "y  c #FFFF00",
            "r  c #FF0000",
            "g  c #00FF00",
            "b  c #0000FF",
            "o  c #FF6600",
            "w  c #FFFFFF",
            "rrrrwbbbb",
            "rrrrwbbbb",
            "rrrrwbbbb",
            "rrrrwbbbb",
            "wwwwwwwww",
            "bbbbwoooo",
            "bbbbwoooo",
            "bbbbwoooo",
            "bbbbwoooo"
        };
        wxImage actual(squares_xpm);
        wxImage paste(squares_xpm);
        wxImage expected(expected_xpm);
        actual.Paste(paste, 5, -5);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste beyond bottom right corner")
    {
        const static char* expected_xpm[] =
        {
            "9 9 7 1",
            "   c None",
            "y  c #FF0000",
            "r  c #FF0000",
            "g  c #00FF00",
            "b  c #0000FF",
            "o  c #FF6600",
            "w  c #FFFFFF",
            "rrrrwgggg",
            "rrrrwgggg",
            "rrrrwgggg",
            "rrrrwgggg",
            "wwwwwwwww",
            "bbbbwrrrr",
            "bbbbwrrrr",
            "bbbbwrrrr",
            "bbbbwrrrr"
        };
        wxImage actual(squares_xpm);
        wxImage paste(squares_xpm);
        wxImage expected(expected_xpm);
        actual.Paste(paste, 5, 5);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste beyond bottom left corner")
    {
        const static char* expected_xpm[] =
        {
            "9 9 7 1",
            "   c None",
            "y  c #FFFF00",
            "r  c #FF0000",
            "g  c #00FF00",
            "b  c #0000FF",
            "o  c #FF6600",
            "w  c #FFFFFF",
            "rrrrwgggg",
            "rrrrwgggg",
            "rrrrwgggg",
            "rrrrwgggg",
            "wwwwwwwww",
            "ggggwoooo",
            "ggggwoooo",
            "ggggwoooo",
            "ggggwoooo"
        };
        wxImage actual(squares_xpm);
        wxImage paste(squares_xpm);
        wxImage expected(expected_xpm);
        actual.Paste(paste, -5, 5);
        CHECK_THAT(actual, RGBSameAs(expected));
    }

    SECTION("Paste fully opaque image onto blank image without alpha")
    {
        const wxImage background("image/paste_input_background.png");
        REQUIRE(background.IsOk());

        wxImage actual(background.GetSize());
        actual.Paste(background, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(background));
        CHECK(!actual.HasAlpha());
    }
    SECTION("Paste fully opaque image onto blank image with alpha")
    {
        const wxImage background("image/paste_input_background.png");
        REQUIRE(background.IsOk());

        wxImage actual(background.GetSize());
        actual.InitAlpha();
        actual.Paste(background, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(background));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste fully transparent image")
    {
        const wxImage background("image/paste_input_background.png");
        REQUIRE(background.IsOk());

        wxImage actual = background.Copy();
        wxImage transparent(actual.GetSize());
        transparent.InitAlpha();
        memset(transparent.GetAlpha(), 0, transparent.GetWidth() * transparent.GetHeight());
        actual.Paste(transparent, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(background));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste image with transparent region")
    {
        wxImage actual("image/paste_input_background.png");
        REQUIRE(actual.IsOk());

        const wxImage opaque_square("image/paste_input_overlay_transparent_border_opaque_square.png");
        REQUIRE(opaque_square.IsOk());

        actual.Paste(opaque_square, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(wxImage("image/paste_result_background_plus_overlay_transparent_border_opaque_square.png")));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste image with semi transparent region")
    {
        wxImage actual("image/paste_input_background.png");
        REQUIRE(actual.IsOk());

        const wxImage transparent_square("image/paste_input_overlay_transparent_border_semitransparent_square.png");
        REQUIRE(transparent_square.IsOk());

        actual.Paste(transparent_square, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(wxImage("image/paste_result_background_plus_overlay_transparent_border_semitransparent_square.png")));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste two semi transparent images on top of background")
    {
        wxImage actual("image/paste_input_background.png");
        REQUIRE(actual.IsOk());

        const wxImage transparent_square("image/paste_input_overlay_transparent_border_semitransparent_square.png");
        REQUIRE(transparent_square.IsOk());

        const wxImage transparent_circle("image/paste_input_overlay_transparent_border_semitransparent_circle.png");
        REQUIRE(transparent_circle.IsOk());

        actual.Paste(transparent_circle, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        actual.Paste(transparent_square, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSimilarTo(wxImage("image/paste_result_background_plus_circle_plus_square.png"), 1));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste two semi transparent images together first, then on top of background")
    {
        wxImage actual("image/paste_input_background.png");
        REQUIRE(actual.IsOk());

        const wxImage transparent_square("image/paste_input_overlay_transparent_border_semitransparent_square.png");
        REQUIRE(transparent_square.IsOk());

        const wxImage transparent_circle("image/paste_input_overlay_transparent_border_semitransparent_circle.png");
        REQUIRE(transparent_circle.IsOk());

        wxImage circle = transparent_circle.Copy();
        circle.Paste(transparent_square, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        actual.Paste(circle, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        // When applied in this order, two times a rounding difference is triggered.
        CHECK_THAT(actual, RGBSimilarTo(wxImage("image/paste_result_background_plus_circle_plus_square.png"), 2));
        CHECK_THAT(actual, CenterAlphaPixelEquals(wxALPHA_OPAQUE));
    }
    SECTION("Paste semitransparent image over transparent image")
    {
        const wxImage transparent_square("image/paste_input_overlay_transparent_border_semitransparent_square.png");
        REQUIRE(transparent_square.IsOk());

        const wxImage transparent_circle("image/paste_input_overlay_transparent_border_semitransparent_circle.png");
        REQUIRE(transparent_circle.IsOk());

        wxImage actual(transparent_circle.GetSize());
        actual.InitAlpha();
        memset(actual.GetAlpha(), 0, actual.GetWidth() * actual.GetHeight());
        actual.Paste(transparent_circle, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, CenterAlphaPixelEquals(192));
        actual.Paste(transparent_square, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSimilarTo(wxImage("image/paste_result_no_background_square_over_circle.png"), 1));
        CHECK_THAT(actual, CenterAlphaPixelEquals(224));
    }
    SECTION("Paste fully transparent (masked) image over light image") // todo make test case for 'blend with mask'
    {
        wxImage actual(light_image_xpm);
        actual.InitAlpha();
        wxImage paste(transparent_image_xpm);
        wxImage expected(light_image_xpm);
        actual.Paste(paste, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(expected));
    }
    SECTION("Paste fully black (masked) image over light image") // todo make test case for 'blend with mask'
    {
        wxImage actual(light_image_xpm);
        actual.InitAlpha();
        wxImage paste(black_image_xpm);
        wxImage expected(black_image_xpm);
        actual.Paste(paste, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, RGBSameAs(expected));
    }
    SECTION("Paste dark image over light image")
    {
        wxImage black("image/paste_input_black.png");
        wxImage actual("image/paste_input_background.png");
        actual.InitAlpha();
        actual.Paste(black, 0, 0, wxIMAGE_ALPHA_BLEND_COMPOSE);
        CHECK_THAT(actual, CenterAlphaPixelEquals(255));
        CHECK_THAT(actual, RGBSameAs(black));
    }
    SECTION("Paste large image with negative vertical offset")
    {
        wxImage target(442, 249);
        wxImage to_be_pasted(345, 24900);
        target.InitAlpha();
        target.Paste(to_be_pasted, 48, -12325, wxIMAGE_ALPHA_BLEND_COMPOSE);
    }
    SECTION("Paste large image with negative horizontal offset")
    {
        wxImage target(249, 442);
        wxImage to_be_pasted(24900, 345);
        target.InitAlpha();
        target.Paste(to_be_pasted, -12325, 48, wxIMAGE_ALPHA_BLEND_COMPOSE);
    }

}

TEST_CASE("wxImage::RGBtoHSV", "[image][rgb][hsv]")
{
    SECTION("RGB(0,0,0) (Black) to HSV")
    {
       wxImage::RGBValue rgbBlack(0, 0, 0);
       wxImage::HSVValue hsvBlack = wxImage::RGBtoHSV(rgbBlack);

       CHECK(hsvBlack.value == Approx(0.0));
       // saturation and hue are undefined
    }
    SECTION("RGB(255,255,255) (White) to HSV")
    {
       wxImage::RGBValue rgbWhite(255, 255, 255);
       wxImage::HSVValue hsvWhite = wxImage::RGBtoHSV(rgbWhite);

       CHECK(hsvWhite.saturation == Approx(0.0));
       CHECK(hsvWhite.value == Approx(1.0));
       // hue is undefined
    }
    SECTION("RGB(0,255,0) (Green) to HSV")
    {
       wxImage::RGBValue rgbGreen(0, 255, 0);
       wxImage::HSVValue hsvGreen = wxImage::RGBtoHSV(rgbGreen);

       CHECK(hsvGreen.hue == Approx(1.0/3.0));
       CHECK(hsvGreen.saturation == Approx(1.0));
       CHECK(hsvGreen.value == Approx(1.0));
    }

    SECTION("RGB to HSV to RGB")
    {
        const wxImage::RGBValue rgbValues[] =
        {
            wxImage::RGBValue(   0,   0,   0 ),
            wxImage::RGBValue(  10,  10,  10 ),
            wxImage::RGBValue( 255, 255, 255 ),
            wxImage::RGBValue( 255,   0,   0 ),
            wxImage::RGBValue(   0, 255,   0 ),
            wxImage::RGBValue(   0,   0, 255 ),
            wxImage::RGBValue(   1,   2,   3 ),
            wxImage::RGBValue(  10,  20,  30 ),
            wxImage::RGBValue(   0,   1,   6 ),
            wxImage::RGBValue(   9,   0,  99 ),
        };

        for (unsigned i = 0; i < WXSIZEOF(rgbValues); i++)
        {
            wxImage::RGBValue rgbValue = rgbValues[i];
            wxImage::HSVValue hsvValue = wxImage::RGBtoHSV(rgbValue);
            wxImage::RGBValue rgbRoundtrip = wxImage::HSVtoRGB(hsvValue);

            CHECK(rgbRoundtrip.red == rgbValue.red);
            CHECK(rgbRoundtrip.green == rgbValue.green);
            CHECK(rgbRoundtrip.blue == rgbValue.blue);
        }
    }
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::Clipboard", "[image][clipboard]")
{
#if wxUSE_CLIPBOARD && wxUSE_DATAOBJ
    wxInitAllImageHandlers();

    wxImage imgOriginal;
    REQUIRE(imgOriginal.LoadFile("horse.png") == true);

    wxImageDataObject* dobj1 = new wxImageDataObject(imgOriginal);
    {
        wxClipboardLocker lockClip;
        REQUIRE(wxTheClipboard->SetData(dobj1) == true);
    }

    wxImageDataObject dobj2;
    {
        wxClipboardLocker lockClip;
        REQUIRE(wxTheClipboard->GetData(dobj2) == true);
    }
    wxImage imgRetrieved = dobj2.GetImage();
    REQUIRE(imgRetrieved.IsOk());

    CHECK_THAT(imgOriginal, RGBASameAs(imgRetrieved));
#endif // wxUSE_CLIPBOARD && wxUSE_DATAOBJ
}

TEST_CASE("wxImage::InitAlpha", "[image][initalpha]")
{
    const wxColour maskCol(*wxRED);
    const wxColour fillCol(*wxGREEN);

    SECTION("RGB image without mask")
    {
        wxImage img(2, 2);
        img.SetRGB(0, 0, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(0, 1, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(1, 0, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetRGB(1, 1, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        REQUIRE_FALSE(img.HasAlpha());
        REQUIRE_FALSE(img.HasMask());

        wxImage imgRes = img;
        imgRes.InitAlpha();
        REQUIRE(imgRes.HasAlpha() == true);
        REQUIRE_FALSE(imgRes.HasMask());

        for (int y = 0; y < img.GetHeight(); y++)
            for (int x = 0; x < img.GetWidth(); x++)
            {
                wxColour cSrc(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y));
                wxColour cRes(imgRes.GetRed(x, y), imgRes.GetGreen(x, y), imgRes.GetBlue(x, y), imgRes.GetAlpha(x, y));

                CHECK_EQUAL_COLOUR_RGB(cRes, cSrc);
                CHECK((int)cRes.Alpha() == (int)wxIMAGE_ALPHA_OPAQUE);
            }
    }

    SECTION("RGB image with mask")
    {
        wxImage img(2, 2);
        img.SetRGB(0, 0, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(0, 1, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(1, 0, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetRGB(1, 1, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetMaskColour(maskCol.Red(), maskCol.Green(), maskCol.Blue());
        REQUIRE_FALSE(img.HasAlpha());
        REQUIRE(img.HasMask() == true);

        wxImage imgRes = img;
        imgRes.InitAlpha();
        REQUIRE(imgRes.HasAlpha() == true);
        REQUIRE_FALSE(imgRes.HasMask());

        for ( int y = 0; y < img.GetHeight(); y++ )
            for ( int x = 0; x < img.GetWidth(); x++ )
            {
                wxColour cSrc(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y));
                wxColour cRes(imgRes.GetRed(x, y), imgRes.GetGreen(x, y), imgRes.GetBlue(x, y), imgRes.GetAlpha(x, y));

                CHECK_EQUAL_COLOUR_RGB(cRes, cSrc);
                if ( cSrc == maskCol )
                {
                    CHECK((int)cRes.Alpha() == (int)wxIMAGE_ALPHA_TRANSPARENT);
                }
                else
                {
                    CHECK((int)cRes.Alpha() == (int)wxIMAGE_ALPHA_OPAQUE);
                }
            }
    }

    SECTION("RGBA image without mask")
    {
        wxImage img(2, 2);
        img.SetRGB(0, 0, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(0, 1, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(1, 0, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetRGB(1, 1, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetAlpha();
        img.SetAlpha(0, 0, 128);
        img.SetAlpha(0, 1, 0);
        img.SetAlpha(1, 0, 128);
        img.SetAlpha(1, 1, 0);
        REQUIRE(img.HasAlpha() == true);
        REQUIRE_FALSE(img.HasMask());

        wxImage imgRes = img;
#ifdef __WXDEBUG__
        CHECK_THROWS(imgRes.InitAlpha());
#else
        imgRes.InitAlpha();
#endif
        REQUIRE(imgRes.HasAlpha() == true);
        REQUIRE_FALSE(imgRes.HasMask());

        for ( int y = 0; y < img.GetHeight(); y++ )
            for ( int x = 0; x < img.GetWidth(); x++ )
            {
                wxColour cSrc(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y), img.GetAlpha(x, y));
                wxColour cRes(imgRes.GetRed(x, y), imgRes.GetGreen(x, y), imgRes.GetBlue(x, y), imgRes.GetAlpha(x, y));

                CHECK_EQUAL_COLOUR_RGBA(cRes, cSrc);
            }
    }

    SECTION("RGBA image with mask")
    {
        wxImage img(2, 2);
        img.SetRGB(0, 0, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(0, 1, maskCol.Red(), maskCol.Green(), maskCol.Blue());
        img.SetRGB(1, 0, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetRGB(1, 1, fillCol.Red(), fillCol.Green(), fillCol.Blue());
        img.SetAlpha();
        img.SetAlpha(0, 0, 128);
        img.SetAlpha(0, 1, 0);
        img.SetAlpha(1, 0, 128);
        img.SetAlpha(1, 1, 0);
        img.SetMaskColour(maskCol.Red(), maskCol.Green(), maskCol.Blue());
        REQUIRE(img.HasAlpha() == true);
        REQUIRE(img.HasMask() == true);

        wxImage imgRes = img;
#ifdef __WXDEBUG__
        CHECK_THROWS(imgRes.InitAlpha());
#else
        imgRes.InitAlpha();
#endif
        REQUIRE(imgRes.HasAlpha() == true);
        REQUIRE(imgRes.HasMask() == true);

        for ( int y = 0; y < img.GetHeight(); y++ )
            for ( int x = 0; x < img.GetWidth(); x++ )
            {
                wxColour cSrc(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y), img.GetAlpha(x, y));
                wxColour cRes(imgRes.GetRed(x, y), imgRes.GetGreen(x, y), imgRes.GetBlue(x, y), imgRes.GetAlpha(x, y));

                CHECK_EQUAL_COLOUR_RGBA(cRes, cSrc);
            }
    }
}

TEST_CASE("wxImage::XPM", "[image][xpm]")
{
   static const char * dummy_xpm[] = {
      "16 16 2 1",
      "@ c Black",
      "  c None",
      "@               ",
      " @              ",
      "  @             ",
      "   @            ",
      "    @           ",
      "     @          ",
      "      @         ",
      "       @        ",
      "        @       ",
      "         @      ",
      "          @     ",
      "           @    ",
      "            @   ",
      "             @  ",
      "              @ ",
      "               @"
   };

   wxImage image(dummy_xpm);
   CHECK( image.IsOk() );

   // The goal here is mostly just to check that this code compiles, i.e. that
   // creating all these classes from XPM works.
   CHECK( wxBitmap(dummy_xpm).IsOk() );
   CHECK( wxCursor(dummy_xpm).IsOk() );
   CHECK( wxIcon(dummy_xpm).IsOk() );
}

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::PNM", "[image][pnm]")
{
#if wxUSE_PNM
    wxImage::AddHandler(new wxPNMHandler);

    SECTION("width*height*3 overflow")
    {
        // wxImage should reject the file as malformed (wxTrac#19326)
        LoadMalformedImageWithException("image/width_height_32_bit_overflow.pgm",
                                        wxBITMAP_TYPE_PNM);
    }
#endif
}

namespace
{

inline ImageRGBMatcher RGBSimilarToFile(const char* name)
{
    INFO("Loading file from " << name);

    wxImage expected;
    REQUIRE(expected.LoadFile(name));

    return ImageRGBMatcher(expected, 1);
}

} // anonymous namespace

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::ChangeColours", "[image]")
{
    wxImage original;
    REQUIRE(original.LoadFile("image/toucan.png", wxBITMAP_TYPE_PNG));

    wxImage test;
    wxImage expected;

    test = original;
    test.RotateHue(0.538);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_hue_0.538.png"));

    test = original;
    test.ChangeSaturation(-0.41);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_sat_-0.41.png"));

    test = original;
    test.ChangeBrightness(-0.259);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_bright_-0.259.png"));

    test = original;
    test.ChangeHSV(0.538, -0.41, -0.259);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_hsv_0.538_-0.41_-0.259.png"));

    test = original;
    test = test.ChangeLightness(46);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_light_46.png"));

    test = original;
    test = test.ConvertToDisabled(240);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_dis_240.png"));

    test = original;
    test = test.ConvertToGreyscale();
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_grey.png"));

    test = original;
    test = test.ConvertToMono(255, 255, 255);
    CHECK_THAT(test, RGBSimilarToFile("image/toucan_mono_255_255_255.png"));
}

TEST_CASE("wxImage::Clear", "[image]")
{
    wxImage image(2, 2);
    image.SetRGB(0, 0, 0xff, 0x00, 0x00);
    image.SetRGB(0, 1, 0x00, 0xff, 0x00);
    image.SetRGB(1, 0, 0x00, 0x00, 0xff);
    image.SetRGB(1, 1, 0xff, 0xff, 0xff);

    wxImage image2{image};

    // Check that the image has the expected red component values initially.
    CHECK( image2.GetRed(0, 0) == 0xff );
    CHECK( image2.GetRed(0, 1) == 0x00 );
    CHECK( image2.GetRed(1, 0) == 0x00 );
    CHECK( image2.GetRed(1, 1) == 0xff );

    // Check that the image got cleared.
    image2.Clear();
    CHECK( image2.GetRed(0, 0) == 0x00 );
    CHECK( image2.GetRed(0, 1) == 0x00 );
    CHECK( image2.GetRed(1, 0) == 0x00 );
    CHECK( image2.GetRed(1, 1) == 0x00 );

    // Check that the original image didn't change (see #23553).
    CHECK( image.GetRed(0, 0) == 0xff );
    CHECK( image.GetRed(1, 1) == 0xff );
}

TEST_CASE("wxImage::SizeLimits", "[image]")
{
#if SIZEOF_VOID_P == 8
    // Check that we can resample an image of size greater than 2^16, which is
    // the limit used in 32-bit code to avoid integer overflows.
    wxImage image(100000, 2);
    REQUIRE_NOTHROW( image = image.ResampleNearest(100000, 1) );
    CHECK( image.GetWidth() == 100000 );
    CHECK( image.GetHeight() == 1 );
#endif // SIZEOF_VOID_P == 8
}

// This can be used to test loading an arbitrary image file by setting the
// environment variable WX_TEST_IMAGE_PATH to point to it.
TEST_CASE_METHOD(ImageHandlersInit, "wxImage::LoadPath", "[.]")
{
    wxString path;
    REQUIRE( wxGetEnv("WX_TEST_IMAGE_PATH", &path) );

    TestLogEnabler enableLogs;

    wxImage image;
    REQUIRE( image.LoadFile(path) );

    WARN("Image "
            << image.GetWidth()
            << "*"
            << image.GetHeight()
            << (image.HasAlpha() ? " with alpha" : "")
            << " loaded");
}

#ifdef wxHAS_SVG

#include "wx/bmpbndl.h"
#include "wx/crt.h"

static wxSize ParseEnvVarAsSize(const wxString& varname)
{
    wxString sizeStr;
    REQUIRE( wxGetEnv(varname, &sizeStr) );

    wxString heightStr;
    unsigned width = 0;
    REQUIRE( sizeStr.BeforeFirst(',', &heightStr).ToUInt(&width) );

    unsigned height = 0;
    if ( !heightStr.empty() )
    {
        REQUIRE( heightStr.ToUInt(&height) );
    }
    else
    {
        height = width;
    }

    return wxSize(width, height);
}

// Compute difference between the 2 images by summing up squares of (naively
// computed, i.e. without any perception-based correction) distances between
// colours for each pixel.
static float ComputeImageDiff(const wxImage& img1, const wxImage& img2)
{
    const wxSize size = img1.GetSize();

    REQUIRE( img2.GetSize() == size );

    const auto sqr = [](int x) { return x * x; };

    const auto numPixels = size.x * size.y;
    const auto end = img1.GetData() + numPixels * 3;

    // Should we take alpha into account here?
    float diff = 0;
    for ( auto p1 = img1.GetData(), p2 = img2.GetData();
          p1 != end;
          p1 += 3, p2 += 3 )
    {
        diff += sqrt(sqr(p1[0] - p2[0]) + sqr(p1[1] - p2[1]) + sqr(p1[2] - p2[2]));
    }

    return diff / numPixels;
}

// The purpose of this test is to compute "resize quality" which is defined as
// the difference between resizing an image obtained by rendering an SVG at
// some resolution to the target size and rendering the same SVG directly at
// the target size.
//
// It requires the following environment variables to be set:
//
//  - WX_TEST_SVG_PATH: path to the SVG file to use
//  - WX_TEST_INITIAL_SIZE: size of the first image to render
//  - WX_TEST_TARGET_SIZE: size of the target image
//
// Sizes can be specified as either "x,y" or just "x" as a shorthand for "x,x".
TEST_CASE_METHOD(ImageHandlersInit, "wxImage::ResizeQuality", "[.]")
{
    wxString path;
    REQUIRE( wxGetEnv("WX_TEST_SVG_PATH", &path) );

    const wxSize startSize = ParseEnvVarAsSize("WX_TEST_INITIAL_SIZE");
    const wxSize targetSize = ParseEnvVarAsSize("WX_TEST_TARGET_SIZE");

    const auto bb = wxBitmapBundle::FromSVGFile(path, startSize);
    REQUIRE( bb.IsOk() );

    const wxImage initial = bb.GetBitmap(startSize).ConvertToImage();
    const wxImage rendered = bb.GetBitmap(targetSize).ConvertToImage();

    wxFprintf(stderr, "Error per pixel when resizing %d*%d to %d*%d:\n",
             startSize.x, startSize.y, targetSize.x, targetSize.y);
    wxFprintf(stderr, "  Normal:   %f\n", ComputeImageDiff(rendered,
                initial.Scale(targetSize)));
    wxFprintf(stderr, "  Nearest:  %f\n", ComputeImageDiff(rendered,
                initial.Scale(targetSize, wxIMAGE_QUALITY_NEAREST)));
    wxFprintf(stderr, "  Box avg:  %f\n", ComputeImageDiff(rendered,
                initial.Scale(targetSize, wxIMAGE_QUALITY_BOX_AVERAGE)));
    wxFprintf(stderr, "  Bilinear: %f\n", ComputeImageDiff(rendered,
                initial.Scale(targetSize, wxIMAGE_QUALITY_BILINEAR)));
    wxFprintf(stderr, "  Bicubic:  %f\n", ComputeImageDiff(rendered,
                initial.Scale(targetSize, wxIMAGE_QUALITY_BICUBIC)));
}

#endif // wxHAS_SVG

TEST_CASE_METHOD(ImageHandlersInit, "wxImage::Cursor", "[image][cursor]")
{
    // cursor from file
    wxCursor cursor1("horse.cur", wxBITMAP_TYPE_CUR);
    CHECK( cursor1.IsOk() );
    wxCursor cursor2("horse.ico", wxBITMAP_TYPE_ICO);
    CHECK( cursor2.IsOk() );
    wxCursor cursor3("horse.bmp", wxBITMAP_TYPE_BMP);
    CHECK( cursor3.IsOk() );

    // converted horse.cur to XBM, 32x32px, hotspot 16,23
    static const unsigned char xbm_horse[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00,
        0x00, 0x1F, 0x00, 0x00, 0xC0, 0x1F, 0x00, 0x00, 0xF8, 0x3F, 0x00, 0x00,
        0xFF, 0x3F, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x01, 0x00, 0xFF, 0xFF, 0x01, 0x00, 0xFF, 0xFF, 0x01, 0x00,
        0xFF, 0xFF, 0x03, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0xFF, 0xFF, 0x07, 0x00,
        0xFF, 0xFF, 0x07, 0x00, 0xFF, 0xFF, 0x07, 0x00, 0xFF, 0xFF, 0x07, 0x00,
        0xFF, 0xFF, 0x0F, 0x00, 0xFF, 0xFF, 0x0F, 0x00, 0xBF, 0xFF, 0x1F, 0x00,
        0x1F, 0xFE, 0x1F, 0x00, 0x0F, 0x7C, 0x1E, 0x00, 0x0F, 0x78, 0x3E, 0x00,
        0x07, 0xF0, 0x3F, 0x00, 0x03, 0xE0, 0x3F, 0x00, 0x01, 0xC0, 0x1F, 0x00,
        0x00, 0x80, 0x0F, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    wxBitmap bmp((const char*)xbm_horse, 32, 32);
    REQUIRE( bmp.IsOk() );
    wxCursor cursorFromBmp(bmp, 16, 23);
    CHECK( cursorFromBmp.IsOk() );

    wxImage img = bmp.ConvertToImage();
    REQUIRE( img.IsOk() );
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 16);
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 23);
    wxCursor cursor4(img);
    CHECK( cursor4.IsOk() );

#if defined(__WXGTK__)
    // GTK has a constructor accepting XBM
    wxCursor cursor5((const char*)xbm_horse, 32, 32, 16, 23, nullptr, wxWHITE, wxBLACK);
    CHECK( cursor5.IsOk() );
#endif

    // cursor from resource (win32 .rc file, macOS app resources)
#if defined( __WXMSW__ ) || defined( __WXOSX__ )
    wxCursor cursor6("horse");
    CHECK( cursor6.IsOk() );
#endif
#if defined( __WXOSX__ )
    wxCursor cursor7("bullseye");
    CHECK( cursor7.IsOk() );
#endif
}

/*
    TODO: add lots of more tests to wxImage functions
*/

#endif //wxUSE_IMAGE
