/*
	File:		PICTFile.c

	Contains:	PICT file for simple text application

	Version:	Mac OS X

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

	Copyright � 1993-2001 Apple Computer, Inc., All Rights Reserved
*/

#include "MacIncludes.h"

#if defined(USE_UMBRELLA_HEADERS) && USE_UMBRELLA_HEADERS
#include <QuickTime.h>
#else
#include "ImageCompression.h"
// #include "GraphicsImporter.h"
#endif

#include "PICTFile.h"

/* ------------------------------------------------------------------------------------	*/
/* GLOBALS ONLY USED BY THIS FILE */
/* ------------------------------------------------------------------------------------	*/
#define kBufferSize	2048

static short			gPictFileRefNum;
static char				gPictureBuffer[kBufferSize];
static long				gAmountInBuffer;
static long				gFirstCharInBuffer;
//static QDProcsPtr     	gSavedProcs;
static QDProcs        	gMyProcs;
static CQDProcs       	gMyColorProcs;

// --------------------------------------------------------------------------------------------------------------
// INTERNAL ROUTINES
// --------------------------------------------------------------------------------------------------------------

static pascal void GetPICTData(void* inDataPtr ,short byteCount)
/*
	replacement for the QuickDraw bottleneck routine
*/
{ 
    OSStatus	err = noErr;
    long	longCount;
    Ptr		dataPtr = inDataPtr;

    longCount = byteCount;
	
	while ( (longCount > 0) && (err == noErr) )
		{
		if (gAmountInBuffer == 0)
			{
			gAmountInBuffer = kBufferSize;
			gFirstCharInBuffer = 0;
			err = FSRead(gPictFileRefNum,&gAmountInBuffer,gPictureBuffer);
			}

		if (gAmountInBuffer > 0)
			{
			long	amountToMove;
			
			amountToMove = gAmountInBuffer;
			if (amountToMove > longCount)
				amountToMove = longCount;
				
			BlockMoveData(&gPictureBuffer[gFirstCharInBuffer], dataPtr, amountToMove);
			longCount -= amountToMove;
			dataPtr += amountToMove;
			gFirstCharInBuffer += amountToMove;
			gAmountInBuffer -= amountToMove;
			}
			
		}
	
} // GetPICTData


// --------------------------------------------------------------------------------------------------------------

