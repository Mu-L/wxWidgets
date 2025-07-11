/////////////////////////////////////////////////////////////////////////////
// Name:        src/qt/graphics.cpp
// Purpose:     Graphics context methods for the Qt platform
// Author:      Jay Nabonne
// Created:     2018-12-13
// Copyright:   (c) Jay Nabonne
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"


#if wxUSE_GRAPHICS_CONTEXT

#include <QApplication>
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QPicture>
#include <QWidget>

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/icon.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/dcprint.h"
    #include "wx/window.h"
#endif

#include "wx/graphics.h"
#include "wx/rawbmp.h"
#include "wx/tokenzr.h"

#include "wx/private/graphics.h"
#include "wx/qt/private/converter.h"

#include <memory>

class WXDLLIMPEXP_CORE wxQtBrushData : public wxGraphicsObjectRefData
{
public:
    wxQtBrushData(wxGraphicsRenderer* renderer)
        : wxGraphicsObjectRefData(renderer)
    {
    }

    wxQtBrushData(wxGraphicsRenderer* renderer, const wxBrush& wxbrush)
        : wxGraphicsObjectRefData(renderer),
          m_brush(wxbrush.GetHandle())
    {
    }

    void CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
                                   wxDouble x2, wxDouble y2,
                                   const wxGraphicsGradientStops& stops)
    {
        QLinearGradient gradient(x1, y1, x2, y2);
        SetStops(gradient, stops);
        m_brush = QBrush(gradient);
    }

    void CreateRadialGradientBrush(wxDouble startX, wxDouble startY,
                                   wxDouble endX, wxDouble endY,
                                   wxDouble radius,
                                   const wxGraphicsGradientStops& stops)
    {
        QRadialGradient gradient(QPointF(endX, endY), radius, QPointF(startX, startY));
        SetStops(gradient, stops);
        m_brush = QBrush(gradient);
    }

    const QBrush& getBrush() const
    {
        return m_brush;
    }

private:
    static void
    SetStops(QGradient& gradient, const wxGraphicsGradientStops& stops)
    {
        QGradientStops qstops;
        for ( size_t i = 0; i < stops.GetCount(); ++i )
        {
            const wxGraphicsGradientStop stop = stops.Item(i);
            qstops.append(QGradientStop(double(stop.GetPosition()),
                                        stop.GetColour().GetQColor()));
        }

        gradient.setStops(qstops);
    }

    QBrush m_brush;

    wxDECLARE_NO_COPY_CLASS(wxQtBrushData);
};

class WXDLLIMPEXP_CORE wxQtPenData : public wxGraphicsObjectRefData
{
public:
    wxQtPenData(wxGraphicsRenderer* renderer, const wxGraphicsPenInfo& info)
        : wxGraphicsObjectRefData(renderer),
          m_pen(CreatePenFromInfo(info))
    {
        m_pen.setWidthF(info.GetWidth());
    }

    const QPen& GetPen() const
    {
        return m_pen;
    }

private:
    static QPen CreatePenFromInfo(const wxGraphicsPenInfo& info)
    {
        wxPen wxpen(info.GetColour(), 1, info.GetStyle());
        wxpen.SetDashes(info.GetDashCount(), info.GetDash());
        wxpen.SetJoin(info.GetJoin());
        wxpen.SetCap(info.GetCap());
        switch ( info.GetStyle() )
        {
            case wxPENSTYLE_STIPPLE:
            case wxPENSTYLE_STIPPLE_MASK:
            case wxPENSTYLE_STIPPLE_MASK_OPAQUE:
            {
                wxpen.SetStipple(info.GetStipple());
                break;
            }
            default:
                break;
        }

        return wxpen.GetHandle();
    }
    QPen m_pen;

    wxDECLARE_NO_COPY_CLASS(wxQtPenData);
};

class WXDLLIMPEXP_CORE wxQtBitmapData : public wxGraphicsBitmapData
{
public:
    wxQtBitmapData(wxGraphicsRenderer* renderer, QPixmap pixmap)
        : wxGraphicsBitmapData(renderer),
          m_pixmap(pixmap)
    {
    }

    wxQtBitmapData(wxGraphicsRenderer* renderer, const wxBitmap& bmp)
        : wxGraphicsBitmapData(renderer)
    {
        wxCHECK_RET( bmp.IsOk(), "Invalid bitmap");

        const bool isSrcBpp32 = bmp.GetDepth() == 32;
        const bool hasAlpha = bmp.HasAlpha();

        QImage::Format imgFormat =
            (isSrcBpp32 && hasAlpha) || bmp.GetMask() != nullptr
            ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;

        auto qtImage = QImage(bmp.GetWidth(), bmp.GetHeight(), imgFormat);

        const int stride = qtImage.bytesPerLine();

        wxBitmap bmpSource = bmp;  // we need a non-const instance
        wxUint32* data = reinterpret_cast<wxUint32*>(qtImage.bits());

        if ( isSrcBpp32 )
        {
            // use the bitmap's alpha
            wxAlphaPixelData pixData(bmpSource);
            wxCHECK_RET( pixData, "Failed to gain raw access to bitmap data." );

            wxAlphaPixelData::Iterator p(pixData);
            for ( int y = 0; y < pixData.GetHeight(); ++y )
            {
                wxAlphaPixelData::Iterator rowStart = p;
                wxUint32* const rowStartDst = data;
                for ( int x = 0; x < pixData.GetWidth(); ++x )
                {
                    unsigned char alpha = hasAlpha ? p.Alpha() : wxALPHA_OPAQUE;
                    // Bitmap pixel bits are already premultiplied.
                    *data = (alpha << 24 | p.Red() << 16 | p.Green() << 8 | p.Blue());
                    ++data;
                    ++p;
                }

                data = rowStartDst + stride / 4;
                p = rowStart;
                p.OffsetY(pixData, 1);
            }
        }
        else // no alpha
        {
            wxNativePixelData pixData(bmpSource);
            wxCHECK_RET( pixData, "Failed to gain raw access to bitmap data." );

            wxNativePixelData::Iterator p(pixData);
            for ( int y = 0; y < pixData.GetHeight(); ++y )
            {
                wxNativePixelData::Iterator rowStart = p;
                wxUint32* const rowStartDst = data;
                for ( int x = 0; x < pixData.GetWidth(); ++x )
                {
                    *data = (wxALPHA_OPAQUE << 24 | p.Red() << 16 | p.Green() << 8 | p.Blue() );
                    ++data;
                    ++p;
                }

                data = rowStartDst + stride / 4;
                p = rowStart;
                p.OffsetY(pixData, 1);
            }
        }

        // if there is a mask, set the alpha bytes in the target buffer to
        // fully transparent or retain original value
        if ( bmp.GetMask() )
        {
            wxBitmap bmpMask = bmp.GetMask()->GetBitmap();
            data = reinterpret_cast<wxUint32*>(qtImage.bits());
            wxNativePixelData pixData(bmpMask);
            wxCHECK_RET( pixData, "Failed to gain raw access to mask data." );

            wxNativePixelData::Iterator p(pixData);
            for ( int y = 0; y < pixData.GetHeight(); ++y )
            {
                wxNativePixelData::Iterator rowStart = p;
                wxUint32* const rowStartDst = data;
                for ( int x = 0; x < pixData.GetWidth(); ++x )
                {
                    if ( p.Red()+p.Green()+p.Blue() == 0 )
                        *data = 0;

                    ++data;
                    ++p;
                }

                data = rowStartDst + stride / 4;
                p = rowStart;
                p.OffsetY(pixData, 1);
            }
        }

        m_pixmap = QPixmap::fromImage(qtImage);
    }

