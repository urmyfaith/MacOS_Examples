/*
    File:       CSkWindow.c
        
    Contains:	Implementation of CSkWindow creation and event handling.
				
    Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                ("Apple") in consideration of your agreement to the following terms, and your
                use, installation, modification or redistribution of this Apple software
                constitutes acceptance of these terms.  If you do not agree with these terms,
                please do not use, install, modify or redistribute this Apple software.

                In consideration of your agreement to abide by the following terms, and subject
                to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
                copyrights in this original Apple software (the "Apple Software"), to use,
                reproduce, modify and redistribute the Apple Software, with or without
                modifications, in source and/or binary forms; provided that if you redistribute
                the Apple Software in its entirety and without modifications, you must retain
                this notice and the following text and disclaimers in all such redistributions of
                the Apple Software.  Neither the name, trademarks, service marks or logos of
                Apple Computer, Inc. may be used to endorse or promote products derived from the
                Apple Software without specific prior written permission from Apple.  Except as
                expressly stated in this notice, no other rights or licenses, express or implied,
                are granted by Apple herein, including but not limited to any patent rights that
                may be infringed by your derivative works or by other works in which the Apple
                Software may be incorporated.

                The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
                WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
                WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
                PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
                COMBINATION WITH YOUR PRODUCTS.

                IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
                CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
                GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
                ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
                OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
                (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
                ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Copyright � 2004 Apple Computer, Inc., All Rights Reserved
*/


#include "CSkWindow.h"
#include "CSkDocStorage.h"
#include "CSkConstants.h"
#include "CSkUtils.h"
#include "CSkObjects.h"
#include "CSkToolPalette.h"
#include "CSkPrinting.h"
#include "NavServicesHandling.h"

#define RECTWIDTH(r)	    ((r).right - (r).left)
#define RECTHEIGHT(r)       ((r).bottom - (r).top)


/* ------------------- Coordinate Juggling ---------------------

Let's say we want to present the drawing area (a "document page") on some background
(as in AppleWorks, Word etc.). The window has size wWidth x wHeight, and scrolls around on 
the background, provided 2 x hMargin + docWidth is bigger than wWidth, or wHeight is bigger
than 2 * vMargin + docHeight. If it scrolls around, scrollPosition (relative to the 
background origin, which is at (-kTopMargin, -kLeftMargin) relativ to the document topLeft)
describes the topLeft of the window. The horizontal scrolling range is then 
2 x hMargin + docWidth - wWidth, the vertical range is 2 x vMargin + docHeight - wHeight.
However, we also want the support a scaling factor for the document content. So, all dimensions
above except wWidth and wHeight need to be scaled by the current scaling factor!

Our drawing objects are described in documentpage-relativ coordinates, with the origin of
the CGContext at the bottom-left of the page. 
To draw an object into the windowContext (which has the origin reset via SyncCGContextOriginWithPort, 
included in QDBeginCGContext), we need to do the following:
- CGContextTranslateCTM(scaledCGDocOrigin)  // bottom-left of scaled document, relativ to bottom-left of window
- CGContextScaleCTM(curScaling)
where scaledCGDocOrigin is computed as follows:
  x = docTopLeft.h * scale - scrollPosition.h
  y = (docTopLeft.v + docSize) * scale - (scrollPosition.v + wHeight)

To convert mouse-tracking (hit-testing) window-topleft-relative QD coordinates into cgDocument coordinates, 
we need to
- add scrollPosition (now the QD coordinates are relative to our global scroll area)
- subtract the (scaled) docOrigin point (still in QD coordintes, now docOrigin-relative). 
- scale by 1/(scaling factor)
- convert to cg coordinates: (v, h) -> (h, docHeight - v), where docHeight is unscaled.

-------------------------------------------------------------- */


//----------------------------------------------------------------------------------------------------------------
static void SetupControls(WindowRef window, DocStorage* docStP)
{
    ControlID   cntlID  = { kControlSignatureMainWindow, kScrollVerticalID };
    ControlRef  control;
    SInt16		cntlMax;
	
    GetControlByID(window, &cntlID, &control);
    cntlMax = docStP->scale * (docStP->docSize.v + 2 * docStP->docTopLeft.v) - docStP->windowRect.bottom;
    if (cntlMax < 0)
        cntlMax = 0;
    SetControlMaximum(control, cntlMax);
	
    cntlID.id = kScrollHorizontalID;
    GetControlByID(window, &cntlID, &control);
    cntlMax = docStP->scale * (docStP->docSize.h + 2 * docStP->docTopLeft.h) - docStP->windowRect.right;
    if (cntlMax < 0)	// is this built into SetControlMaximum when 0 = cntlMin ?
        cntlMax = 0;
    SetControlMaximum(control, cntlMax);
	
	cntlID.id = kScalePopupID;
    GetControlByID(window, &cntlID, &control);
	SetControlMinimum(control, 0);
	SetControlMaximum(control, kMaxScaleMenuItem);
}

//----------------------------------------------------------------------------------------------------------------
// Given a QDPoint in local window coordinates, return a CGPoint in document (drawObject) coordinates.
static CGPoint QDCoordsToCSkDocCGCoords(Point inPt, DocStorage* docStP)
{
    float   scale           = docStP->scale;
    Point   docTopLeft      = docStP->docTopLeft;
    float   x, y;
    
    AddPt(docStP->scrollPosition, &inPt);          // now on global scaled document background
    x = (float)inPt.h / scale;
    y = (float)inPt.v / scale;                  // now unscaled
    x -= docTopLeft.h;
    y -= docTopLeft.v;                          // document-topLeft relative
    return CGPointMake(x, docStP->docSize.v - y);  // document-bottomLeft relative
}

//----------------------------------------------------------------------------------------------------------------
// If either the window size, or the scroll position, or the scale changes, we need to recompute the displayCTM.
// The docTopLeft would only change in the case of a multi-page document.
static void RecalculateDisplayCTM(DocStorage* docStP)
{
    int     wHeight = docStP->windowRect.bottom - docStP->windowRect.top;
    CGPoint scaledCGDocOrigin;  // relative to the origin of the window context
    
    scaledCGDocOrigin.x = docStP->docTopLeft.h * docStP->scale - docStP->scrollPosition.h;
    scaledCGDocOrigin.y = (docStP->scrollPosition.v + wHeight) - (docStP->docTopLeft.v + docStP->docSize.v) * docStP->scale;
    
    // displayCTM does a translation by scaledCGDocOrigin and a scaling
    docStP->displayCTM = CGAffineTransformMake(docStP->scale, 0, 0, docStP->scale, scaledCGDocOrigin.x, scaledCGDocOrigin.y);
}