static OSStatus	DiskPictureDraw( WindowDataPtr pData, 
	Boolean doDraw, Rect *pictureRect, Point *pOffset)
{

	OSStatus	anErr;
	PicHandle	picHandle;
	Point		offset;
	FSSpec		fileSpec;
	
	if (pOffset)
		offset = *pOffset;
	else
		{
		offset.h = GetControlValue(pData->hScroll);
		offset.v = GetControlValue(pData->vScroll);
		}

	if( ((PICTDataPtr)pData)->isQuickTimeImageFile )
		{
		GraphicsImportComponent theGraphicsImporter = 0;
		
		anErr = FSGetCatalogInfo( &pData->fileRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL );
		anErr = GetGraphicsImporterForFile( &fileSpec, &theGraphicsImporter );
		
		if( noErr == anErr )
			{
			anErr = GraphicsImportSetQuality(
							theGraphicsImporter, 
							codecHighQuality );
			}
		
		if( noErr == anErr )
			{
			Rect	destRect = ((PICTDataPtr)pData)->pictureRectangle;
			
			OffsetRect(&destRect, 
								-destRect.left + 
									pData->contentRect.left -
									offset.h, 
								-destRect.top + 
									pData->contentRect.top -
									offset.v);

			anErr = GraphicsImportSetBoundsRect( theGraphicsImporter, &destRect ); 
			}
		
		if( noErr == anErr )
			{
			RgnHandle	theGripClip = NewRgn();
			RectRgn( theGripClip, &pData->contentRect );
			anErr = GraphicsImportSetClip( theGraphicsImporter, theGripClip );
			DisposeRgn( theGripClip );
			}
		
		if( noErr == anErr )
			{
			anErr = GraphicsImportDraw( theGraphicsImporter );
			}
		
		if( 0 != theGraphicsImporter )
			{
			CloseComponent( theGraphicsImporter );
			}
		
		return anErr;
		}

	picHandle = ((PICTDataPtr)pData)->cacheHandle;
	
	if (picHandle)
		{


		if (doDraw)
			{
			Rect	destRect;
			
			GetPICTRectangleAt72dpi(picHandle, &destRect);

			OffsetRect(&destRect, 
								-destRect.left + 
									pData->contentRect.left -
									offset.h, 
								-destRect.top + 
									pData->contentRect.top -
									offset.v);
			DrawPicture(picHandle, &destRect);
			}
		
		if (pictureRect)
			{
			GetPICTRectangleAt72dpi(picHandle, pictureRect);
			OffsetRect(pictureRect, -pictureRect->left, -pictureRect->top);
			}
			
		picHandle = nil;
		anErr = noErr;
		}
	else
		{
		// make enough room for PICT header, including version
		picHandle = (PicHandle) NewHandle(sizeof(Picture)  + sizeof(long)*8);
		anErr = MemError();
		nrequire(anErr, FailedNewHandle);
		
		gPictFileRefNum = pData->dataRefNum;
		anErr = SetFPos(gPictFileRefNum, fsFromStart, 512);
		nrequire(anErr, FailedSetFPos);
		
		gAmountInBuffer = kBufferSize;
		gFirstCharInBuffer = 0;
		anErr = FSRead(gPictFileRefNum, &gAmountInBuffer, gPictureBuffer);
		if (anErr == eofErr)
			anErr = noErr;
		if (gAmountInBuffer < sizeof(Picture))
			anErr = eofErr;
		nrequire(anErr, FailedInitialRead);
	
		// copy PICT header, including version
		BlockMoveData(gPictureBuffer, *picHandle, sizeof(Picture) + sizeof(long)*8);
		gFirstCharInBuffer += sizeof(Picture);
		gAmountInBuffer -= sizeof(Picture);
		
		if (doDraw)
			{
			Rect	destRect;
#if USE_QD_GRAFPROCS
			CGrafPtr thePort = GetQDGlobalsThePort();
#endif
			
			GetPICTRectangleAt72dpi(picHandle, &destRect);
			if (!gMachineInfo.haveQuickTime)
				{
				static QDGetPicUPP gGetPICTData = NULL;
#if USE_QD_GRAFPROCS
				CQDProcs gprocs;
#endif

				if (!gGetPICTData) {
					gGetPICTData = NewQDGetPicUPP(GetPICTData);
				}

#if USE_QD_GRAFROCS
				if (GetPortGrafProcs(thePort, &gprocs))
					SetPortGrafProcs(thePort, &gMyColorProcs);
				else
					SetStdCProcs(&gMyColorProcs);
#endif
				gMyProcs.getPicProc = gGetPICTData;
				gMyColorProcs.getPicProc = gGetPICTData;
				}
				
			OffsetRect(&destRect, -destRect.left + 						pData->contentRect.left - offset.h, 
				-destRect.top + pData->contentRect.top -
				offset.v);
												
			if (!gMachineInfo.haveQuickTime)
				{
				DrawPicture(picHandle, &destRect);
				}
			else
				DrawPictureFile(gPictFileRefNum, &destRect, nil);
			}
			
		if (pictureRect)
			{
			GetPICTRectangleAt72dpi(picHandle, pictureRect);
			OffsetRect(pictureRect, -pictureRect->left, 
					-pictureRect->top);
			}
		}
	
// FALL THROUGH EXCEPTION HANDLING
FailedInitialRead:
FailedSetFPos:
	DisposeHandle((Handle) picHandle);

FailedNewHandle:
	
	return(anErr);
		
} // DiskPictureDraw