    QPixmap GetPixmap() const
    {
        return m_pixmap;
    }

    virtual void* GetNativeBitmap() const override
    {
        return const_cast<QPixmap*>(&m_pixmap);
    }

    static const QPixmap GetPixmapFromBitmap(const wxGraphicsBitmap& bitmap)
    {
        const wxQtBitmapData* const
            data = static_cast<const wxQtBitmapData*>(bitmap.GetBitmapData());
        return data->GetPixmap();
    }


#if wxUSE_IMAGE
    wxImage ConvertToImage() const
    {
        wxBitmap bitmap(m_pixmap);
        return bitmap.ConvertToImage();
    }
#endif // wxUSE_IMAGE

private:
    QPixmap m_pixmap;

    wxDECLARE_NO_COPY_CLASS(wxQtBitmapData);
};


class WXDLLIMPEXP_CORE wxQtMatrixData : public wxGraphicsMatrixData
{
public:
    explicit wxQtMatrixData(wxGraphicsRenderer* renderer)
        : wxGraphicsMatrixData(renderer),
          m_transform(new QTransform)
    {
    }

    wxQtMatrixData(wxGraphicsRenderer* renderer, const QTransform& transform)
        : wxGraphicsMatrixData(renderer),
          m_transform(new QTransform(transform))
    {
    }

    virtual ~wxQtMatrixData()
    {
        delete m_transform;
    }

    virtual wxGraphicsObjectRefData* Clone() const override
    {
        wxQtMatrixData* newMatrix = new wxQtMatrixData(m_renderer);
        *newMatrix->m_transform = *m_transform;
        return newMatrix;
    }

    // concatenates the matrix
    virtual void Concat(const wxGraphicsMatrixData *t) override
    {
        const wxQtMatrixData* rhs = static_cast<const wxQtMatrixData*>(t);
        *m_transform = *rhs->m_transform* (*m_transform);
    }

    // sets the matrix to the respective values
    virtual void Set(wxDouble a = 1.0, wxDouble b = 0.0,
                     wxDouble c = 0.0, wxDouble d = 1.0,
                     wxDouble tx = 0.0, wxDouble ty = 0.0) override
    {
        m_transform->setMatrix(a, b, 0.0,
                               c, d, 0.0,
                               tx, ty, 1.0);
    }

    // gets the component values of the matrix
    virtual void Get(wxDouble* a, wxDouble* b,
                     wxDouble* c, wxDouble* d,
                     wxDouble* tx, wxDouble* ty) const override
    {
        if ( a )
            *a = m_transform->m11();
        if ( b )
            *b = m_transform->m12();
        if ( c )
            *c = m_transform->m21();
        if ( d )
            *d = m_transform->m22();
        if ( tx )
            *tx = m_transform->m31();
        if ( ty )
            *ty = m_transform->m32();
    }

    // makes this the inverse matrix
    virtual void Invert() override
    {
        bool invertible = false;
        const QTransform invTransform = m_transform->inverted(&invertible);
        if ( invertible )
        {
            *m_transform = invTransform;
        }
    }

    // returns true if the elements of the transformation matrix are equal ?
    virtual bool IsEqual(const wxGraphicsMatrixData* t) const override
    {
        const wxQtMatrixData* rhs = static_cast<const wxQtMatrixData*>(t);
        return *m_transform == *rhs->m_transform;
    }

    // return true if this is the identity matrix
    virtual bool IsIdentity() const override
    {
        return m_transform->isIdentity();
    }

    //
    // transformation
    //

    // add the translation to this matrix
    virtual void Translate(wxDouble dx, wxDouble dy) override
    {
        m_transform->translate(dx, dy);
    }

    // add the scale to this matrix
    virtual void Scale(wxDouble xScale, wxDouble yScale) override
    {
        m_transform->scale(xScale, yScale);
    }

    // add the rotation to this matrix (radians)
    virtual void Rotate(wxDouble angle) override
    {
        m_transform->rotateRadians(angle);
    }

    //
    // apply the transforms
    //

    // applies that matrix to the point
    virtual void TransformPoint(wxDouble *x, wxDouble *y) const override
    {
        qreal transformed_x, transformed_y;
        m_transform->map(static_cast<qreal>(*x), static_cast<qreal>(*y),
                         &transformed_x, &transformed_y);
        *x = transformed_x;
        *y = transformed_y;
    }

    // applies the matrix except for translations
    virtual void TransformDistance(wxDouble *dx, wxDouble *dy) const override
    {
        const QTransform untransTransform(
            m_transform->m11(),
            m_transform->m12(),
            m_transform->m13(),
            m_transform->m21(),
            m_transform->m22(),
            m_transform->m23(),
            0.0,
            0.0,
            1.0);

        qreal transformed_x, transformed_y;
        untransTransform.map(static_cast<qreal>(*dx), static_cast<qreal>(*dy),
                             &transformed_x, &transformed_y);

        *dx = transformed_x;
        *dy = transformed_y;
    }