//-----------------------------------------------------------------------------------------------------------------------
static CGContextRef CreateSinglePixelBitmapContext()
{
    int             width       = 1;
    int             height      = 1;
    int             size        = 4 * width * height;
    void*           data        = calloc(size, 1);
    CGColorSpaceRef genericColorSpace  = GetGenericRGBColorSpace();
    CGContextRef    bmCtx       = CGBitmapContextCreate(data, width, height, 8, 4 * width, genericColorSpace, kCGImageAlphaPremultipliedFirst);

    // ensure that drawing takes place in the correct color space, a calibrated color space
    CGContextSetFillColorSpace(bmCtx, genericColorSpace); 
    CGContextSetStrokeColorSpace(bmCtx, genericColorSpace); 

    return bmCtx;
}

//--------------------------------------------------------------------------------------
static DocStorage* CreateDocumentStorage(WindowRef window, WindowRef toolPalette)
{
    // default document position in window
    const int kLeftMargin   = 18;
    const int kTopMargin    = 18;
    const int kDocWidth     = 576;
    const int kDocHeight    = 720;
    const int kGridWidth    = 18;
    const Point nullPt      = { 0, 0 };
    
    DocStorage*  docStP = (DocStorage*) NewPtrClear( sizeof(DocStorage) );
    
    GetWindowPortBounds(window, &docStP->windowRect);
    docStP->ownerWindow         = window;
    docStP->bmCtx               = CreateSinglePixelBitmapContext();
    docStP->docTopLeft.v        = kTopMargin;
    docStP->docTopLeft.h        = kLeftMargin;
    docStP->docSize.v           = kDocHeight;
    docStP->docSize.h           = kDocWidth;
    docStP->gridWidth           = kGridWidth;
    docStP->scale               = 1.0;
    docStP->scrollPosition      = nullPt;
    docStP->toolPalette         = toolPalette;
    docStP->dupOffset           = CGPointMake(9.0, 9.0);
    docStP->pageFormat          = kPMNoPageFormat;
    docStP->printSettings       = kPMNoPrintSettings;
    RecalculateDisplayCTM(docStP);
    return docStP;
}

//--------------------------------------------------------------------------------------
void ReleaseDocumentStorage(DocStorage* docStP)
{
    ReleaseDrawObjList(&docStP->objList);
    
    if (docStP->bmCtx != NULL)
        CGContextRelease(docStP->bmCtx);
     // Don't dispose of toolPalette!
	 if (docStP->pdfData != NULL)
		CFRelease(docStP->pdfData);
    if (docStP->pageFormat != kPMNoPageFormat)
        (void)PMRelease(docStP->pageFormat);
    if (docStP->printSettings != kPMNoPrintSettings)
        (void)PMRelease(docStP->printSettings);
    if (docStP->flattenedPageFormat != NULL)
        DisposeHandle(docStP->flattenedPageFormat);
}

//--------------------------------------------------------------------------------------
static void SetupTransform(CGContextRef ctx, CGRect srcRect, CGRect dstRect)
{
	float scaleX = dstRect.size.width / srcRect.size.width;
	float scaleY = dstRect.size.height / srcRect.size.height;
	float scale = (scaleX < scaleY ? scaleX : scaleY);

	// Do nothing if srcRect fits entirely into dstRect
	if (scale < 1.0)
	{
		CGContextScaleCTM(ctx, scale, scale);   // assume the origin is (0, 0) in srcRect and dstRect, for now
	}
	// Always put the drawing into the destination's topLeft
	CGContextTranslateCTM(ctx, dstRect.size.width - scale * srcRect.size.width, 
							dstRect.size.height - scale * srcRect.size.height);
}


//--------------------------------------------------------------------------------------
static void DrawPDFData(CGContextRef ctx, CFDataRef pdfData, CGRect dstRect)
{
	CGDataProviderRef	provider;
	CGPDFDocumentRef	document;
	CGPDFPageRef		page;
	CGRect				pageSize;
	
	provider = CGDataProviderCreateWithData(NULL, CFDataGetBytePtr(pdfData), CFDataGetLength(pdfData), NULL);
	document = CGPDFDocumentCreateWithProvider(provider);
	page = CGPDFDocumentGetPage(document, 1);				// always page 1 only
	pageSize = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);

    CGContextSaveGState(ctx);
	SetupTransform(ctx, pageSize, dstRect);					// Scale pdf page into dstRect, if the pdf is too big
	CGContextDrawPDFPage(ctx, page);
	CGContextRestoreGState(ctx);

	CFRelease(document);
	CFRelease(provider);
}

//--------------------------------------------------------------------------------------
static void DrawWindowContent(WindowRef window)
{
    DocStorage*		docStP      		= (DocStorage*)GetWRefCon( window );
    CGrafPtr		port        		= GetWindowPort(window);
    const CGrgba 	white 				= { 1.0, 1.0, 1.0, 1.0 };
    const CGrgba 	gridColor 			= { 0.8, 0.9, 0.8, 1.0 };
    const CGrgba 	docBackgroundColor 	= { 0.5, 0.5, 0.5, 1.0 };
    CGColorSpaceRef genericColorSpace 	= GetGenericRGBColorSpace();
    Rect			r;
    CGRect			cgWinRect;
    CGContextRef	ctx;
    ControlID		cntlID      = { kControlSignatureMainWindow, kScrollHorizontalID };
    RgnHandle		contentRgn  = NewRgn();
    RgnHandle		tempRgn     = NewRgn();
    ControlRef		control;
    float			t;
    
    cgWinRect = CGRectMake(0, 0, docStP->windowRect.right - docStP->windowRect.left, docStP->windowRect.bottom - docStP->windowRect.top);
    
    QDBeginCGContext(port, &ctx);
    
    // ensure that we are drawing in the correct color space
    CGContextSetFillColorSpace(ctx, genericColorSpace); 
    CGContextSetStrokeColorSpace(ctx, genericColorSpace); 

    GetWindowRegion(window, kWindowContentRgn, contentRgn);
    QDGlobalToLocalRegion(port, contentRgn);

    // subtract area of controls
    GetControlByID(window, &cntlID, &control);
    RectRgn(tempRgn, GetControlBounds(control, &r));
    DiffRgn(contentRgn, tempRgn, contentRgn);
    cntlID.id = kScrollVerticalID;
    GetControlByID(window, &cntlID, &control);
    RectRgn(tempRgn, GetControlBounds(control, &r));
    DiffRgn(contentRgn, tempRgn, contentRgn);
    cntlID.id = kScalePopupID;
    GetControlByID(window, &cntlID, &control);
    RectRgn(tempRgn, GetControlBounds(control, &r));
    DiffRgn(contentRgn, tempRgn, contentRgn);
    
    ClipCGContextToRegion(ctx, &docStP->windowRect, contentRgn);
	
    CGContextSetFillColor(ctx, (float *)&docBackgroundColor);
    CGContextFillRect(ctx, cgWinRect);                  // paint background around document

    CGContextConcatCTM(ctx, docStP->displayCTM);        // assuming the ctx's CTM is the identity

    // also clip to document page, now
    CGContextClipToRect(ctx, CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v));

    // From now on, all drawing is to be done in "document page coordinates"
    
    CGContextSetFillColor(ctx, (float *)&white);
    CGContextFillRect(ctx, CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v ));  // paint white background

	// If we have a background pdf, draw it
	if (docStP->pdfData != NULL)
	{
		DrawPDFData(ctx, docStP->pdfData, CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v));
	}
	else    // Draw document background grid
	{
		CGContextSetStrokeColor(ctx, (float *)&gridColor);
		CGContextSetLineWidth(ctx, 0.5);
		
		t = 0.5;
		while (t < docStP->docSize.h)
		{
			CGContextMoveToPoint(ctx, t, 0.5);
			CGContextAddLineToPoint(ctx, t, docStP->docSize.v);
			t += docStP->gridWidth;
		}
		t = 0.5;
		while (t < docStP->docSize.v)
		{
			CGContextMoveToPoint(ctx, 0.5, t);
			CGContextAddLineToPoint(ctx, docStP->docSize.h, t);
			t += docStP->gridWidth;
		}
		CGContextStrokePath(ctx);
    }
	
    RenderDrawObjList(ctx, &docStP->objList, true);
	
    CGContextSynchronize(ctx);

    QDEndCGContext(port, &ctx);

    DisposeRgn(contentRgn);
}   // DrawWindowContent