// --------------------------------------------------------------------------------------------------------------
static OSStatus GetSelectedPicture(WindowDataPtr pData, PicHandle *pResult)
{
	OSStatus		anErr;
	PicHandle	scrapPict;
	Rect		globRect;
	GDHandle	theMaxDevice;
	short		theDepth;
	GDHandle	savedGDevice;
	CGrafPtr	savedPort;
	GWorldPtr	offscreenGWorld;
        RgnHandle	visRgn;
	Rect		bounds;
        PixMapHandle	bMap;

	// save away current value for restores
	GetGWorld(&savedPort, &savedGDevice);

	// determine the best way in which to allocate stuff
	
	SetRect(&globRect, -32760, -32760, 32760, 32760);

	theDepth = 8;
	theMaxDevice = GetMaxDevice(&globRect);
	if (theMaxDevice != nil)
		theDepth = (**(**theMaxDevice).gdPMap).pixelSize;
	
	// allocate the GWorld in temp mem, or local if we run out
	anErr = NewGWorld(&offscreenGWorld, theDepth, 
						&((PICTDataPtr)pData)->selectionRectangle,
						nil, nil, useTempMem);
	if (anErr != noErr)
		anErr = NewGWorld(&offscreenGWorld, theDepth, 
							&((PICTDataPtr)pData)->selectionRectangle,
							nil, nil, 0);
	nrequire(anErr, FailedNewGWorld);

	// blow open the visRgn, and clip to the selected area
	visRgn = NewRgn();
	GetPortVisibleRegion(offscreenGWorld, visRgn);
	RectRgn(visRgn, &globRect);
	DisposeRgn(visRgn);
	PortChanged(offscreenGWorld);
	SetGWorld(offscreenGWorld, nil);
	EraseRect(GetPortBounds(offscreenGWorld, &bounds));
	ClipRect(&((PICTDataPtr)pData)->selectionRectangle);
	
	// Draw the picture into the offscreen
	LockPixels( GetGWorldPixMap(offscreenGWorld));
	{
	Point	offset = {0,0};
	anErr = DiskPictureDraw(pData, true, nil, &offset);
	}
	UnlockPixels( GetGWorldPixMap(offscreenGWorld));
	nrequire(anErr, FailedDraw);

	// CopyBits in place to grab the selection
	scrapPict = OpenPicture(&((PICTDataPtr)pData)->selectionRectangle);
	LockPixels( GetGWorldPixMap(offscreenGWorld));
        bMap = GetPortPixMap(offscreenGWorld);
	CopyBits((BitMap *)*bMap, (BitMap *)*bMap, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->selectionRectangle, srcCopy, nil);
	anErr = QDError();
	UnlockPixels( GetGWorldPixMap(offscreenGWorld));
	ClosePicture();
	QDGetPictureBounds(scrapPict, &globRect);
	if ( (anErr == noErr) && (EmptyRect(&globRect)) )
		anErr = memFullErr;
		
	// done with our offscreen now
	SetGWorld(savedPort, savedGDevice);
	DisposeGWorld(offscreenGWorld);

	if (anErr == noErr)
		*pResult = scrapPict;
	
	return(anErr);
	
// EXCEPTION HANDLING
FailedDraw:
	DisposeGWorld(offscreenGWorld);
	
FailedNewGWorld:

	SetGWorld(savedPort, savedGDevice);
	return(anErr);
	
} // GetSelectedPicture

// --------------------------------------------------------------------------------------------------------------
static OSStatus CopyGWorld(WindowDataPtr pData)
{
	OSStatus	anErr;
	PicHandle	scrapPict;
	ScrapRef	scrap;

	anErr = GetSelectedPicture(pData, &scrapPict);

	if (!anErr)
	{
		do
		{
			if (anErr) break;
			anErr = ClearCurrentScrap ( );
			if (anErr) break;
			anErr = GetCurrentScrap (&scrap);
			MoveHHi ((Handle) scrapPict);
			anErr = MemError ( );
			if (anErr) break;
			HLock ((Handle) scrapPict);
			anErr = MemError ( );
			if (anErr) break;
			anErr = PutScrapFlavor (scrap, 'PICT', 0, GetHandleSize((Handle) scrapPict), *scrapPict);
			if (anErr) break;
		}
		while (false);

		KillPicture(scrapPict);
	}

	return anErr;
	
} // CopyGWorld