    // returns the native representation
    virtual void* GetNativeMatrix() const override
    {
        return static_cast<void*>(m_transform);
    }

    const QTransform& GetQTransform() const
    {
        return *m_transform;
    }
private:
    QTransform* m_transform;

    wxDECLARE_NO_COPY_CLASS(wxQtMatrixData);
};

class wxQtFontData : public wxGraphicsObjectRefData
{
public:
    wxQtFontData(wxGraphicsRenderer* renderer,
                 const wxFont& font,
                 const wxColour& col)
        : wxGraphicsObjectRefData(renderer),
          m_font(font.GetHandle()),
          m_color(col.GetQColor())
    {
    }

    wxQtFontData(
        wxGraphicsRenderer* renderer,
        double sizeInPixels,
        const wxString& facename,
        int flags,
        const wxColour& col)
        : wxGraphicsObjectRefData(renderer),
          m_color(col.GetQColor())
    {
        m_font.setFamily(wxQtConvertString(facename));
        m_font.setPixelSize(static_cast<int>(sizeInPixels));
        if ( flags & wxFONTFLAG_LIGHT )
            m_font.setWeight(QFont::Light);
        if ( flags & wxFONTFLAG_BOLD )
            m_font.setWeight(QFont::Bold);
        if ( flags & (wxFONTFLAG_ITALIC | wxFONTFLAG_SLANT) )
            m_font.setItalic(true);
        if ( flags & wxFONTFLAG_ANTIALIASED )
            m_font.setStyleStrategy(QFont::PreferAntialias);
        if ( flags & wxFONTFLAG_NOT_ANTIALIASED )
            m_font.setStyleStrategy(QFont::NoAntialias);
        if ( flags & wxFONTFLAG_UNDERLINED )
            m_font.setUnderline(true);
        if ( flags & wxFONTFLAG_STRIKETHROUGH )
            m_font.setStrikeOut(true);
    }

    const QFont& GetFont() const
    {
        return m_font;
    }

    const QColor& GetColor() const
    {
        return m_color;
    }

private:
    QFont m_font;
    QColor m_color;

    wxDECLARE_NO_COPY_CLASS(wxQtFontData);
};

//-----------------------------------------------------------------------------
// wxQtGraphicsPathData
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxQtGraphicsPathData : public wxGraphicsPathData
{
public:
    wxQtGraphicsPathData(wxGraphicsRenderer* renderer)
        : wxGraphicsPathData(renderer),
          m_path(new QPainterPath()),
          m_current_subpath_start(-1)
    {
    }

    ~wxQtGraphicsPathData()
    {
        delete m_path;
    }

    virtual wxGraphicsObjectRefData *Clone() const override
    {
        return new wxQtGraphicsPathData(*this);
    }

    //
    // These are the path primitives from which everything else can be
    // constructed.
    //

    // begins a new subpath at (x,y)
    virtual void MoveToPoint(wxDouble x, wxDouble y) override
    {
        m_path->moveTo(x, y);
        m_current_subpath_start = m_path->elementCount() - 1;
    }

    // adds a straight line from the current point to (x,y)
    // if there is no current path, it is equivalent to a moveTo.
    virtual void AddLineToPoint(wxDouble x, wxDouble y) override
    {
        if ( !HasCurrentSubpath() )
            MoveToPoint(x, y);
        else
            m_path->lineTo(x, y);
    }

    // adds a cubic Bezier curve from the current point, using two control
    // points and an end point
    virtual void
    AddCurveToPoint(wxDouble cx1, wxDouble cy1,
                    wxDouble cx2, wxDouble cy2,
                    wxDouble x, wxDouble y) override
    {
        if ( !HasCurrentSubpath() )
            MoveToPoint(cx1, cy1);
        m_path->cubicTo(QPointF(cx1, cy1), QPointF(cx2, cy2), QPointF(x, y));
    }

    // adds an arc of a circle centering at (x,y) with radius (r) from
    // startAngle to endAngle
    virtual void
    AddArc(wxDouble x, wxDouble y, wxDouble r,
           wxDouble startAngle, wxDouble endAngle,
           bool clockwise) override
    {
        const bool fixupFirst = !HasCurrentSubpath();
        if ( fixupFirst )
            MoveToPoint(x, y);

        qreal arcLength;
        if ( clockwise )
        {
            if ( endAngle < startAngle )
                endAngle += 2* M_PI;
            arcLength = -wxRadToDeg(endAngle - startAngle);
        }
        else
        {
            if ( endAngle > startAngle )
                endAngle -= 2* M_PI;
            arcLength = -wxRadToDeg(endAngle - startAngle);
        }
        m_path->arcTo(x-r, y-r, r*2, r*2, -wxRadToDeg(startAngle), arcLength);
        if ( fixupFirst )
        {
            QPainterPath::Element
                element = m_path->elementAt(m_current_subpath_start+1);
            m_path->setElementPositionAt(m_current_subpath_start,
                                         element.x, element.y);
        }
    }

    // gets the last point of the current path, (0,0) if not yet set
    virtual void GetCurrentPoint(wxDouble* x, wxDouble* y) const override
    {
        QPointF position = m_path->currentPosition();
        *x = position.x();
        *y = position.y();
    }

    // adds another path
    virtual void AddPath(const wxGraphicsPathData* path) override
    {
        const wxQtGraphicsPathData*
            pathAdded = static_cast<const wxQtGraphicsPathData*>(path);
        m_path->addPath(*pathAdded->m_path);
    }

    // closes the current sub-path
    virtual void CloseSubpath() override
    {
        // Current position must be end of last path after close, not (0,0) as
        // Qt sets.
        if ( !HasCurrentSubpath() )
            return;

        const QPainterPath::Element
            firstElement = m_path->elementAt(m_current_subpath_start);

        m_path->closeSubpath();
        MoveToPoint(firstElement.x, firstElement.y);
    }

    //
    // These are convenience functions which - if not available natively will
    // be assembled using the primitives from above
    //

    // appends a circle as a new closed subpath
    virtual void AddCircle(wxDouble x, wxDouble y, wxDouble r) override
    {
        m_path->addEllipse(x - r, y - r, r*2, r*2);
    }

    // appends an ellipse as a new closed subpath fitting the passed rectangle
    virtual void
    AddEllipse(wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        m_path->addEllipse(x, y, w, h);
    }

    // returns the native path
    virtual void* GetNativePath() const override
    {
        return static_cast<void*>(m_path);
    }

    // give the native path returned by GetNativePath() back (there might be
    // some deallocations necessary)
    virtual void UnGetNativePath(void *) const override
    {
    }

    // transforms each point of this path by the matrix
    virtual void Transform(const wxGraphicsMatrixData* matrix) override
    {
        const wxQtMatrixData*
            qmatrix = static_cast<const wxQtMatrixData*>(matrix);
        *m_path = qmatrix->GetQTransform().map(*m_path);
    }

    // gets the bounding box enclosing all points (possibly including control
    // points)
    virtual void
    GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const override
    {
        QRectF boundingRect = m_path->controlPointRect();

        if ( !boundingRect.isValid() )
            boundingRect = QRectF();

        if ( x ) *x = boundingRect.left();
        if ( y ) *y = boundingRect.top();
        if ( w ) *w = boundingRect.width();
        if ( h ) *h = boundingRect.height();
    }

    virtual bool
    Contains(wxDouble x, wxDouble y,
             wxPolygonFillMode /*fillStyle = wxWINDING_RULE*/) const override
    {
        return m_path->contains(QPointF(x, y));
    }

private:
    // for Clone
    wxQtGraphicsPathData(const wxQtGraphicsPathData& rhs)
        : wxGraphicsPathData(rhs.GetRenderer()),
          m_path(new QPainterPath(*rhs.m_path)),
          m_current_subpath_start(rhs.m_current_subpath_start)
    {
    }

    bool HasCurrentSubpath() const
    {
        return m_current_subpath_start != -1;
    }

    QPainterPath* m_path;
    int m_current_subpath_start;

    wxQtGraphicsPathData& operator=(const wxQtGraphicsPathData&);
};