//--------------------------------------------------------------------------------------
static Boolean BigEnoughForDragSelection(Point a, Point b)
{
    return (    ((b.v - a.v > 2) || (a.v - b.v > 2))
             && ((b.h - a.h > 2) || (a.h - b.h > 2)) );
}

//--------------------------------------------------------------------------------------
static void AdjustEndPoint(Point* ioPt, Point beginPt, int selectedShape, UInt32 modifiers)
{
    Boolean shiftKeyDown = ((modifiers & shiftKey) != 0);

    if ( shiftKeyDown )
    {
        int dh = ioPt->h - beginPt.h;
        int dv = ioPt->v - beginPt.v;
		int sign = (dh * dv >= 0 ? 1 : -1);
	
        if (selectedShape == kLineShape)  // allow horizontal, vertical and diagonal only
        {
            int absDH = (dh > 0 ? dh : -dh);
            int absDV = (dv > 0 ? dv : -dv);
            if (absDH < absDV/4)
            {
                ioPt->h = beginPt.h;
            }
            else if (absDV < absDH/4)
            {
                ioPt->v = beginPt.v;
            }
            else    // diagonal
            {
                ioPt->v = beginPt.v + sign * dh;
                ioPt->h = beginPt.h + dh;
            }
        }
        else    // make it square
        {
            ioPt->v = beginPt.v + dh;
            ioPt->h = beginPt.h + dh;
        }
    }
}


//-----------------------------------------------------------------------------------
static void DoMouseArrowTracking( WindowRef window, CGContextRef overlayContext,
				     Point beginPt, UInt32 evtModifiers )
{
    const float     dashLengths[2]  = { 3, 3 };     // for drawing "marching ants"
    static float    phase           = 1.0;          // (make them march)

    enum {
        dragSelect  = 1,
        dragMove    = 2,
        dragResize  = 3
    };
    
    int                 dragState       = dragSelect;
    OSStatus            err;
    MouseTrackingResult	trackingResult;
    Point               endPt;
    CGPoint             cgStartPt;
    CGPoint             windowContextPt;
    DocStorage*         docStP          = (DocStorage*)GetWRefCon( window );
    CGRect              docRect         = CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v);
    CSkObjectPtr	tempObj         = NULL;
    CSkObjectPtr	hitObj          = NULL;
    CGrafPtr            port            = GetWindowPort(window);
    Boolean             beenDragging    = false;
    Boolean             isStillDown     = true;
    Boolean             shiftKeyDown    = ((evtModifiers & shiftKey) != 0);
    UInt32              grabNum;
    
    // also clip to document page, now
    CGRect cgDocRect = CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v);
    CGContextClipToRect(overlayContext, cgDocRect);
    CGContextClipToRect(docStP->bmCtx,  cgDocRect);

    cgStartPt = QDCoordsToCSkDocCGCoords(beginPt, docStP);
    
    // First, check if we are down on a selected object. If so, switch to dragMove or dragResize.
    // If not, stay in noDrag, and go to dragSelect if dragging continues.

    windowContextPt = CGPointMake( beginPt.h, (docStP->windowRect.bottom - docStP->windowRect.top) - beginPt.v );
    hitObj = DrawObjListHitTesting(&docStP->objList, docStP->bmCtx, docStP->displayCTM, windowContextPt, cgStartPt, &grabNum);
    if ( (hitObj != NULL) && IsDrawObjSelected(hitObj) )
    {
        dragState = (grabNum > 0 ? dragResize : dragMove);
        if (dragState == dragResize)
        {
            tempObj = CopyDrawObject(hitObj);
            MakeDrawObjTransparent(tempObj, 0.4);
            SetContextStateForDrawObject(overlayContext, tempObj);
        }
    }
    
    while ( isStillDown ) 
    {
        UInt32      modifiers;
		CGPoint     cgEndPt;
        
        err = TrackMouseLocationWithOptions( port, 0, 0, &endPt, &modifiers, &trackingResult );
        
        windowContextPt = CGPointMake( endPt.h, (docStP->windowRect.bottom - docStP->windowRect.top) - endPt.v );
        cgEndPt = QDCoordsToCSkDocCGCoords(endPt, docStP);

        switch ( trackingResult )
        {
            case kMouseTrackingMouseDragged:                
                beenDragging = true;
                CGContextClearRect( overlayContext, docRect );    // "Erase" the overlay window (doc coordinates!)
                // Either draw selection rectangle with marching ants, or move the selected objects, or resize the selected object
                
                if (dragState == dragSelect)
                {
                    if ( BigEnoughForDragSelection(beginPt, endPt) )
                    {
                        CGRect selectionRect = CGRectMake(cgStartPt.x, cgStartPt.y, cgEndPt.x - cgStartPt.x, cgEndPt.y - cgStartPt.y);
                        CGContextSetLineDash(overlayContext, phase, dashLengths, 2);
                        CGContextStrokeRect(overlayContext, selectionRect);
                        phase += 1.0;   // will use a timer to keep the ants marching, later ...
			
			if (!shiftKeyDown)
			    CSkObjListSetSelectState(&docStP->objList, false);
			CSkObjListSelectWithinRect(&docStP->objList, selectionRect);
                    }
		    else
		    {
			hitObj = DrawObjListHitTesting(&docStP->objList, docStP->bmCtx, docStP->displayCTM, windowContextPt, cgEndPt, NULL);
			
			if (hitObj != NULL)
			{
			    Boolean wasSelected = IsDrawObjSelected(hitObj);
			    
			    if (shiftKeyDown)   // extend selection, or toggle selected state
			    {
				SetDrawObjSelectState(hitObj, !wasSelected);
			    }
			    else if (!wasSelected)
			    {
				CSkObjListSetSelectState(&docStP->objList, false);
				SetDrawObjSelectState(hitObj, true);
			    }
			}
			else if (!shiftKeyDown)   // just deselect everything
			{
			    CSkObjListSetSelectState(&docStP->objList, false);
			}
		    }
                    DrawWindowContent(window);  // to reflect selection state changes
                }
                else if (dragState == dragMove)
                {
                    // redraw selected objects at offset cgEndPt - cgStartPt
                    RenderSelectedDrawObjs(overlayContext, &docStP->objList, cgEndPt.x - cgStartPt.x, cgEndPt.y - cgStartPt.y, 0.5);
                }
                else // if (dragState == dragResize)
                {
                    CSkShapeResize(GetCSkObjectShape(tempObj), &grabNum, cgEndPt);
                    RenderCSkObject(overlayContext, tempObj, true);
                }
                CGContextSynchronize(overlayContext);           //  Make sure we get our drawing to the screen
                break;

            case kMouseTrackingMouseUp:
                if (!beenDragging)          // there was no dragging, just a single click
                {
                    hitObj = DrawObjListHitTesting(&docStP->objList, docStP->bmCtx, docStP->displayCTM, windowContextPt, cgEndPt, NULL);
                            
                    if (shiftKeyDown == false)
                        CSkObjListSetSelectState(&docStP->objList,false);
                    
                    if (hitObj != NULL)
                    {
                        if (shiftKeyDown)
                            SetDrawObjSelectState(hitObj, !IsDrawObjSelected(hitObj));
                        else    
                            SetDrawObjSelectState(hitObj, true);
                    }
                }
                else
                {
                    if (dragState == dragMove)
                    {
                        MoveSelectedDrawObjs(&docStP->objList, cgEndPt.x - cgStartPt.x, cgEndPt.y - cgStartPt.y);
                    }
                    else if (dragState == dragResize)   // Move new shape geometry from tempObj to hitObj
		    		{
						memcpy( GetCSkObjectShape(hitObj), GetCSkObjectShape(tempObj), CSkShapeSize());
                    }
                }
                // fall through to isStillDown = false
                
            case kMouseTrackingUserCancelled:
                isStillDown = false;
                break;
        }
    }
    
    if ( (tempObj != NULL) )
    {
        ReleaseDrawObj(tempObj);
    }
    
    CGContextClearRect( overlayContext, docRect );
    
    // Make sure we always redraw the new state of the new object list
    DrawWindowContent(window); 

    SetThemeCursor( kThemeArrowCursor );
}   // DoMouseArrowTracking