// --------------------------------------------------------------------------------------------------------------
static pascal OSErr PICTSendDataProc(FlavorType theType, void *dragSendRefCon,
								ItemReference theItem, DragReference theDrag)
/*
 *	The ItemReference is the gxShape to be sent. The dragSendRefCon is ignored.
*/
{
#pragma unused (theItem)

	OSStatus	result = noErr;

	switch (theType) 
		{
		case 'PICT':
			{	
			PicHandle 	pict;
			OSStatus		anErr = GetSelectedPicture((WindowDataPtr)dragSendRefCon, &pict);
	
			if (anErr == noErr)
				{	
				HLock((Handle)pict);
				result = SetDragItemFlavorData(theDrag, 1, 'PICT', (Ptr)*pict, GetHandleSize((Handle)pict), 0);
				KillPicture(pict);
				}
			}
			break;
			
		default:
			result = badDragFlavorErr;
			break;
		}
		
	return result;
	
} // PICTSendDataProc


// --------------------------------------------------------------------------------------------------------------
// OOP INTERFACE ROUTINES
// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTUpdateWindow(WindowPtr pWindow, WindowDataPtr pData)
{
	OSStatus		anErr;
	RgnHandle	oldClip = NewRgn();
	
	EraseRect(&pData->contentRect);
	
	GetClip(oldClip);
	ClipRect(&pData->contentRect);
	anErr = DiskPictureDraw( pData, true, nil, nil);
	SetClip(oldClip);
	DisposeRgn(oldClip);
	
	DrawSelection(pData, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->patternPhase, false);
	
	DrawControls(pWindow);
	DrawGrowIcon(pWindow);
	
	return(anErr);
	
} // PICTUpdateWindow

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTGetDocumentRect(WindowPtr pWindow, WindowDataPtr pData, 
			LongRect * documentRectangle, Boolean forGrow)
{
#pragma unused (pWindow, forGrow)

	RectToLongRect(&((PICTDataPtr)pData)->pictureRectangle, documentRectangle);
	
	return(noErr);
	
} // PICTGetDocumentRect

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTCloseWindow(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	DisposeHandle( (Handle) (((PICTDataPtr)pData)->cacheHandle));
	
	return(noErr);
	
} // PICTCloseWindow

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTContentClick(WindowPtr pWindow, WindowDataPtr pData, EventRecord *pEvent)
{
	OSStatus			anErr = noErr;
	Rect			selectionRect = ((PICTDataPtr)pData)->selectionRectangle;
	Point			clickPoint = pEvent->where;
	ControlHandle	theControl;
	
	GlobalToLocal(&clickPoint);
	if (FindControl(clickPoint, pWindow, &theControl) == 0)
		{
		OffsetRect(&selectionRect, -GetControlValue(pData->hScroll), -GetControlValue(pData->vScroll));
		if ( (gMachineInfo.haveDragMgr) && (PtInRect(clickPoint, &selectionRect)) )
			{
			DragAndDropArea(pWindow, pData, pEvent, 
								&selectionRect);
			}
		else
			{
			anErr = SelectContents(pWindow, pData, pEvent, 
						&((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->pictureRectangle, 
						&((PICTDataPtr)pData)->patternPhase);
			}
		}
	
	return(anErr);
	
} // PICTContentClick

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTAdjustCursor(WindowPtr pWindow, WindowDataPtr pData, Point * localMouse, RgnHandle globalRgn)
{
#pragma unused (pWindow)

	OSStatus			anErr = noErr;
	CursHandle		theCross;
	Rect			selectionRect = ((PICTDataPtr)pData)->selectionRectangle;
	Rect			globalSelection;
	
	OffsetRect(&selectionRect, -GetControlValue(pData->hScroll), -GetControlValue(pData->vScroll));
	
	globalSelection = selectionRect;
	LocalToGlobal(&TopLeft(globalSelection));
	LocalToGlobal(&BotRight(globalSelection));
	
	if (!PtInRect(*localMouse, &selectionRect) )
		{
		theCross = MacGetCursor(crossCursor);
		if (theCross)
			{
			char	oldState;
			
			oldState = HGetState((Handle) theCross);
			HLock((Handle) theCross);
			SetCursor(*theCross);
			HSetState((Handle) theCross, oldState);
			anErr = eActionAlreadyHandled;
			}
			
		// make sure we get mouse-moved events if the mouse moves into the selection rect,
		// so that we can change the cursor
		if (!EmptyRect(&globalSelection))
			{	
			RgnHandle	tempRgn = NewRgn();
			
			RectRgn(tempRgn, &globalSelection);
			DiffRgn(globalRgn, tempRgn, globalRgn);
			DisposeRgn(tempRgn);
			}
		}
	else
		{
		// if we're already in the selection rect, we don't need mouse-moved events as long
		// as we stay there
		RectRgn(globalRgn, &globalSelection);
		}
		
	return(anErr);
	
} // PICTAdjustCursor

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTDragAddFlavors(WindowPtr pWindow, WindowDataPtr pData, DragReference theDragRef)
{
#pragma unused (pWindow)

	static DragSendDataUPP gPICTSendDataProc = NULL;
	OSStatus	anErr = noErr;
	
	if (!gPICTSendDataProc) {
		gPICTSendDataProc = NewDragSendDataUPP(PICTSendDataProc);
	}
	SetDragSendProc(theDragRef, gPICTSendDataProc, pData);
	AddDragItemFlavor(theDragRef, 1, 'PICT', nil, 0, 0);
	
	return(anErr);
	
} // PICTDragAddFlavors

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTGetBalloon(WindowPtr pWindow, WindowDataPtr pData, 
		Point *localMouse, short * returnedBalloonIndex, Rect *returnedRectangle)
{
#pragma unused (pWindow, pData)

	Rect	tempRect = ((PICTDataPtr)pData)->selectionRectangle;
	
	// assume generic content
	*returnedBalloonIndex = iHelpPictContent;
	
	// see if we are within the selection
	OffsetRect(&tempRect, -GetControlValue(pData->hScroll), -GetControlValue(pData->vScroll) );
	SectRect(&tempRect, &pData->contentRect, &tempRect);
	
	if (PtInRect(*localMouse, &tempRect))
		{
		*returnedRectangle		= tempRect;
		*returnedBalloonIndex 	= iHelpPictSelection;
		}
		
	return(noErr);
	
} // PICTGetBalloon

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTPrintPage(WindowPtr pWindow, WindowDataPtr pData,
					Rect * pageRect, long *pageNum)
{
#pragma unused (pWindow)

	OSStatus	anErr = noErr;
	short	pagesWide, pagesHigh;
	Rect	pictureRect = ((PICTDataPtr)pData)->pictureRectangle;
	Point	offset;
	Rect	localPageRect = *pageRect;
	
	// calculate how many physical pages will be required to print this PICT
	if (EqualRect(&pictureRect, &localPageRect))
		pagesWide = pagesHigh = 1;
	else
		{
		pagesWide = (pictureRect.right - pictureRect.left) / (localPageRect.right - localPageRect.left) + 1;
		pagesHigh = (pictureRect.bottom - pictureRect.top) / (localPageRect.bottom - localPageRect.top) + 1;
		}
					
	// compute the offset in # of pages
	offset.h = ((*pageNum - 1) % pagesWide);
	offset.v = ((*pageNum - 1) / pagesWide);

	// compute the pixel offset for this page
	offset.h *= localPageRect.right - localPageRect.left;
	offset.v *= localPageRect.bottom - localPageRect.top;
	
	anErr = DiskPictureDraw( pData, true, &localPageRect, &offset);
	
	// tell it to stop printing when we reach the end
	if (*pageNum >= (pagesWide*pagesHigh))
		*pageNum = -1;
	
	return(anErr);
	
} // PICTPrintPage

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTAdjustMenus(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	OSStatus anErr = noErr;
	
	if (!EmptyRect(&((PICTDataPtr)pData)->selectionRectangle))
		EnableCommand(cCopy);

	if 	(EqualRect(&((PICTDataPtr)pData)->pictureRectangle, &((PICTDataPtr)pData)->selectionRectangle))
		ChangeCommandName(cSelectAll, kMiscStrings, iSelectNoneCommand);
	else
		ChangeCommandName(cSelectAll, kMiscStrings, iSelectAllCommand);
	EnableCommand(cSelectAll);
	
	return(anErr);
	
} // PICTAdjustMenus

// --------------------------------------------------------------------------------------------------------------

static OSStatus PICTCommand(WindowPtr pWindow, WindowDataPtr pData, short commandID, long menuResult)
{
#pragma unused (pWindow, menuResult)

	OSStatus anErr = noErr;
	
	switch (commandID)
		{
		case cCopy:
			anErr = CopyGWorld(pData);
			break;
		
		case cSelectAll:
			// erase the old selection
			DrawSelection(pData, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->patternPhase, false);
			
			if 	(EqualRect(&((PICTDataPtr)pData)->pictureRectangle, &((PICTDataPtr)pData)->selectionRectangle))
				{
				((PICTDataPtr)pData)->selectionRectangle.top = 0;
				((PICTDataPtr)pData)->selectionRectangle.left = 0;
				((PICTDataPtr)pData)->selectionRectangle.bottom = 0;
				((PICTDataPtr)pData)->selectionRectangle.right = 0;
				}
			else
				{
				((PICTDataPtr)pData)->selectionRectangle = ((PICTDataPtr)pData)->pictureRectangle;
				}
				
			// draw the new selection
			DrawSelection(pData, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->patternPhase, true);
			
			DoAdjustCursor(pWindow, nil);
			break;
		}
	
	return(anErr);
	
} // PICTCommand

// --------------------------------------------------------------------------------------------------------------

static long PICTCalculateIdleTime(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	if (!EmptyRect( &((PICTDataPtr)pData)->selectionRectangle))
		return(0);
	else
		return(kMaxWaitTime);
		
} // PICTCalculateIdleTime

// --------------------------------------------------------------------------------------------------------------

static Boolean	PICTFilterEvent(WindowPtr pWindow, WindowDataPtr pData, EventRecord *pEvent)
{
	if 	(
		(!gMachineInfo.amInBackground) &&
		(pEvent->what == nullEvent) &&
		(pWindow == FrontNonFloatingWindow()) &&
		//(EmptyRgn( ((WindowPeek)pWindow)->updateRgn)) &&
		(MOVESELECTION(pEvent->when) )
		)
		{
		// erase the old
		DrawSelection(pData, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->patternPhase, false);
		
		// draw the new, moving onto the next pattern
		DrawSelection(pData, &((PICTDataPtr)pData)->selectionRectangle, &((PICTDataPtr)pData)->patternPhase, true);
		}
		
	return(false);
	
} // PICTFilterEvent

// --------------------------------------------------------------------------------------------------------------

static OSStatus	PICTMakeWindow(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

    Size	availableRAM;
    long	amountInFile;
    OSStatus	anErr = noErr;
    Cursor	arrow;
    FSSpec	fileSpec;

    pData->pUpdateWindow 		= (UpdateWindowProc)		PICTUpdateWindow;
    pData->pGetDocumentRect 	= (GetDocumentRectProc)		PICTGetDocumentRect;
    pData->pCloseWindow 		= (CloseWindowProc)			PICTCloseWindow;
    pData->pContentClick 		= (ContentClickProc)		PICTContentClick;
    pData->pAdjustCursor 		= (AdjustCursorProc)		PICTAdjustCursor;
    pData->pGetBalloon	 		= (GetBalloonProc)			PICTGetBalloon;
    pData->pAdjustMenus 		= (AdjustMenusProc)			PICTAdjustMenus;
    pData->pPrintPage	 		= (PrintPageProc)			PICTPrintPage;
    pData->pCommand		 		= (CommandProc)				PICTCommand;
    pData->pFilterEvent		 	= (FilterEventProc)			PICTFilterEvent;
    pData->pCalculateIdleTime	= (CalculateIdleTimeProc)	PICTCalculateIdleTime;
    pData->pDragAddFlavors		= (DragAddFlavorsProc)		PICTDragAddFlavors;

    pData->hasGrow				= true;
    pData->hScrollAmount		= 10;
    pData->vScrollAmount		= 10;

    ((PICTDataPtr)pData)->isQuickTimeImageFile = ( pData->originalFileType != 'PICT' );

    if( ((PICTDataPtr)pData)->isQuickTimeImageFile )
	{
        GraphicsImportComponent theGraphicsImporter;

        if (pData->dataRefNum != -1)
            {
            FSClose(pData->dataRefNum);
            pData->dataRefNum = -1;
            }

        SetPort( GetWindowPort(pWindow) );
	anErr = FSGetCatalogInfo( &pData->fileRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL );
        anErr = GetGraphicsImporterForFile( &fileSpec, &theGraphicsImporter );
        if( noErr == anErr )
            {
            anErr = GraphicsImportGetNaturalBounds( theGraphicsImporter, &((PICTDataPtr)pData)->pictureRectangle );
            CloseComponent( theGraphicsImporter );
            }
        }
    else
    {
        // Calculate amount of available RAM for cache
        {
            Size grow;
            availableRAM = MaxMem(&grow);
		}

        // Take half of it
        availableRAM >>= 1;

        // if too big, make it just big enough
        GetEOF(pData->dataRefNum, &amountInFile);
        amountInFile -= 512;

        SetWatchCursor();

        if (amountInFile < sizeof(Picture))
        {
            anErr = eErrorWhileDrawing;
        }
        else
        {
            if (availableRAM > amountInFile)
            {
                Handle	theHandle = NewHandle(amountInFile);

                if (theHandle)
                {
                    SetFPos(pData->dataRefNum, fsFromStart, 512);
                    FSRead(pData->dataRefNum, &amountInFile, *theHandle);

                    ((PICTDataPtr)pData)->cacheHandle = (PicHandle)theHandle;
                }
            }
            
            // initialize the rectangle to be valid
            DiskPictureDraw( pData, false, &((PICTDataPtr)pData)->pictureRectangle, nil);

        }
   		SetCursor(GetQDGlobalsArrow(&arrow));
    }
    
    if (anErr == noErr)
	{
        Rect	pictureRect;

        pictureRect = ((PICTDataPtr)pData)->pictureRectangle;

        if ((pData->contentRect.right > pictureRect.right) && (pictureRect.right > 0))
            pData->contentRect.right = pictureRect.right;
        if ((pData->contentRect.bottom > pictureRect.bottom) && (pictureRect.bottom > 0))
            pData->contentRect.bottom = pictureRect.bottom;

        // proportional scrolling
        if( gMachineInfo.haveProxyIcons )
            {
            SetControlViewSize( pData->hScroll, pictureRect.right - pictureRect.left );
            SetControlViewSize( pData->vScroll, pictureRect.bottom - pictureRect.top );
            }
        }

	return(anErr);

} // PICTMakeWindow


// --------------------------------------------------------------------------------------------------------------

OSStatus	PICTPreflightWindow(PreflightPtr pPreflightData)
{
    pPreflightData->wantHScroll			= true;
    pPreflightData->wantVScroll			= true;

    pPreflightData->continueWithOpen 	= true;
    pPreflightData->makeProcPtr 		= PICTMakeWindow;

    pPreflightData->storageSize = sizeof(PICTDataRecord);

    return(noErr);

} // PICTPreflightWindow

// --------------------------------------------------------------------------------------------------------------

void PICTGetFileTypes(OSType * pFileTypes, OSType * pDocumentTypes, short * numTypes)
{
    long version;	

    pFileTypes[*numTypes] 		= 'PICT';
    pDocumentTypes[*numTypes] 	= kPICTWindow;
    (*numTypes)++;

    if( (Gestalt(gestaltQuickTime, &version) == noErr) && (version >= 0x03000000) )
        {
        pFileTypes[*numTypes] 		= 'qtif';
        pDocumentTypes[*numTypes] 	= kPICTWindow;
        (*numTypes)++;
        }
} // PICTGetFileTypes