class WXDLLIMPEXP_CORE wxQtGraphicsContext : public wxGraphicsContext
{
    void InitFromDC(const wxDC& dc)
    {
        m_qtPainter = static_cast<QPainter*>(dc.GetHandle());

        const wxSize sz = dc.GetSize();
        m_width = sz.x;
        m_height = sz.y;

        m_internalTransform = m_qtPainter->worldTransform();

        SetInitialClipping();
    }

    // Set the initial clipping to the entire surface when the context is
    // first constructed or when the clipping is reset.
    void SetInitialClipping()
    {
        m_qtPainter->setWorldMatrixEnabled(false);
        m_qtPainter->setClipRect(QRectF(0, 0, m_width, m_height));
        m_qtPainter->setWorldMatrixEnabled(true);
    }

    class OffsetHelper
    {
    public:
        OffsetHelper(bool shouldOffset, QPainter* qpainter)
        {
            m_qtPainter = qpainter;
            m_shouldOffset = shouldOffset;
            if ( !m_shouldOffset )
                return;

            m_qtPainter->translate(0.5, 0.5);
        }

        ~OffsetHelper()
        {
            if ( m_shouldOffset )
                m_qtPainter->translate(-0.5, -0.5);
        }

    private:
        QPainter* m_qtPainter;
        bool m_shouldOffset;
    };

protected:
    // Use the specified painter and take ownership of it, i.e. it will be
    // destroyed in this class dtor.
    void AttachPainter(QPainter* painter)
    {
        m_qtPainter = painter;

        // Ensure that it will be destroyed when this object is.
        m_ownedPainter.reset(m_qtPainter);
    }

    wxQtGraphicsContext(wxGraphicsRenderer* renderer)
        : wxGraphicsContext(renderer),
          m_qtPainter(nullptr)
    {
    }

public:
    wxQtGraphicsContext(wxGraphicsRenderer* renderer, QPaintDevice* device)
        : wxGraphicsContext(renderer)
    {
        AttachPainter(new QPainter(device));

        m_width = device->width();
        m_height = device->height();
    }

    wxQtGraphicsContext(wxGraphicsRenderer* renderer, const wxWindowDC& dc)
        : wxGraphicsContext(renderer)
    {
        InitFromDC(dc);
    }

    wxQtGraphicsContext(wxGraphicsRenderer* renderer, const wxMemoryDC& dc)
        : wxGraphicsContext(renderer)
    {
        InitFromDC(dc);
    }

#if wxUSE_PRINTING_ARCHITECTURE
    wxQtGraphicsContext(wxGraphicsRenderer* renderer, const wxPrinterDC& dc)
        : wxGraphicsContext(renderer)
    {
        InitFromDC(dc);
    }

#endif

    wxQtGraphicsContext(wxGraphicsRenderer* renderer, wxWindow *window)
        : wxGraphicsContext(renderer)
    {
        m_qtPainter = static_cast<QPainter*>(window->QtGetPainter());

        const wxSize sz = window->GetClientSize();
        m_width = sz.x;
        m_height = sz.y;
    }

    virtual bool ShouldOffset() const override
    {
        if ( m_pen.IsNull() )
            return false;

        const auto& qpen = static_cast<wxQtPenData*>(m_pen.GetRefData())->GetPen();

        if ( qpen.style() == Qt::NoPen )
            return false;

        const double width = qpen.widthF();

        // always offset for 1-pixel width
        if ( width <= 0 )
            return true;

        // offset if pen width is odd integer
        const int w = int(width);
        return (w & 1) && wxIsSameDouble(width, w);
    }

    virtual void Clip(const wxRegion& region) override
    {
        // Notice that setClipRegion() doesn't enable clipping for us.
        // So we need to enable it explicitly.
        m_qtPainter->setClipping(true);
        m_qtPainter->setClipRegion(region.GetHandle(), Qt::IntersectClip);
    }