//-----------------------------------------------------------------------------------
static void DoMouseToolTracking( WindowRef window, CGContextRef overlayContext,
				    int shapeSelect, Point beginPt )
{
    const CGrgba        trStrokeColor   = { 0.5, 0.5, 0.5, 0.4 };   // gray with alpha = 0.4
    const CGrgba        trFillColor     = { 0.9, 0.9, 0.9, 0.3 };   // ltgray with alpha = 0.3
    OSStatus            err;
    MouseTrackingResult	trackingResult;
    Point               endPt;
    CGPoint             cgStartPt;
    CGPoint             cgEndPt;
    CGRect              objBounds       = CGRectMake(0, 0, 0, 0);
    DocStorage*         docStP          = (DocStorage*)GetWRefCon( window );
    CGRect              docRect         = CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v);
    CSkObjectPtr		tempObj         = CreateDrawObj(docStP->toolPalette, shapeSelect);
    CGrafPtr            port            = GetWindowPort(window);
    Boolean             beenDragging    = false;
    Boolean             isStillDown     = true;
    
    CSkToolPaletteSetContextState(overlayContext, docStP->toolPalette);
    CGContextSetStrokeColor( overlayContext, (float*)&trStrokeColor);
    CGContextSetFillColor( overlayContext, (float*)&trFillColor);
    SetThemeCursor( kThemeCrossCursor );
         
    // also clip to document page
    {
        CGRect cgDocRect = CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v);
        CGContextClipToRect(overlayContext, cgDocRect);
        CGContextClipToRect(docStP->bmCtx,  cgDocRect);
    }

    cgStartPt = QDCoordsToCSkDocCGCoords(beginPt, docStP);
    
    while ( isStillDown ) 
    {
        UInt32      modifiers;
        
        err = TrackMouseLocationWithOptions( port, 0, 0, &endPt, &modifiers, &trackingResult );
        if (err != noErr)   // when would this occur?
            break;
        
	AdjustEndPoint(&endPt, beginPt, shapeSelect, modifiers);
		
        cgEndPt = QDCoordsToCSkDocCGCoords(endPt, docStP);

        switch ( trackingResult )
        {
            case kMouseTrackingMouseDragged:
                beenDragging = true;
                CGContextClearRect( overlayContext, docRect );    // "Erase" the overlay window (doc coordinates!)
                if (shapeSelect == kLineShape)
                {
                    CSkShapeSetLine(GetCSkObjectShape(tempObj), cgStartPt, cgEndPt);
                }
                else
                {
                    objBounds.origin = cgStartPt;   // need to reassign each time, because CGRectStandardize may change it
                    objBounds.size = CGSizeMake(cgEndPt.x - cgStartPt.x, cgEndPt.y - cgStartPt.y);
                    objBounds = CGRectStandardize(objBounds);   // make size components positive
                    CSkShapeSetBounds(GetCSkObjectShape(tempObj), objBounds);
                }
                RenderCSkObject(overlayContext, tempObj, false);
                CGContextSynchronize(overlayContext);           //  Make sure we get our drawing to the screen
                break;

            case kMouseTrackingMouseUp:
            case kMouseTrackingUserCancelled:
                isStillDown = false;
                break;
        }
    }
    
    CGContextClearRect( overlayContext, docRect );
    
    if ( beenDragging )  // we have created a new object
    {
        SetDrawObjAttributesFromToolPalette(tempObj, docStP->toolPalette);
        CSkObjListSetSelectState(&docStP->objList, false);
        SetDrawObjSelectState(tempObj, true);
        AddDrawObjToList(&docStP->objList, tempObj);
        tempObj = NULL;     // don't release it!
    }
    
    if ( tempObj != NULL )
    {
        ReleaseDrawObj(tempObj);
    }
    
    // Make sure we always redraw the new state of the new object list
    DrawWindowContent(window); 

    SetThemeCursor( kThemeArrowCursor );
    
    if (beenDragging && (shapeSelect >= kLineShape))  // set ToolPalette selection back to "Arrow"
    {
        ControlID   cntlID = { kControlSignaturePalette, kArrowTool };
        ControlRef  control;
        GetControlByID(docStP->toolPalette, &cntlID, &control);
        SendControlEventHit(control);
    }
}   // DoMouseToolTracking