    // clips drawings to the rect
    virtual void Clip(wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        m_qtPainter->setClipRect(QRectF(x, y, w, h), Qt::IntersectClip);
    }

    // resets the clipping to original extent
    virtual void ResetClip() override
    {
        SetInitialClipping();

        m_qtPainter->setClipping(false);
    }

    // returns bounding box of the clipping region
    virtual void
    GetClipBox(wxDouble* x, wxDouble* y, wxDouble* w, wxDouble* h) override
    {
        QRectF box = m_qtPainter->clipBoundingRect();

        if ( box.isEmpty() )
            box = QRectF();

        if ( x )
            *x = box.left();
        if ( y )
            *y = box.top();
        if ( w )
            *w = box.width();
        if ( h )
            *h = box.height();
    }

    virtual void* GetNativeContext() override
    {
        return static_cast<void*>(m_qtPainter);
    }

    virtual bool SetAntialiasMode(wxAntialiasMode antialias) override
    {
        bool enableAA = false;
        switch ( antialias )
        {
            case wxANTIALIAS_DEFAULT:
                enableAA = true;
                break;

            case wxANTIALIAS_NONE:
                enableAA = false;
                break;
        }

        m_qtPainter->setRenderHints(QPainter::Antialiasing |
                                    QPainter::TextAntialiasing,
                                    enableAA);

        m_antialias = antialias;
        return true;
    }

    virtual bool
    SetInterpolationQuality(wxInterpolationQuality interpolation) override
    {
        m_qtPainter->setRenderHint(QPainter::SmoothPixmapTransform,
                                   interpolation == wxINTERPOLATION_BEST);
        m_interpolation = interpolation;
        return true;
    }

    virtual bool SetCompositionMode(wxCompositionMode composition) override
    {
        QPainter::CompositionMode q_composition_mode;
        switch ( composition )
        {
            case wxCOMPOSITION_CLEAR:
                q_composition_mode = QPainter::CompositionMode_Clear;
                break;
            case wxCOMPOSITION_SOURCE:
                q_composition_mode = QPainter::CompositionMode_Source;
                break;
            case wxCOMPOSITION_OVER:
                q_composition_mode = QPainter::CompositionMode_SourceOver;
                break;
            case wxCOMPOSITION_IN:
                q_composition_mode = QPainter::CompositionMode_SourceIn;
                break;
            case wxCOMPOSITION_OUT:
                q_composition_mode = QPainter::CompositionMode_SourceOut;
                break;
            case wxCOMPOSITION_ATOP:
                q_composition_mode = QPainter::CompositionMode_SourceAtop;
                break;
            case wxCOMPOSITION_DEST:
                q_composition_mode = QPainter::CompositionMode_Destination;
                break;
            case wxCOMPOSITION_DEST_OVER:
                q_composition_mode = QPainter::CompositionMode_DestinationOver;
                break;
            case wxCOMPOSITION_DEST_IN:
                q_composition_mode = QPainter::CompositionMode_DestinationIn;
                break;
            case wxCOMPOSITION_DEST_OUT:
                q_composition_mode = QPainter::CompositionMode_DestinationOut;
                break;
            case wxCOMPOSITION_DEST_ATOP:
                q_composition_mode = QPainter::CompositionMode_DestinationAtop;
                break;
            case wxCOMPOSITION_XOR:
                q_composition_mode = QPainter::CompositionMode_Xor;
                break;
            case wxCOMPOSITION_ADD:
                q_composition_mode = QPainter::CompositionMode_Plus;
                break;
            default:
                return false;
        }
        m_qtPainter->setCompositionMode(q_composition_mode);
        m_composition = composition;
        return true;
    }

    virtual void BeginLayer(wxDouble /*opacity*/) override
    {
        wxFAIL_MSG("BeginLayer not implemented");
    }

    virtual void EndLayer() override
    {
        wxFAIL_MSG("EndLayer not implemented");
    }

    virtual void StrokePath(const wxGraphicsPath& p) override
    {
        if ( m_pen.IsNull() )
        {
            return;
        }

        OffsetHelper helper(ShouldOffset(), m_qtPainter);

        const QPainterPath*
            pathData = static_cast<QPainterPath*>(p.GetNativePath());
        const QPen&
            pen = static_cast<wxQtPenData*>(m_pen.GetRefData())->GetPen();
        m_qtPainter->strokePath(*pathData, pen);
    }

    virtual void
    FillPath(const wxGraphicsPath& p,
             wxPolygonFillMode fillStyle = wxWINDING_RULE) override
    {
        if ( m_brush.IsNull() )
        {
            return;
        }

        OffsetHelper helper(ShouldOffset(), m_qtPainter);

        QPainterPath* pathData = static_cast<QPainterPath*>(p.GetNativePath());
        const Qt::FillRule fillRule = fillStyle == wxWINDING_RULE ? Qt::WindingFill : Qt::OddEvenFill;
        pathData->setFillRule(fillRule);

        const QBrush&
            brush = static_cast<wxQtBrushData*>(m_brush.GetRefData())->getBrush();
        m_qtPainter->fillPath(*pathData, brush);
    }

    virtual void
    ClearRectangle(wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        m_qtPainter->fillRect(QRectF(x, y, w, h), QBrush(QColor(0, 0, 0, 0)));
    }

    virtual void Translate(wxDouble dx, wxDouble dy) override
    {
        m_qtPainter->translate(dx, dy);
    }

    virtual void Scale(wxDouble xScale, wxDouble yScale) override
    {
        m_qtPainter->scale(xScale, yScale);
    }

    virtual void Rotate(wxDouble angle) override
    {
        // wx angle is in radians. Qt angle is in degrees.
        m_qtPainter->rotate(wxRadToDeg(angle));
    }

    // concatenates this transform with the current transform of this context
    virtual void ConcatTransform(const wxGraphicsMatrix& matrix) override
    {
        if ( !m_qtPainter->isActive() )
            return;

        const wxQtMatrixData*
            qmatrix = static_cast<const wxQtMatrixData*>(matrix.GetRefData());
        m_qtPainter->setTransform(qmatrix->GetQTransform(), true);
    }

    // sets the transform of this context
    virtual void SetTransform(const wxGraphicsMatrix& matrix) override
    {
        if ( !m_qtPainter->isActive() )
            return;

        const wxQtMatrixData*
            qmatrix = static_cast<const wxQtMatrixData*>(matrix.GetRefData());
        m_qtPainter->setTransform(m_internalTransform);
        m_qtPainter->setTransform(qmatrix->GetQTransform(), true);
    }

    // gets the matrix of this context
    virtual wxGraphicsMatrix GetTransform() const override
    {
        if ( !m_qtPainter->isActive() )
            return CreateMatrix(); // return identity matrix

        QTransform transform = m_qtPainter->transform();

        if ( m_internalTransform.isInvertible() )
        {
            const auto intTransformInv = m_internalTransform.inverted();
            transform *= intTransformInv;
        }

        wxGraphicsMatrix m;
        m.SetRefData(new wxQtMatrixData(GetRenderer(), transform));
        return m;
    }

    virtual void
    DrawBitmap(const wxGraphicsBitmap& bmp,
               wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        const auto pixmap = wxQtBitmapData::GetPixmapFromBitmap(bmp);
        m_qtPainter->drawPixmap(QRectF(x, y, w, h), pixmap, QRectF());
    }

    virtual void
    DrawBitmap(const wxBitmap& bmp,
               wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        DoDrawBitmap(bmp, x, y, w, h, true);
    }

    virtual void
    DrawIcon(const wxIcon& icon,
             wxDouble x, wxDouble y, wxDouble w, wxDouble h) override
    {
        DoDrawBitmap(icon, x, y, w, h, true);
    }

    virtual void PushState() override
    {
        m_qtPainter->save();
    }

    virtual void PopState() override
    {
        m_qtPainter->restore();
    }

    virtual void Flush() override
    {
    }

    virtual void GetTextExtent(const wxString& str,
                               wxDouble *width,
                               wxDouble *height,
                               wxDouble *descent,
                               wxDouble *externalLeading) const override
    {
        wxCHECK_RET( !m_font.IsNull(),
                     "wxQtContext::GetTextExtent - no valid font set" );

        if ( width )
            *width = 0;
        if ( height )
            *height = 0;
        if ( descent )
            *descent = 0;
        if ( externalLeading )
            *externalLeading = 0;

        // We can skip computing the string width and height if it is empty, but
        // not its descent and/or external leading, which still needs to be
        // returned even for an empty string.
        if ( str.empty() && !descent && !externalLeading )
            return;

        const wxQtFontData*
            fontData = static_cast<wxQtFontData*>(m_font.GetRefData());
        QFontMetrics metrics(fontData->GetFont());

        if ( m_qtPainter->isActive() )
        {
            m_qtPainter->setFont(fontData->GetFont());
            metrics = m_qtPainter->fontMetrics();
        }

        const QRect boundingRect = metrics.boundingRect(wxQtConvertString(str));

        if ( width )
            *width = boundingRect.width();
        if ( height )
            *height = boundingRect.height();
        if ( descent )
            *descent = metrics.descent();
        if ( externalLeading )
            *externalLeading = metrics.leading() - (metrics.ascent() + metrics.descent());
    }

    virtual void
    GetPartialTextExtents(const wxString& text,
                          wxArrayDouble& widths) const override
    {
        wxCHECK_RET( !m_font.IsNull(),
                     "wxQtContext::GetPartialTextExtents - no valid font set" );

        const wxQtFontData*
            fontData = static_cast<wxQtFontData*>(m_font.GetRefData());
        QFontMetrics metrics(fontData->GetFont());

        if ( m_qtPainter->isActive() )
        {
            m_qtPainter->setFont(fontData->GetFont());
            metrics = m_qtPainter->fontMetrics();
        }

        const size_t textLength = text.length();

        widths.clear();
        widths.insert(widths.end(), textLength, 0);

        for ( size_t i = 0; i < textLength; ++i )
        {
            const wxString subString = text.substr(0, i+1);
            const QRect
                boundingRect = metrics.boundingRect(wxQtConvertString(subString));
            widths[i] = boundingRect.width();
        }
    }

protected:
    virtual void
    DoDrawText(const wxString& str, wxDouble x, wxDouble y) override
    {
        wxCHECK_RET( !m_font.IsNull(),
                     "wxQtContext::DrawText - no valid font set" );

        const wxQtFontData*
            fontData = static_cast<wxQtFontData*>(m_font.GetRefData());

        m_qtPainter->setFont(fontData->GetFont());
        m_qtPainter->setPen(QPen(fontData->GetColor()));

        QFontMetrics metrics = m_qtPainter->fontMetrics();

        wxStringTokenizer tokenizer(str, "\n");
        while ( tokenizer.HasMoreTokens() )
        {
            const wxString line = tokenizer.GetNextToken();
            m_qtPainter->drawText(QPointF(x, y + metrics.ascent()), wxQtConvertString(line));
            y += metrics.lineSpacing();
        }
    }

    void
    DoDrawBitmap(const wxBitmap& bmp,
                 wxDouble x, wxDouble y,
                 wxDouble w, wxDouble h, bool useMask) const
    {
        QPixmap pix = *bmp.GetHandle();

        if (useMask && bmp.GetMask() && bmp.GetMask()->GetHandle())
            pix.setMask(*bmp.GetMask()->GetHandle());

        m_qtPainter->drawPixmap(QRectF(x, y, w, h), pix, QRectF());
    }

    QPainter*  m_qtPainter;
    QTransform m_internalTransform;

private:
    // This pointer may be empty if we don't own m_qtPainter.
    std::unique_ptr<QPainter> m_ownedPainter;

    wxDECLARE_NO_COPY_CLASS(wxQtGraphicsContext);
};

class wxQtMeasuringContext : public wxQtGraphicsContext
{
public:
    wxQtMeasuringContext(wxGraphicsRenderer* renderer)
        : wxQtGraphicsContext(renderer)
    {
        AttachPainter(new QPainter());
    }
};

class wxQtImageContext : public wxQtGraphicsContext
{
public:
    wxQtImageContext(wxGraphicsRenderer* renderer, wxImage& image) :
        wxQtGraphicsContext(renderer),
        m_image(image),
        m_bitmap(GetRenderer(), wxBitmap(image))
    {
        auto pixmap = static_cast<QPixmap*>(m_bitmap.GetNativeBitmap());
        AttachPainter(new QPainter(pixmap));
    }