//-----------------------------------------------------------------------------------------------------------------------
static void ControlActionProc( ControlRef control, ControlPartCode partCode )
{
    WindowRef       window      = GetControlOwner(control);
    DocStorage*  docStP         = (DocStorage*)GetWRefCon(window);
    SInt16          value       = GetControlValue(control);
    SInt16          minVal      = GetControlMinimum(control);
    SInt16          maxVal      = GetControlMaximum(control);
    int             pageIncr;
    int             buttonIncr;
    ControlID       cntlID;
    Boolean         horizontal;
    
    GetControlID(control, &cntlID);
    horizontal = (cntlID.id == kScrollHorizontalID);
    
    pageIncr = (horizontal ? 3 * (docStP->windowRect.right - docStP->windowRect.left) / 4 : 3 * (docStP->windowRect.bottom - docStP->windowRect.top) / 4);
    buttonIncr = (pageIncr > 10 ? pageIncr / 10 : 1);
        
    switch (partCode)
    {
        case kControlUpButtonPart:      value -= buttonIncr;    break;
        case kControlDownButtonPart:	value += buttonIncr;    break;
        case kControlPageUpPart:        value -= pageIncr;      break;
        case kControlPageDownPart:      value += pageIncr;      break;
    }
    
    if (value < minVal)
        value = minVal;
    else if (value > maxVal)
        value = maxVal;
    SetControlValue(control, value);
    
    if (horizontal)
        docStP->scrollPosition.h = value;
    else
        docStP->scrollPosition.v = value;
        
    RecalculateDisplayCTM(docStP);
    DrawWindowContent(window);                  // for live scrolling
}

//---------------------------------------------------------------------------------------------------------------------
// Create an overlay window for mouse tracking. Its "overlayWindowCtx" should behave the same as
// the window context in terms of drawing our objects (except for stroke and fill colors).
// Note that we do not attempt to keep the overlay window around for repeated use; instead, we
// create and destroy it each time. This allows to avoid WindowGroups, which would have to be
// used when dealing with more than one document window.
static OSStatus CreateOverlayWindow( Rect* inBounds, WindowRef* outOverlayWindow )
{
    UInt32 flags = kWindowHideOnSuspendAttribute | kWindowIgnoreClicksAttribute;
    OSStatus err = CreateNewWindow( kOverlayWindowClass, flags, inBounds, outOverlayWindow );
    require_noerr(err, CreateNewWindowFAILED);
    
//  SetPortWindowPort( *outOverlayWindow );
    ShowWindow( *outOverlayWindow );
    
CreateNewWindowFAILED:
    return err;
}


//-----------------------------------------------------------------------------------------------------------------------
static void DoContentClick(WindowRef window, Point where, UInt32 modifiers)
{
    DocStorage*     docStP      = (DocStorage*)GetWRefCon( window );
    ControlPartCode partCode;
    ControlRef      control     = FindControlUnderMouse ( where, window, &partCode );

    if (control != NULL)
    {
        HandleControlClick ( control, where, 0, ControlActionProc );
    }
    else
    {
		// Create an overlayWindow for mousetracking feedback
		WindowRef   overlayWindow;
		Rect	    wRect = docStP->windowRect;
		OSStatus    err;
		
		QDLocalToGlobalRect( GetWindowPort(window), &wRect );
		err = CreateOverlayWindow(&wRect, &overlayWindow);
		if (err == noErr)
		{
			CGContextRef overlayContext;
			CGColorSpaceRef genericColorSpace = GetGenericRGBColorSpace();
			int	    selectedTool = CSkToolPaletteGetTool(docStP->toolPalette);
	
			QDBeginCGContext(GetWindowPort(overlayWindow), &overlayContext);
			
			// ensure that we are drawing in the correct calibrated color space
			CGContextSetFillColorSpace(overlayContext, genericColorSpace); 
			CGContextSetStrokeColorSpace(overlayContext, genericColorSpace); 
			
			CGContextConcatCTM(overlayContext, docStP->displayCTM);
			
			// From now on, all drawing is done in "document page coordinates"
			
			if (selectedTool == kArrowTool)
			{
				DoMouseArrowTracking(window, overlayContext, where, modifiers);
				// if we end up with a new selection, adjust the toolpalette popup check marks
				CSkToolPaletteSyncPopups(FirstSelectedObject(&docStP->objList));
			}
			else
			{
				DoMouseToolTracking(window, overlayContext, MapToolToShape(selectedTool), where);
			}
			
			QDEndCGContext(GetWindowPort(overlayWindow), &overlayContext);
			
			// Always get rid of the overlayWindow. This way, we don't have to play around with window groups,
			// and still avoid problems when switching between several windows or between applications.
			DisposeWindow(overlayWindow);
		}
    }
}

//-----------------------------------------------------------------------------------------------------------------------
static void DoResize(WindowRef window, Rect* newBounds)
{
    const int       kScrollbarWidth = 15;
    DocStorage*  docStP             = (DocStorage*)GetWRefCon(window);
    ControlID       cntlID          = { kControlSignatureMainWindow, kScalePopupID };
    ControlRef      control;
    Rect            bounds          = *newBounds;;
    RgnHandle       clipRgn         = NewRgn();
    
    OffsetRect(&bounds, -bounds.left, -bounds.top);
    docStP->windowRect = bounds;
    
    // Resize and reposition controls
    GetControlByID(window, &cntlID, &control);
    GetControlBounds(control, &bounds);
    
    OffsetRect(&bounds, 0, docStP->windowRect.bottom - bounds.bottom);
    SetControlBounds(control, &bounds);
    
    cntlID.signature = kControlSignatureMainWindow;
    cntlID.id = kScrollHorizontalID;
    GetControlByID(window, &cntlID, &control);
    bounds.top = docStP->windowRect.bottom - kScrollbarWidth;
    bounds.left = bounds.right;                         // scrollbar left = popup button right
    bounds.bottom = docStP->windowRect.bottom;
    bounds.right = docStP->windowRect.right - kScrollbarWidth;
    SetControlBounds(control, &bounds);
    
    cntlID.id = kScrollVerticalID;
    GetControlByID(window, &cntlID, &control);
    bounds.top = 0;
    bounds.left = docStP->windowRect.right - kScrollbarWidth;
    bounds.bottom = docStP->windowRect.bottom - kScrollbarWidth;
    bounds.right = docStP->windowRect.right;
    SetControlBounds(control, &bounds);

    RecalculateDisplayCTM(docStP);
    SetupControls(window, docStP);
    RectRgn(clipRgn, &docStP->windowRect);
    InvalWindowRect(window, &docStP->windowRect);
}

//-----------------------------------------------------------------------------------------------------------------------
static size_t MyCFDataPutBytes(void* info, const void* buffer, size_t count)
{
	CFDataAppendBytes((CFMutableDataRef)info, buffer, count);
	return count;
}