    ~wxQtImageContext()
    {
        // Finish the painting on the pixmap so we can access it below.
        m_qtPainter->end();

        m_image = m_bitmap.ConvertToImage();
    }

private:
    wxImage& m_image;
    wxQtBitmapData m_bitmap;

    wxDECLARE_NO_COPY_CLASS(wxQtImageContext);
};

//-----------------------------------------------------------------------------
// wxQtGraphicsRenderer declaration
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxQtGraphicsRenderer : public wxGraphicsRenderer
{
public:
    wxQtGraphicsRenderer() {}

    virtual ~wxQtGraphicsRenderer() {}

    // Context

    virtual wxGraphicsContext* CreateContext(const wxWindowDC& dc) override;
    virtual wxGraphicsContext* CreateContext(const wxMemoryDC& dc) override;
#if wxUSE_PRINTING_ARCHITECTURE
    virtual wxGraphicsContext* CreateContext(const wxPrinterDC& dc) override;
#endif

    virtual wxGraphicsContext* CreateContextFromNativeContext(void* context) override;

    virtual wxGraphicsContext* CreateContextFromNativeWindow(void* window) override;

#if wxUSE_IMAGE
    virtual wxGraphicsContext* CreateContextFromImage(wxImage& image) override;
#endif // wxUSE_IMAGE

    virtual wxGraphicsContext* CreateContext(wxWindow* window) override;

    virtual wxGraphicsContext* CreateMeasuringContext() override;

    // Path

    virtual wxGraphicsPath CreatePath() override;

    // Matrix

    virtual wxGraphicsMatrix CreateMatrix(wxDouble a = 1.0, wxDouble b = 0.0,
                                          wxDouble c = 0.0, wxDouble d = 1.0,
                                          wxDouble tx = 0.0, wxDouble ty = 0.0) override;

    virtual wxGraphicsPen CreatePen(const wxGraphicsPenInfo& info) override;

    virtual wxGraphicsBrush CreateBrush(const wxBrush& brush) override;

    virtual wxGraphicsBrush
        CreateLinearGradientBrush(wxDouble x1, wxDouble y1,
        wxDouble x2, wxDouble y2,
        const wxGraphicsGradientStops& stops,
        const wxGraphicsMatrix& matrix = wxNullGraphicsMatrix) override;

    virtual wxGraphicsBrush
        CreateRadialGradientBrush(wxDouble startX, wxDouble startY,
        wxDouble endX, wxDouble endY,
        wxDouble radius,
        const wxGraphicsGradientStops& stops,
        const wxGraphicsMatrix& matrix = wxNullGraphicsMatrix) override;

    // sets the font
    virtual wxGraphicsFont CreateFont(const wxFont& font,
                                      const wxColour& col = *wxBLACK) override;
    virtual wxGraphicsFont CreateFont(double sizeInPixels,
        const wxString& facename,
        int flags = wxFONTFLAG_DEFAULT,
        const wxColour& col = *wxBLACK) override;
    virtual wxGraphicsFont CreateFontAtDPI(const wxFont& font,
                                           const wxRealPoint& dpi,
                                           const wxColour& col) override;

    // create a native bitmap representation
    virtual wxGraphicsBitmap CreateBitmap(const wxBitmap& bitmap) override;
#if wxUSE_IMAGE
    virtual wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) override;
    virtual wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp) override;
#endif // wxUSE_IMAGE

    // create a graphics bitmap from a native bitmap
    virtual wxGraphicsBitmap CreateBitmapFromNativeBitmap(void* bitmap) override;

    // create a subimage from a native image representation
    virtual wxGraphicsBitmap CreateSubBitmap(const wxGraphicsBitmap& bitmap,
                                             wxDouble x, wxDouble y,
                                             wxDouble w, wxDouble h) override;

    virtual wxString GetName() const override;
    virtual void GetVersion(int *major,
                            int *minor,
                            int *micro) const override;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxQtGraphicsRenderer);
};

//-----------------------------------------------------------------------------
// wxQtGraphicsRenderer implementation
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxQtGraphicsRenderer, wxGraphicsRenderer);

wxGraphicsContext* wxQtGraphicsRenderer::CreateContext(const wxWindowDC& dc)
{
    return new wxQtGraphicsContext(this, dc);
}

wxGraphicsContext* wxQtGraphicsRenderer::CreateContext(const wxMemoryDC& dc)
{
    return new wxQtGraphicsContext(this, dc);
}

#if wxUSE_PRINTING_ARCHITECTURE
wxGraphicsContext* wxQtGraphicsRenderer::CreateContext(const wxPrinterDC& dc)
{
    return new wxQtGraphicsContext(this, dc);
}
#endif

wxGraphicsContext*
wxQtGraphicsRenderer::CreateContextFromNativeContext(void* context)
{
    return new wxQtGraphicsContext(this, static_cast<QPaintDevice*>(context));
}

wxGraphicsContext*
wxQtGraphicsRenderer::CreateContextFromNativeWindow(void* window)
{
    return new wxQtGraphicsContext(this, static_cast<QWidget*>(window));
}

#if wxUSE_IMAGE
wxGraphicsContext* wxQtGraphicsRenderer::CreateContextFromImage(wxImage& image)
{
    return new wxQtImageContext(this, image);
}
#endif // wxUSE_IMAGE

wxGraphicsContext* wxQtGraphicsRenderer::CreateMeasuringContext()
{
    return new wxQtMeasuringContext(this);
}

wxGraphicsContext* wxQtGraphicsRenderer::CreateContext(wxWindow* window)
{
    return new wxQtGraphicsContext(this, window);
}

// Path

wxGraphicsPath wxQtGraphicsRenderer::CreatePath()
{
    wxGraphicsPath path;
    path.SetRefData(new wxQtGraphicsPathData(this));
    return path;
}

// Matrix

wxGraphicsMatrix wxQtGraphicsRenderer::CreateMatrix(wxDouble a, wxDouble b,
                                                    wxDouble c, wxDouble d,
                                                    wxDouble tx, wxDouble ty)