static void MyCFDataRelease(void* info)
{
	CFRelease((CFMutableDataRef)info);
}

//-----------------------------------------------------------------------------------------------------------------------
void DrawIntoPDFPage(CGContextRef pdfContext, CGRect pageBounds, const DocStorage* docStP, UInt32 pageNumber)
{
#pragma unused(pageNumber)
	CGColorSpaceRef genericColorSpace = GetGenericRGBColorSpace();

	CGContextBeginPage(pdfContext, &pageBounds);
	
	// ensure that we are drawing in the correct color space, a calibrated color space
	CGContextSetFillColorSpace(pdfContext, genericColorSpace); 
	CGContextSetStrokeColorSpace(pdfContext, genericColorSpace); 
	
	if (docStP->pdfData != NULL)
	{
		DrawPDFData(pdfContext, docStP->pdfData, pageBounds);
	}
    RenderDrawObjList(pdfContext, &docStP->objList, false);
	CGContextEndPage(pdfContext);
}

//-----------------------------------------------------------------------------------------------------------------------
static OSStatus 
AddWindowContentToPasteboardAsPDF( PasteboardRef pasteboard, const DocStorage* docStP)
{
	OSStatus				err		= noErr;
	CGRect					docRect = CGRectMake(0, 0, docStP->docSize.h, docStP->docSize.v);
	CFDataRef				pdfData = CFDataCreateMutable( kCFAllocatorDefault, 0);
	CGContextRef			pdfContext;
	CGDataConsumerRef		consumer;
	CGDataConsumerCallbacks cfDataCallbacks = { MyCFDataPutBytes, MyCFDataRelease };
	
	// We need to clear the pasteboard of it's current contents so that this application can
	// own it and add it's own data.
	err = PasteboardClear( pasteboard );
	require_noerr( err, PasteboardClear_FAILED );

	consumer = CGDataConsumerCreate((void*)pdfData, &cfDataCallbacks);
	
	// For now (and for demo purposes), just put the whole window drawing as pdf data
	// on the paste board, regardless of what is selected in the window.
	pdfContext = CGPDFContextCreate(consumer, &docRect, NULL);
	require(pdfContext != NULL, CGPDFContextCreate_FAILED);
	
	DrawIntoPDFPage(pdfContext, docRect, docStP, 1);
	CGContextRelease(pdfContext);   // this finalizes the pdfData
	
	err = PasteboardPutItemFlavor( pasteboard, (PasteboardItemID)1,
						CFSTR("com.adobe.pdf"), pdfData, 0 );
	require_noerr( err, PasteboardPutItemFlavor_FAILED );
	
CGPDFContextCreate_FAILED:
PasteboardPutItemFlavor_FAILED:
	CGDataConsumerRelease(consumer);	// this also releases the pdfData, via MyCFDataRelease

PasteboardClear_FAILED:
	return err;
}

//-----------------------------------------------------------------------------------------------------------------------
// Check whether the pasteboard contains pdf data. If so, return the CFDataRef in the pdfData parameter, if it's not NULL.
static Boolean PasteboardContainsPDF(PasteboardRef inPasteboard, CFDataRef* pdfData)
{
	Boolean				gotPDF		= false;
	OSStatus			err			= noErr;
	PasteboardSyncFlags	syncFlags   = PasteboardSynchronize(inPasteboard);
	ItemCount			itemCount;
	UInt32				itemIndex;
	
	if (syncFlags & kPasteboardModified)
	{
		fprintf(stderr, "Pasteboard modified\n");
	}
	
	// Count the number of items on the pasteboard so we can iterate through them.
	err = PasteboardGetItemCount(inPasteboard, &itemCount);
	require_noerr(err, PasteboardGetItemCount_FAILED);
	
	for (itemIndex = 1; itemIndex <= itemCount; ++itemIndex)
	{
		PasteboardItemID	itemID;
		CFArrayRef			flavorTypeArray;
		CFIndex				flavorCount;
		CFIndex				flavorIndex;
		
		// Every item is identified by a unique value.
		err = PasteboardGetItemIdentifier( inPasteboard, itemIndex, &itemID );
		require_noerr( err, PasteboardGetItemIdentifier_FAILED );
		
		// The flavorTypeArray is a CFType and we'll need to call CFRelease on it later.
		err = PasteboardCopyItemFlavors( inPasteboard, itemID, &flavorTypeArray );
		require_noerr( err, PasteboardCopyItemFlavors_FAILED );
		
		flavorCount = CFArrayGetCount( flavorTypeArray );
		
		for (flavorIndex = 0; flavorIndex < flavorCount; ++flavorIndex)
		{
			CFStringRef				flavorType;
			CFComparisonResult		comparisonResult;
			
			// grab the flavorType so we can extract it's flags and data
			flavorType = (CFStringRef)CFArrayGetValueAtIndex( flavorTypeArray, flavorIndex );

			// Don't care about flavorFlags, for now
			{
//			PasteboardFlavorFlags	flavorFlags;
//			err = PasteboardGetItemFlavorFlags( inPasteboard, itemID, flavorType, &flavorFlags );
//			require_noerr( err, PasteboardGetItemFlavorFlags_FAILED );
			}

			// Look at the flavorTypeStr for debugging:
			{
//			char flavorTypeStr[128];
//			CFStringGetCString( flavorType, flavorTypeStr, 128, kCFStringEncodingMacRoman );
//			fprintf(stderr, "%s\n", flavorTypeStr);
			}
			
			// If it's a pdf, get it and return true
			comparisonResult = CFStringCompare(flavorType, CFSTR("com.adobe.pdf"), 0);
			if (comparisonResult == kCFCompareEqualTo)
			{
				if (pdfData != NULL)
				{
					err = PasteboardCopyItemFlavorData( inPasteboard, itemID, flavorType, pdfData );
					require_noerr( err, PasteboardCopyItemFlavorData_FAILED );
				}
				gotPDF = true;
				break;
			}
						
PasteboardCopyItemFlavorData_FAILED:
//PasteboardGetItemFlavorFlags_FAILED:
			;
		}
		
		CFRelease(flavorTypeArray);

PasteboardCopyItemFlavors_FAILED:
PasteboardGetItemIdentifier_FAILED:
		;
	}
	
PasteboardGetItemCount_FAILED:	
	return gotPDF;
}

//-----------------------------------------------------------------------------------------------------------------------
static void AttachPDFToWindow(CFDataRef pdfData, DocStorage* docStP)
{
	if (docStP->pdfData != NULL)
	{
		CFRelease(docStP->pdfData);
		docStP->pdfData = NULL;
	}
	docStP->pdfData = CFRetain(pdfData);
}

//-----------------------------------------------------------------------------------------------------------------------
static void DoCommandUpdateStatus(EventRef inEvent, WindowRef window, DocStorage* docStP)
{
#pragma unused(inEvent, window, docStP)		// for now
	MenuRef			menu;
	MenuItemIndex   unused;
	
	// For now, always enable "Copy" to put a (potentially empty) pdf on the pasteboard
	GetIndMenuItemWithCommandID(NULL, kHICommandCopy, 1, &menu, &unused);
	EnableMenuCommand(menu, kHICommandCopy);
	
	// For now, enable "Paste" only if we have a pdf on the pasteboard
	GetIndMenuItemWithCommandID(NULL, kHICommandPaste, 1, &menu, &unused);
	if ( PasteboardContainsPDF(GetPasteboard(), NULL) )
		EnableMenuCommand(menu, kHICommandPaste);
	else
		DisableMenuCommand(menu, kHICommandPaste);
}

//-----------------------------------------------------------------------------------------------------------------------
static OSStatus DoCommandProcess(EventRef inEvent, WindowRef window, DocStorage* docStP)
{
    OSStatus            err			= eventNotHandledErr;
    CGrgba              color;
    HICommandExtended   command;
    short				itemNo;
    Boolean             scaleChanged = false;
    
    GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command );

//  { char* c = &command.commandID;
//  fprintf(stderr, "kEventClassCommand, commandID = %c%c%c%c\n", c[0], c[1], c[2], c[3]); }

    switch (command.commandID)
    {
		case kHICommandCopy:
		// for now, only demonstrate how to put the current document content as 'pdf' on the clip board
		{
			err = AddWindowContentToPasteboardAsPDF( GetPasteboard(), docStP);
		}
		break;

		case kHICommandPaste:
		// for now, only demonstrate how to paste in a 'pdf' from the clip board
		{
			CFDataRef pdfData;
			if ( PasteboardContainsPDF(GetPasteboard(), &pdfData) )
			{
				AttachPDFToWindow(pdfData, docStP); // this will retain the pdfData in docStP
				CFRelease(pdfData);
			}
			else
			{
				fprintf(stderr, "kHICommandPaste: Paste menu item should have been disabled!\n");
			}
		}
		break;

        case kHICommandClose:
            // For now, do nothing here; kEventWindowClose will deal with window closing.
	    	// Later, we'll ask whether a "dirty" window needs to be saved.
        break;
		
        case kHICommandPageSetup:
        case kHICommandPrint:
        {
            ProcessPrintCommand(docStP, command.commandID);
	    	err = noErr;
        }
        break;
        
        case kCmdWritePDF:
			(void)SaveAsPDFDocument(window, docStP);
	    	err = noErr;
		break;
			
        case kHICommandClear:       
			RemoveSelectedDrawObjs(&docStP->objList); 
			err = noErr;
		break;
        
		case kHICommandSelectAll:
			CSkObjListSetSelectState(&docStP->objList, true);
			err = noErr;
		break;
        
        case kCmdDuplicate:
			DuplicateSelectedDrawObjs(&docStP->objList, docStP->dupOffset);
			err = noErr;
		break;
        
        case kCmdMoveForward:
			MoveObjectForward(&docStP->objList);
			err = noErr;
		break;

        case kCmdMoveToFront:
			MoveObjectToFront(&docStP->objList);
			err = noErr;
		break;
        
        case kCmdMoveBackward:
			MoveObjectBackward(&docStP->objList);
			err = noErr;
		break;
        
        case kCmdMoveToBack:
			MoveObjectToBack(&docStP->objList);
			err = noErr;
		break;
        
        case kCmdScale50:
			docStP->scale = 0.5;   scaleChanged = true;
			itemNo = 1;
			err = noErr;
		break;
	
        case kCmdScale100:
			docStP->scale = 1.0;   scaleChanged = true;
			itemNo = 2;
			err = noErr;
		break;
        
        case kCmdScale200:
			docStP->scale = 2.0;   scaleChanged = true;
			itemNo = 3;
			err = noErr;
	    break;

	// Line Width
	case kCmdWidthNone:      
        SetLineWidthOfSelecteds(&docStP->objList, 0.0);
	    err = noErr;
	    break;
	case kCmdWidthOne:       
        SetLineWidthOfSelecteds(&docStP->objList, 1.0);
	    err = noErr;
	    break;
	case kCmdWidthTwo:       
        SetLineWidthOfSelecteds(&docStP->objList, 2.0);
	    err = noErr;
	    break;
	case kCmdWidthFour:      
        SetLineWidthOfSelecteds(&docStP->objList, 4.0);
	    err = noErr;
	    break;
	case kCmdWidthEight:     
        SetLineWidthOfSelecteds(&docStP->objList, 8.0);
	    err = noErr;
	    break;
	case kCmdWidthSixteen:   
        SetLineWidthOfSelecteds(&docStP->objList, 16.0);
	    err = noErr;
	    break;
	case kCmdWidthThinner:   
        SetLineWidthOfSelecteds(&docStP->objList, kMakeItThinner);
	    err = noErr;
	    break;
	case kCmdWidthThicker:
        SetLineWidthOfSelecteds(&docStP->objList, kMakeItThicker);
	    err = noErr;
	    break;
	case kCmdLineWidthChanged:  
		SetLineWidthOfSelecteds(&docStP->objList, CSkToolPaletteGetLineWidth(docStP->toolPalette));
	    err = noErr;
	    break;
        
	// Line Cap
	case kCmdCapButt:        
            SetLineCapOfSelecteds(&docStP->objList, kCGLineCapButt);
	    err = noErr;
	    break;
	case kCmdCapRound:       
            SetLineCapOfSelecteds(&docStP->objList, kCGLineCapRound);
	    err = noErr;
	    break;
	case kCmdCapSquare:      
            SetLineCapOfSelecteds(&docStP->objList, kCGLineCapSquare);
	    err = noErr;
	    break;
        case kCmdLineCapChanged:
            SetLineCapOfSelecteds(&docStP->objList, CSkToolPaletteGetLineCap(docStP->toolPalette));
	    err = noErr;
	    break;
        
	// Line Join
	case kCmdJoinMiter:
            SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinMiter);
	    err = noErr;
	    break;
	case kCmdJoinRound:
            SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinRound);
	    err = noErr;
	    break;
	case kCmdJoinBevel:
            SetLineJoinOfSelecteds(&docStP->objList, kCGLineJoinBevel);
	    err = noErr;
	    break;
        case kCmdLineJoinChanged:
            SetLineJoinOfSelecteds(&docStP->objList, CSkToolPaletteGetLineJoin(docStP->toolPalette));
	    err = noErr;
	    break;
        
	// Line Style
	case kCmdStyleSolid:
            SetLineStyleOfSelecteds(&docStP->objList, kStyleSolid);
	    err = noErr;
	    break;
	case kCmdStyleDashed:
            SetLineStyleOfSelecteds(&docStP->objList, kStyleDashed);
	    err = noErr;
	    break;
        case kCmdLineStyleChanged:
            SetLineStyleOfSelecteds(&docStP->objList, CSkToolPaletteGetLineStyle(docStP->toolPalette));
	    err = noErr;
	    break;
        
        case kCmdStrokeColorChanged:
            SetStrokeColorOfSelecteds(&docStP->objList, CSkToolPaletteGetStrokeColor(docStP->toolPalette, &color));
	    err = noErr;
	    break;
        
        case kCmdFillColorChanged:
            SetFillColorOfSelecteds(&docStP->objList, CSkToolPaletteGetFillColor(docStP->toolPalette, &color));
	    err = noErr;
	    break;
        
        case kCmdStrokeAlphaChanged:
        {
            ControlID   cntlID  = { kControlSignaturePalette, kStrokeAlphaSlider };
            ControlRef  control;
            CGrgba      color;
            float       alpha;
            
            GetControlByID(docStP->toolPalette, &cntlID, &control);
            alpha = (float)GetControlValue(control) / 100.0;
            CSkToolPaletteGetStrokeColor(docStP->toolPalette, &color);
            color.a = alpha;
            CSkToolPaletteSetStrokeColor(docStP->toolPalette, &color);
            SetStrokeAlphaOfSelecteds(&docStP->objList, alpha);
	    err = noErr;
        }
        break;
        
        case kCmdFillAlphaChanged:
        {
            ControlID   cntlID  = { kControlSignaturePalette, kFillAlphaSlider };
            ControlRef  control;
            CGrgba      color;
            float       alpha;
            
            GetControlByID(docStP->toolPalette, &cntlID, &control);
            alpha = (float)GetControlValue(control) / 100.0;
            CSkToolPaletteGetFillColor(docStP->toolPalette, &color);
            color.a = alpha;
            CSkToolPaletteSetFillColor(docStP->toolPalette, &color);
            SetFillAlphaOfSelecteds(&docStP->objList, alpha);
	    err = noErr;
        }
        break;
    }   // switch
    
    if (scaleChanged)
    {
        ControlID   cntlID  = { kControlSignatureMainWindow, kScalePopupID };
        ControlRef  control;
        Str255      text;
		
        GetControlByID(window, &cntlID, &control);
        GetMenuItemText(command.source.menu.menuRef, command.source.menu.menuItemIndex, text);
        SetControlTitle(control, text);
		SetControlData(control, kControlEntireControl, kControlBevelButtonMenuValueTag, sizeof(itemNo), &itemNo);
        SetupControls(window, docStP);                  // do the scroll thumbs still need to be drawn?
        RecalculateDisplayCTM(docStP);
    }
    
    DrawWindowContent(window);                          // redraw unconditionally, for now
    
    return err;
}   // DoCommandProcess