{
    wxGraphicsMatrix m;
    wxQtMatrixData* data = new wxQtMatrixData(this);
    data->Set(a, b, c, d, tx, ty);
    m.SetRefData(data);
    return m;
}

wxGraphicsPen wxQtGraphicsRenderer::CreatePen(const wxGraphicsPenInfo& info)
{
    wxGraphicsPen p;
    if ( info.GetStyle() != wxPENSTYLE_TRANSPARENT )
    {
        p.SetRefData(new wxQtPenData(this, info));
    }
    return p;
}

wxGraphicsBrush wxQtGraphicsRenderer::CreateBrush(const wxBrush& brush)
{
    wxGraphicsBrush p;
    if ( brush.IsOk() && brush.GetStyle() != wxBRUSHSTYLE_TRANSPARENT )
    {
        p.SetRefData(new wxQtBrushData(this, brush));
    }
    return p;
}

wxGraphicsBrush wxQtGraphicsRenderer::CreateLinearGradientBrush(
    wxDouble x1, wxDouble y1,
    wxDouble x2, wxDouble y2,
    const wxGraphicsGradientStops& stops,
    const wxGraphicsMatrix& WXUNUSED(matrix))
{
    wxGraphicsBrush p;
    wxQtBrushData* d = new wxQtBrushData(this);
    d->CreateLinearGradientBrush(x1, y1, x2, y2, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsBrush wxQtGraphicsRenderer::CreateRadialGradientBrush(
    wxDouble startX, wxDouble startY,
    wxDouble endX, wxDouble endY, wxDouble r,
    const wxGraphicsGradientStops& stops,
    const wxGraphicsMatrix& WXUNUSED(matrix))
{
    wxGraphicsBrush p;
    wxQtBrushData* d = new wxQtBrushData(this);
    d->CreateRadialGradientBrush(startX, startY, endX, endY, r, stops);
    p.SetRefData(d);
    return p;
}

wxGraphicsFont
wxQtGraphicsRenderer::CreateFont(const wxFont& font, const wxColour& col)
{
    wxGraphicsFont p;
    if ( font.IsOk() )
    {
        p.SetRefData(new wxQtFontData(this, font, col));
    }
    return p;
}

wxGraphicsFont wxQtGraphicsRenderer::CreateFont(
    double sizeInPixels,
    const wxString& facename,
    int flags,
    const wxColour& col)
{
    wxGraphicsFont font;
    font.SetRefData(new wxQtFontData(this, sizeInPixels, facename, flags, col));
    return font;
}

wxGraphicsFont
wxQtGraphicsRenderer::CreateFontAtDPI(const wxFont& font,
                                      const wxRealPoint& WXUNUSED(dpi),
                                      const wxColour& col)
{
    return CreateFont(font, col);
}

wxGraphicsBitmap wxQtGraphicsRenderer::CreateBitmap(const wxBitmap& bmp)
{
    wxGraphicsBitmap p;
    if ( bmp.IsOk() )
    {
        p.SetRefData(new wxQtBitmapData(this, bmp));
    }
    return p;
}

#if wxUSE_IMAGE

wxGraphicsBitmap
wxQtGraphicsRenderer::CreateBitmapFromImage(const wxImage& image)
{
    wxGraphicsBitmap bmp;
    if ( image.IsOk() )
    {
        bmp.SetRefData(new wxQtBitmapData(this, image));
    }
    return bmp;
}

wxImage wxQtGraphicsRenderer::CreateImageFromBitmap(const wxGraphicsBitmap& bmp)
{
    const wxQtBitmapData* const
        data = static_cast<const wxQtBitmapData*>(bmp.GetBitmapData());
    return data->ConvertToImage();
}
#endif // wxUSE_IMAGE

wxGraphicsBitmap
wxQtGraphicsRenderer::CreateBitmapFromNativeBitmap(void* bitmap)
{
    wxGraphicsBitmap p;
    if ( bitmap != nullptr )
    {
        p.SetRefData(new wxQtBitmapData(this, *(QPixmap*)bitmap));
    }
    return p;
}

wxGraphicsBitmap
wxQtGraphicsRenderer::CreateSubBitmap(const wxGraphicsBitmap& bitmap,
    wxDouble x, wxDouble y,
    wxDouble w, wxDouble h)
{
    wxCHECK_MSG(!bitmap.IsNull(), wxNullGraphicsBitmap, wxS("Invalid bitmap"));

    const auto sourcePixmap = wxQtBitmapData::GetPixmapFromBitmap(bitmap);
    wxCHECK_MSG(!sourcePixmap.isNull(), wxNullGraphicsBitmap, wxS("Invalid bitmap"));

    const int srcWidth = sourcePixmap.width();
    const int srcHeight = sourcePixmap.height();
    const int dstWidth = wxRound(w);
    const int dstHeight = wxRound(h);
    const int dstX = wxRound(x);
    const int dstY = wxRound(y);

    wxCHECK_MSG(dstX >= 0 && dstY >= 0 && dstWidth > 0 && dstHeight > 0 &&
            dstX + dstWidth <= srcWidth && dstY + dstHeight <= srcHeight,
            wxNullGraphicsBitmap, wxS("Invalid bitmap region"));

    QPixmap subPixmap = sourcePixmap.copy(dstX, dstY, dstWidth, dstHeight);

    wxGraphicsBitmap bmpRes;
    bmpRes.SetRefData(new wxQtBitmapData(this, subPixmap));
    return bmpRes;
}

wxString wxQtGraphicsRenderer::GetName() const
{
    return "qt";
}

void wxQtGraphicsRenderer::GetVersion(int *major, int *minor, int *micro) const
{
    if ( major ) *major = QT_VERSION_MAJOR;
    if ( minor ) *minor = QT_VERSION_MINOR;
    if ( micro ) *micro = QT_VERSION_PATCH;
}

static wxQtGraphicsRenderer gs_qtGraphicsRenderer;

wxGraphicsRenderer* wxGraphicsRenderer::GetDefaultRenderer()
{
    return &gs_qtGraphicsRenderer;
}

#endif // wxUSE_GRAPHICS_CONTEXT