//-----------------------------------------------------------------------------------------------------------------------
static pascal OSStatus CSkWindowEventHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
    #pragma unused ( inCallRef )
    OSStatus	err		= eventNotHandledErr;
    UInt32	eventKind   = GetEventKind( inEvent );
    UInt32	eventClass  = GetEventClass( inEvent );

    WindowRef   window  = (WindowRef)inUserData;
    DocStorage*  docStP = (DocStorage*)GetWRefCon( window );
    
    switch ( eventClass )
    {
    case kEventClassKeyboard:
        if (eventKind == kEventRawKeyDown )
        {
            char ch;
            
            err = GetEventParameter( inEvent, 
                                    kEventParamKeyMacCharCodes, 
                                    typeChar,
                                    NULL,
                                    sizeof( char ), 
                                    NULL, 
                                    &ch );
            if (ch == kBackspaceCharCode)
            {
                HICommand hiCmd;
                memset(&hiCmd, 0, sizeof(hiCmd));
                hiCmd.commandID = kHICommandClear;
                SendWindowCommandEvent(window, &hiCmd);
            }
        }
        
    case kEventClassWindow:
        if ( eventKind == kEventWindowDrawContent )
        {
            DrawWindowContent(window);
			DrawControls(window);
			err = noErr;
        }
        else if ( eventKind == kEventWindowClose )	// Dispose extra window storage here
        {
            ReleaseDocumentStorage( docStP );
            // should check whether this is the last window, and if so, close the toolPalette
        }
        else if ( eventKind == kEventWindowClickContentRgn )
        {
            Point   where;
            UInt32  modifiers;
            
            GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &where);
            GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifiers);
            QDGlobalToLocalPoint(GetWindowPort(window), &where);
            DoContentClick(window, where, modifiers);
            //err = noErr;
        }
        else if ( eventKind == kEventWindowBoundsChanged )
        {
            Rect r;
            (void) GetEventParameter( inEvent, kEventParamCurrentBounds, typeQDRectangle, NULL, sizeof(Rect), NULL, &r );
            // We get here also when the window gets moved and doesn't need to be redrawn.
			if ( (RECTWIDTH(docStP->windowRect) != RECTWIDTH(r)) || (RECTHEIGHT(docStP->windowRect) != RECTHEIGHT(r)) )
			{
				DoResize(window, &r);
			}
        }
        break;
            
    case kEventClassCommand:
        if ( eventKind == kEventCommandUpdateStatus )
		{
			DoCommandUpdateStatus(inEvent, window, docStP);
		}
        else if ( eventKind == kEventCommandProcess )
        {
            err = DoCommandProcess(inEvent, window, docStP);
        }
        break;
    }

    return err;
}


//-----------------------------------------------------------------------------------------------------------------------
OSStatus NewCSkWindow(IBNibRef theNibRef, WindowRef toolPalette)
{
    const EventTypeSpec	windowEvents[]	=   {   { kEventClassCommand,   kEventCommandProcess },
												{ kEventClassCommand,   kEventCommandUpdateStatus },
                                                { kEventClassKeyboard, 	kEventRawKeyDown },
                                                { kEventClassWindow,    kEventWindowClickContentRgn },
                                                { kEventClassWindow, 	kEventWindowDrawContent },
                                                { kEventClassWindow,    kEventWindowBoundsChanged },
                                                { kEventClassWindow,    kEventWindowClose }
                                            };
											
    static EventHandlerUPP  gCSkWindowEventProc = NULL;
    WindowRef				window;
    OSStatus				err		= CreateWindowFromNib( theNibRef, CFSTR("MainWindow"), &window );

    if (err == noErr)
    {
        DocStorage*  docStP = CreateDocumentStorage(window, toolPalette);
        SetWRefCon( window, (long) docStP );    // Should use SetWindowProperty()
        
		if (gCSkWindowEventProc == NULL)
		{
			gCSkWindowEventProc = NewEventHandlerUPP(CSkWindowEventHandler);
			if (gCSkWindowEventProc == NULL)
				return memFullErr;		// We are screwed. Really.
		}

        err = InstallWindowEventHandler( window, gCSkWindowEventProc, GetEventTypeCount(windowEvents), windowEvents, window, NULL );

        SetupControls(window, docStP);

        ShowWindow( window );
    }
	return err;
}
