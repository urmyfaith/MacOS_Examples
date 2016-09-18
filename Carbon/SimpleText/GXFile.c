/*
	File:		GXFile.c

	Contains:	GX print file support for simple text application.

	Version:	SimpleText 1.4 or later

	Written by:	Tom Dowdy
				DAL = Dave Lyons

	Copyright:	� 1993-1997 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Tom Dowdy

		Other Contact:		Jim Negrette

		Technology:			Macintosh Graphics Group

	Writers:

		(ecs)	Eric Schlegel
		(dmp)	Dave Polaschek
		(ted)	Tom Dowdy
		(TD)	Tom Dowdy

	Change History (most recent first):

	$Log: GXFile.c,v $
	Revision 1.1  2005/02/16 03:33:19  mig
	[3943574] add SimpleText project from ~cvs/repository/pure and don't install binary anymore
	
	Revision 1.9  2003/12/18 01:25:33  ericsc
	[3513557]  Use FrontNonFloatingWindow instead of FrontWindow.
	
	Revision 1.8  2000/07/01 02:23:49  mig
	some fixes for framework-style includes
	
	Revision 1.7  1999/05/26 17:03:12  wilkes
	Removed all vestiges of Zones...
	
	Revision 1.6  1999/05/20 22:41:21  christ
	CountMItems -> CountMenuItems
	
	Revision 1.5  1998/10/12 18:50:52  danp
	*** empty log message ***
	
	Revision 1.4  1998/09/15 18:59:43  jiarocci
	SimpleText now builds with -DTARGET_CARBON=1. Still needs further cleanup.
	
	Revision 1.3  1998/03/30 22:12:26  mkellner
	Update to use new GetQDxxxx macros for qd.globals

	Revision 1.2  1998/03/20 03:19:54  mkellner
	change qd.thePort to FrontWindow()
	add SysEnvirons
	
	Revision 1.1.1.1  1998/03/18 22:56:09  ivory
	Initial checkin of SimpleText.
	
		
		5     8/20/97 4:23 PM Tom Dowdy
		1674136: adjust cursor after select all
		
		4     8/11/97 3:05 PM Tom Dowdy
		rolling in nav services
		
		3     7/29/97 2:07 PM Tom Dowdy
		Removed all of the old and boring refs
		
		2     7/29/97 1:52 PM Tom Dowdy
		Various new interface fixes
		
		1     7/28/97 11:18 AM Duane Byram
		first added to Source Safe project

		<19>	 6/17/97	ted		Getting rid of some compiler warnings
		<18>	 5/14/97	ted		More drag clipping bug fixes
		<17>	 5/14/97	ted		More drag clipping bug fixes
		<16>	11/26/96	ecs		support GXGraphics extension; smarter cursor adjustment
		<15>	  9/9/96	dmp		staticfy local functions to eliminate warnings in MWC.
		<14>	  7/9/96	ted		adding pragma unused
		<13>	 5/31/96	ted		AddImage -> SetImage
		<12>	 5/31/96	ted		adding PPC, FAT, and NuKernel builds
		<11>	 4/11/96	ted		cwindowptr->windowptr
		<10>	  2/8/96	ted		CountMenuItems->CountMItems (??)
		 <9>	 1/12/96	ted		need to cull style and ink also
		 <8>	 1/12/96	ted		dealing with override transforms on cullshape
		 <7>	 11/2/95	ted		BlockMoveData
		 <6>	 10/5/95	ted		fixing hilight for rotated shapes
		 <5>	 10/2/95	TD		adding in other selections for editing
		 <4>	 9/11/95	TD		fixing zoom in icons
		 <3>	  9/8/95	TD		started annotation
		 <2>	 8/22/95	TD		moving enum to simpletext.h
		 <1>	 8/21/95	TD		First checked in.
		<20>	 12/8/94	DAL		Radar #1204706. Now handles updates behind the go-to-page
									dialog.

*/

#include "MacIncludes.h"

#include "GXFile.h"

#pragma segment GXFile

// --------------------------------------------------------------------------------------------------------------
// PRIVATE TYPEDEFS AND DECLARES
// --------------------------------------------------------------------------------------------------------------
// items to the left of the horizontal scroll bar
#define kScrollAreaWidth	120
#define kPageControlsWidth	32
#define kZoomControlsWidth	26
#define kToolControlWidth	16

// items in the pop up page selection window
#define kPageSliderHeight	10
#define kPageSliderMargins	7
#define kPageThumbEdge		4
#define kPageThumbHeight	(kPageSliderHeight + kPageThumbEdge*2)
#define kPageThumbWidth		(kPageThumbHeight / 2)
#define kPageThumbMargins	3
#define kProxyHeight		150
#define kProxyWidth			150
#define kPopUpWindowHeightSmall	(kPageThumbHeight + kPageSliderMargins*2 + kPageThumbEdge*2)
#define kPopUpWindowHeightLarge	(kPageThumbHeight + kPageSliderMargins*3 + kPageThumbEdge*2 + kProxyHeight)

#define kMinGXDocSize		kMinDocSize

// PICT proxies for the pages
#define kProxyBaseID		(gxPrintingTagID)
#define kProxyType			'prxy'

// flattened GX shapes for annotations
#define kAnnotationBaseID	(gxPrintingTagID)
#define kAnnotationType		'anot'

// table of pop up menu items and corosponding zoom factors
typedef struct
	{
	short	menuItem;
	Fixed	zoomFactor;
	} ZoomTableEntry;
	
typedef struct
	{
	short	theFont;
	short	theSize;
	} TextState;
	
typedef struct  
	{
	gxSpoolBlock	spool;
	long			reference;
	long			position;
	long			size;
	void			*data;
	void			*userField;
	} userSpool;

#define LONGALIGN(n)		(((n) + 3) & ~3L)
#define kAtomHeaderSize		(sizeof(Size) + sizeof(OSType))
#define ABS(n)				(((n) < 0) ? -(n) : (n))

#if GENERATINGCFM
	extern pascal OSErr SetImageDescriptionExtension(ImageDescriptionHandle desc, Handle extension, long idType);
#endif

// --------------------------------------------------------------------------------------------------------------
// FORWARD DECLARES
// --------------------------------------------------------------------------------------------------------------
OSErr	GXGetDocumentRect(WindowPtr pWindow, WindowDataPtr pData, 
			LongRect * documentRectangle, Boolean forGrow);
OSErr	GXCommand(WindowPtr pWindow, WindowDataPtr pData, short commandID, long menuResult);

// --------------------------------------------------------------------------------------------------------------
// LOCAL GLOBALS
// --------------------------------------------------------------------------------------------------------------
static ZoomTableEntry gZoomTable[] = {
								{i50, 	0x8000},
								{i100,	ff(1)},
								{i112,	0x00011EB8},
								{i150, 	0x00018000},
								{i200,	ff(2)},
								{i400,	ff(4)},
								{0,0}};


// --------------------------------------------------------------------------------------------------------------
// PRIVATE ROUTINES
// --------------------------------------------------------------------------------------------------------------

static void GetCurrentPageAndPaper(WindowDataPtr pData, gxRectangle *pPageSize, gxRectangle *pPaperSize)
{
	GXGetFormatDimensions( ((GXDataPtr)pData)->currentPageFormat, pPageSize, pPaperSize);	
	if (((GXDataPtr)pData)->dontShowMargins)
		*pPaperSize = *pPageSize;
		
} // GetCurrentPageAndPaper

// ------------------------------------------------------------------------------------------------------
static void InitColorMatrix(Fixed m[5][4])
{
	register Fixed *x;
	register short i;

	x = &m[0][0];
	for(i = 19; i>=0; i--)
		*x++ = 0;
	m[0][0] = m[1][1] = m[2][2] = m[3][3] = fixed1;		   /* Identity matrix, for cleanliness */
	
} // InitColorMatrix

// --------------------------------------------------------------------------------------------------------------
static void RectangleToRect(const gxRectangle* gxr, Rect* qdr)
{
	qdr->left = FixedRound(gxr->left);
	qdr->top = FixedRound(gxr->top);
	qdr->right = FixedRound(gxr->right);
	qdr->bottom = FixedRound(gxr->bottom);
	
} // RectangleToRect

// --------------------------------------------------------------------------------------------------------------
#define allocationIncrement   1024     /* the storage handle is grown by this amount */

static long HandleSpoolProc(gxSpoolCommand command,  userSpool *block)
{
	gxGraphicsError	anErr = noErr;
	
   	switch (command)
   		{
      	case gxOpenReadSpool:
         	block->size = 0;
         	block->position = 0;
      		break;
	  
      	case gxOpenWriteSpool:
         	block->data = NewHandle(allocationIncrement);
         	block->size = allocationIncrement;
         	block->position = 0;
			anErr = MemError();
      		break;
      
      	case gxReadSpool:
         	BlockMoveData((*(char **) block->data) + block->position, block->spool.buffer, block->spool.count);
         	block->position += block->spool.count;
      		break;

      	case gxWriteSpool:
      		{  
			register long oldPosition;

        	oldPosition = block->position;
        	block->position += block->spool.count;

        	/* make sure there is at least enough room for one buffer size past current pointer */
        	if (block->position + block->spool.bufferSize > block->size)      
         		{
            	block->size += block->spool.bufferSize;
            	HUnlock((Handle) block->data);
            	SetHandleSize((Handle) block->data, block->size);
				anErr = MemError();
            	HLock((Handle) block->data);
         		}
			if (anErr == noErr)
         		BlockMoveData(block->spool.buffer, (*(char **) block->data + oldPosition), block->spool.count);
      		}
     		 break;
      
      	case gxCloseSpool:
         	SetHandleSize((Handle) block->data, block->position);
      		break;
   		}
		
   return anErr;
   
} // HandleSpoolProc

#if GENERATINGCFM
	static RoutineDescriptor gHandleSpoolProcRD = BUILD_ROUTINE_DESCRIPTOR(uppgxSpoolProcInfo, HandleSpoolProc);
	static gxSpoolUPP gHandleSpoolProc = &gHandleSpoolProcRD;
#else
	static gxSpoolUPP gHandleSpoolProc = NewgxSpoolProc(HandleSpoolProc);
#endif

// --------------------------------------------------------------------------------------------------------------
static long* AppendAtom(long stream[], Size size, OSType tag, const void* data)
{

	*stream++	= size + kAtomHeaderSize;
	*stream++	= tag;
	BlockMoveData(data, (Ptr)stream, size);

	return (long*)((char*)stream + size);
	
} // AppendAtom

// --------------------------------------------------------------------------------------------------------------
static Handle CreateQDGXStream(gxShape source, PicHandle proxie, Boolean forPrintingOnly, Boolean eraseBackground)
/*
 *	See the comment on DecompressShape for an explaination of the parameters.
 *	This routine is used by both DecompressShape for embedding shapes in PICTs,
 *	and AddQDGXRecorderFrame for making gx movies.
*/
{
	#define			gxForPrintingOnlyAtom	'fpto'
	#define			gxEraseBackgroundAtom	'erbg'

	long			atomCount, shapeSize, proxieSize, dataSize, fontListSize;
	Handle			dataHdl, shapeHdl;
	gxFlatFontList*	fontList;
	gxTag			fontListTag;
  	userSpool 		block;

	block.spool.spoolProcedure = gHandleSpoolProc;
	block.spool.buffer = nil;
	block.spool.bufferSize = 0;
	GXFlattenShape(source, gxFontListFlatten | gxFontGlyphsFlatten | gxFontVariationsFlatten, &block.spool);
	shapeHdl = (Handle) block.data;
	if (shapeHdl == nil)
		return nil;

	if (proxie)
		{	
		atomCount = 2;
		proxieSize = LONGALIGN(GetHandleSize((Handle)proxie));
		}
	else
		{	
		atomCount = 1;
		proxieSize = 0;
		}
	shapeSize = LONGALIGN(GetHandleSize(shapeHdl));

	if (forPrintingOnly)
		++atomCount;
	if (eraseBackground)
		++atomCount;

	fontListSize = 0;
	fontList = nil;
	GXIgnoreGraphicsWarning(count_out_of_range);
	if (GXGetShapeTags(source, gxFlatFontListItemTag, 1, 1, &fontListTag) > 0)
		{	
		fontListSize = GXGetTag(fontListTag, nil, nil);
		if (fontListSize > 0)
			{	
			fontList = (gxFlatFontList*)NewPtr(fontListSize);
			if (fontList != nil)
				{	
				GXGetTag(fontListTag, nil, fontList);
				fontListSize = LONGALIGN(fontListSize);
				++atomCount;
				}
			else
				fontListSize = 0;
			}
		}
	GXPopGraphicsWarning();		// count_out_of_range

	dataSize = atomCount * kAtomHeaderSize + shapeSize + proxieSize + fontListSize + sizeof(long);
	dataHdl = NewHandle(dataSize);
	if (dataHdl == nil)
		{	
		DisposeHandle(shapeHdl);
		if (fontList)
			DisposePtr((Ptr)fontList);
		return nil;
		}
	
	{	
		long* p = (long*)*dataHdl;

		if (forPrintingOnly)
			p = AppendAtom(p, 0, gxForPrintingOnlyAtom, nil);
		if (eraseBackground)
			p = AppendAtom(p, 0, gxEraseBackgroundAtom, nil);
		if (proxie)
			p = AppendAtom(p, proxieSize, 'PICT', *proxie);
		if (fontList)
			p = AppendAtom(p, fontListSize, gxFlatFontListItemTag, fontList);
		p = AppendAtom(p, shapeSize, 'qdgx', *shapeHdl);
		*p++ = 0;		// end of the atom-list

		DisposeHandle(shapeHdl);
		if (fontList)
			DisposePtr((Ptr)fontList);
	}
	return dataHdl;

} // CreateQDGXStream

// --------------------------------------------------------------------------------------------------------------
static PicHandle DecompressShape(gxShape theShape, PicHandle proxie, Boolean forPrintingOnly, Boolean eraseBackground)
/*
 *	This guy returns a Quickdraw picture containing an embedded shape, and a proxie
 *	of the shape, if proxie is not nil. This is called by ShapeToScrap and DragAndDropShape.
 *
 *	theShape			� the shape you want to embedd in a PICT
 *	proxie			� a PICT to be drawn if theShape cannot be drawn (optional but recommended)
 *	forPrintingOnly		� if TRUE, then the decompressor will always look for the proxie
 *					and theShape will only be used when printing. Use this setting if
 *					theShape might be too large or too slow when drawn from other apps.
 *					� If FALSE, then the decompressor will draw theShape unless it
 *					gets an error, in which case it will look for a proxie.
 *	eraseBackground	� if TRUE, the decompressor will always erase the background to WHITE
 *					before drawing the shape. This is slower, but needed if the shape does not
 *					fill its bounding rectangle.
 *					� if FALSE, the decompressor will just draw the shape. Use this setting
 *					if the shape entirely fills its bounding rectangle.
 *
 *	The shape [and proxie] is embedded by constructing a stream of atoms. Each atom begins
 *	with a size (long) and a type (OSType) and then the data for that type. After the last atom,
 *	there is a trailing zero (long) to mark the end of the stream. For embedded shapes, the type
 *	is 'qdgx', and for the proxie the type is 'PICT'. Note that the size fields are rounded up to
 *	a multiple of 4. Finally, to alert QuickTime that the data is in this parsable form with a
 *	possible PICT proxie, we add a 'prxy' extension to the ImageDescriptionHandle.
 *
 *	Picture of this form will draw the embedded shape when an application calls DrawPicture
 *	if GX is around, and if not, the proxie will be drawn. When printed, the shape or the proxie
 *	will be printed. This is meant to replace the PicComment described in GX 1.0 for embedding
 *	shapes in pictures.
 *
 *	If you want to include a flatFontList tag, be sure that theShape is a picture, otherwise GX will
 *	not return the tag after GXFlattenShape. The flatFontList tag makes certain printing conditions
 *	more efficient (i.e. font downloading to postscript printers).
 *
 *	Your shape must not contain a gxQuickDrawPictTag, meaning it contains embedded QD data, becuase
 *	this will potentially crash when it tries to print. To fix that, DecompressShape looks for occurrences
 *	of the tag, and converts them to real gx data by calling GXSetShapeType(shape, gxPictureType).
*/
{
	PicHandle				thePicture;
	ImageDescriptionHandle	descHdl;
	ImageDescriptionPtr		descPtr;
	Handle					dataHdl;
	CGrafPtr 				thePort;
	if (!gMachineInfo.haveQuickTime)
		return nil;

	/*
	 *	Move the shape's topLeft to 0,0 so that it draws neatly inside the picture frame.
	 *	Note that the qdgx movie library does not move the shape, since the shape may not
	 *	take up the whole frame.
	*/
	{	
		gxRectangle	bounds;

		GXGetShapeLocalBounds(theShape, &bounds);
		if (bounds.left || bounds.top)
			GXMoveShape(theShape, -bounds.left, -bounds.top);
		dataHdl = CreateQDGXStream(theShape, proxie, forPrintingOnly, eraseBackground);
		if (bounds.left || bounds.top)
			GXMoveShape(theShape, bounds.left, bounds.top);
	}
	if (dataHdl == nil)
		return nil;

	descHdl = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription));
	if (descHdl)
		{	
		Rect			shortBounds;
		gxRectangle	bounds;

		GXGetShapeLocalBounds(theShape, &bounds);
		RectangleToRect(&bounds, &shortBounds);
		OffsetRect(&shortBounds, -shortBounds.left, -shortBounds.top);	// set the topLeft of the src to 0,0
		thePicture = OpenPicture(&shortBounds);

		descPtr = *descHdl;
		descPtr->idSize = sizeof(ImageDescription);
		descPtr->cType = 'qdgx';
		descPtr->vendor = 'appl';
		descPtr->temporalQuality = codecLosslessQuality;
		descPtr->width = shortBounds.right;
		descPtr->height = shortBounds.bottom;
		descPtr->hRes = descPtr->vRes = ff(72);
		descPtr->dataSize = GetHandleSize(dataHdl);
		descPtr->frameCount = 1;
		descPtr->depth = 32;
		descPtr->clutID = -1;

		//	If there is a PICT proxie, add an image extension to tell QuickTime, in case GX is not around.
		if (proxie)
			{	
			Handle prxyVersionHdl = NewHandle(sizeof(long));

			if (prxyVersionHdl != nil)
				{	
				*(long*)*prxyVersionHdl = 0;		// version number for 'prxy' extension
				#if GENERATINGCFM
					SetImageDescriptionExtension(descHdl, prxyVersionHdl, 'prxy');
				#else
					AddImageDescriptionExtension(descHdl, prxyVersionHdl, 'prxy');
				#endif
				}
			}

		HLock(dataHdl);
		thePort = GetQDGlobalsThePort();
		DecompressImage(*dataHdl, descHdl, GetPortPixMap(thePort),
				&shortBounds, &shortBounds, srcCopy, nil);
		DisposeHandle((Handle)descHdl);
		ClosePicture();
		}
	else
		thePicture = nil;

	DisposeHandle(dataHdl);

	return thePicture;
	
} // DecompressShape

// --------------------------------------------------------------------------------------------------------------
static PicHandle ShapeToPICT(gxShape source)
/*
 *	This guy returns a Quickdraw picture containing a 1-bit bitmap of the shape.
 *	This is used by ShapeToScrap to create a proxie when calling DecompressShape.
 *	If you want to make the proxie prettier (and larger), change the bitmap to 8-bit.
 *	However, if you're using this in conjunction with DecompressShape to place a
 *	gxShape on the clipboard, 1-bit should be enough, since the actual shape will be
 *	drawn, rather than the proxie (unless forPrintingOnly is true).
*/
{
	gxRectangle		bounds;
	gxShape			bitShape;
	gxBitmap		bitmap;
	PicHandle		thePicture;
	Rect			shortBounds;

	/*
	 *	GetShapeLocalBounds doesn't accurately report the bounds of a gxQuickDrawPictTag.
	 *	but we should have none of them at this point anyway.
	*/
	GXGetShapeLocalBounds(source, &bounds);
	RectangleToRect(&bounds, &shortBounds);
	OffsetRect(&shortBounds, -shortBounds.left, -shortBounds.top);

	bitmap.width		= shortBounds.right;
	bitmap.height		= shortBounds.bottom;
	bitmap.rowBytes		= bitmap.width + 31 >> 5 << 2;
	bitmap.pixelSize	= 1;
	bitmap.space		= gxIndexedSpace;
	bitmap.set			= nil;
	bitmap.profile		= nil;
	bitmap.image		= NewPtrClear(bitmap.rowBytes * bitmap.height);
	if (bitmap.image == nil)
		return nil;

	bitShape = GXNewBitmap(&bitmap, nil);
	if (bitShape != nil)
		{	
		gxViewGroup group	= GXNewViewGroup();
		gxViewDevice device	= GXNewViewDevice(group, bitShape);
		gxViewPort port		= GXNewViewPort(group);
		gxTransform trans	= GXCloneTransform(GXGetShapeTransform(source));

		GXSetShapeAttributes(source, GXGetShapeAttributes(source) | gxMapTransformShape);
		GXMoveShape(source, -bounds.left, -bounds.top);
		GXSetViewPortDither(port, 4);
		GXSetShapeViewPorts(source, 1, &port);
		GXDrawShape(source);
		GXSetShapeTransform(source, trans);
		GXDisposeTransform(trans);
		
		GXDisposeViewGroup(group);	/* this disposes the gxViewPort and gxViewDevice */
		GXDisposeShape(bitShape);
		}

	{	
		GrafPtr	thePort;
		BitMap	srcBits;
	
		GetPort(&thePort);
		srcBits.baseAddr = bitmap.image;
		srcBits.rowBytes = bitmap.rowBytes;
		srcBits.bounds = shortBounds;

		thePicture = OpenPicture(&shortBounds);
		CopyBits(&srcBits, &thePort->portBits, &shortBounds, &shortBounds, srcOr, nil);
		ClosePicture();
	}
	
	DisposePtr((Ptr)bitmap.image);
	
	return thePicture;
	
} // ShapeToPICT



// --------------------------------------------------------------------------------------------------------------

static void GetRidOfAnyQDShapeTags(gxShape shape)
{
	gxShapeType shapeType = GXGetShapeType(shape);

	if (shapeType == gxPictureType)
		{	
		long		index, count;
		gxShape		contentShape;
	
		count = GXGetPicture(shape, nil, nil, nil, nil);
		for (index = 0; index < count; index++)
			{
			GXGetPictureParts(shape, index+1, 1, &contentShape, nil, nil, nil);
			GetRidOfAnyQDShapeTags(contentShape);
			}
		}
	else 
		{
		if ( (shapeType == gxRectangleType) && (GXGetShapeTags(shape, gxQuickDrawPictTag, 1, gxSelectToEnd, nil) > 0) )
			GXSetShapeType(shape, gxPictureType);
		}
			
} // GetRidOfAnyQDShapeTags

// --------------------------------------------------------------------------------------------------------------

static void ShapeToScrap(gxShape source)
/*
 *	This guy puts a Quickdraw picture on the clipboard containing an embedded shape
 *	and, if addProxie is true, a 1-bit bitmap of the shape. Call this in response to
 *	the user choosing "Copy" or "Cut" from the Edit menu. See comment for DecompressShape
 *	to explain forPrintingOnly.
*/
{
	PicHandle	picture, proxy;
	
	proxy = ShapeToPICT(source);
	picture = DecompressShape(source, proxy, false, true);
	if (proxy)
		KillPicture(proxy);

	if (picture)
		{	
		HLock((Handle)picture);
		ZeroScrap();
		PutScrap(GetHandleSize((Handle)picture), 'PICT', (Ptr)*picture);
		KillPicture(picture);
		}
		
} // ShapeToScrap

// --------------------------------------------------------------------------------------------------------------

static void CullShape(gxShape shape, gxShape addToThis, gxRectangle *pCullRect, gxStyle cullStyle, gxInk cullInk, gxTransform cullTransform)
/*
	Add to "addToThis" the "shape", only if the shape intersects the pCullRect.
*/
{
	gxShapeType shapeType = GXGetShapeType(shape);

	if (shapeType == gxPictureType)
		{	
		long		index, count;
		gxShape		contentShape;
	
		count = GXGetPicture(shape, nil, nil, nil, nil);
		for (index = 0; index < count; index++)
			{
			GXGetPictureParts(shape, index+1, 1, &contentShape, &cullStyle, &cullInk, &cullTransform);
			CullShape(contentShape, addToThis, pCullRect, cullStyle, cullInk, cullTransform);
			}
		}
	else 
		{
		gxRectangle	bounds;
		
		GXGetShapeLocalBounds(shape, &bounds);
		
		if (IsSomewhereInRectangle(pCullRect, &bounds))
			{
			if ( (shapeType == gxRectangleType) && (GXGetShapeTags(shape, gxQuickDrawPictTag, 1, gxSelectToEnd, nil) > 0) )
				{
				// convert shape and add -- but only if it goes okay
				GXSetShapeType(shape, gxPictureType);
				if (GXGetShapeType(shape) == gxPictureType)
					CullShape(shape, addToThis, pCullRect, cullStyle, cullInk, cullTransform);
				}
			else
				{
				GXSetPictureParts(addToThis, 0, 0, 1, &shape, &cullStyle, &cullInk, &cullTransform);
				}
			}
		}
		
} // CullShape

// --------------------------------------------------------------------------------------------------------------
static gxShape CullPicture(gxShape pictureShape, gxRectangle * pCullRect)
/*
	Returns a new shape that is all of the shapes inside of pictureShape
	that intersect pCullRect.
*/
{
	gxShape		newPicture = GXNewShape(gxPictureType);
	gxShape 	clipShape = GXNewRectangle(pCullRect);
	gxMapping	mapping;
	gxRectangle	clipRect;
	
	// new shape as same mapping as old one
	GXGetTransformMapping(GXGetShapeTransform(pictureShape), &mapping);
	GXSetShapeMapping(newPicture, &mapping);
	
	// clip also has the mapping, but inverted so that it's the right space
	InvertMapping(&mapping, &mapping);
	GXMapShape(clipShape, &mapping);
	GXGetShapeLocalBounds(clipShape, &clipRect);
	
	// clip to the selection
	GXSetShapeClip(newPicture, clipShape);
	GXDisposeShape(clipShape);

	// add all shapes that intersect the clip area
	CullShape(pictureShape, newPicture, &clipRect, nil, nil, nil);
		
	// new shape is zero based
	GXMoveShape(newPicture, -pCullRect->left, -pCullRect->top);
	
	return(newPicture);
	
} // CullPicture

// --------------------------------------------------------------------------------------------------------------
static gxShape GetSelectedShape(WindowDataPtr pData)
/*
	Returns a shape that represents all shapes on the current page
	that are contained by the current selection rectangle.
*/
{
	gxRectangle		cullRect;
	gxShape			cullShape;
	gxPoint			offset;
	Fixed			zoomFactor = ((GXDataPtr)pData)->zoomFactor;
	
	// calculate the actual coodinate space, removing margins
	{
	gxRectangle		pageSize, paperSize;
	
	GetCurrentPageAndPaper(pData, &pageSize, &paperSize);
	offset.y = FixedMultiply(-paperSize.top, zoomFactor);
	offset.x = FixedMultiply(-paperSize.left, zoomFactor);
	}
	
	// calculate the actual coodinates to copy
	cullRect.top	= FixedDivide(ff(((GXDataPtr)pData)->selectionRectangle.top) - offset.y, zoomFactor);
	cullRect.left	= FixedDivide(ff(((GXDataPtr)pData)->selectionRectangle.left) - offset.x, zoomFactor);
	cullRect.bottom	= FixedDivide(ff(((GXDataPtr)pData)->selectionRectangle.bottom) - offset.y, zoomFactor);
	cullRect.right	= FixedDivide(ff(((GXDataPtr)pData)->selectionRectangle.right) - offset.x, zoomFactor);
	
	// chop the data
	cullShape = CullPicture(((GXDataPtr)pData)->currentPageShape, &cullRect);
	
	return(cullShape);
	
} // GetSelectedShape

// --------------------------------------------------------------------------------------------------------------
static pascal OSErr GXSendDataProc(FlavorType theType, void *dragSendRefCon,
								ItemReference theItem, DragReference theDrag)
/*
 *	The ItemReference is the gxShape to be sent. The dragSendRefCon is ignored.
*/
{
#pragma unused (theItem)

	OSErr	result = noErr;
	gxShape	shape = ((GXDataPtr)dragSendRefCon)->tempDragShape;
	
	// haven't made clipped version yet?
	if (shape == nil)
		{
		shape = GetSelectedShape((WindowDataPtr) dragSendRefCon);
		((GXDataPtr)dragSendRefCon)->tempDragShape = shape;
		}
		
	switch (theType) 
		{
		case 'qdgx':
			{	
			Handle 		flat;
  			userSpool 	block;

			block.spool.spoolProcedure = gHandleSpoolProc;
			block.spool.buffer = nil;
			block.spool.bufferSize = 0;
			GXFlattenShape(shape, gxFontListFlatten | gxFontGlyphsFlatten | gxFontVariationsFlatten, &block.spool);
			flat = (Handle) block.data;
	
			if (flat)
				{	
				HLock(flat);
				result = SetDragItemFlavorData(theDrag, 1, 'qdgx', *flat, GetHandleSize(flat), 0);
				DisposeHandle(flat);
				}
			}
			break;
			
		case 'PICT':
			{	
			PicHandle proxie = ShapeToPICT(shape);
			PicHandle pict = DecompressShape(shape, proxie, false, true);
	
			if (proxie)
				KillPicture(proxie);
			if (pict)
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
	
} // GXSendDataProc

#if GENERATINGCFM
	static RoutineDescriptor gGXSendDataProcRD = BUILD_ROUTINE_DESCRIPTOR(uppDragSendDataProcInfo, GXSendDataProc);
	static DragSendDataUPP gGXSendDataProc = &gGXSendDataProcRD;
#else
	static DragSendDataUPP gGXSendDataProc = NewDragSendDataUPP(GXSendDataProc);
#endif

// --------------------------------------------------------------------------------------------------------------
static void ClearCurrentSelection(GXDataPtr pData)
{
	pData->currentShapeIndex = 0;
	pData->currentShapeStart = 0;
	pData->currentShapeEnd = 0;
	if (pData->currentSelectionShape)
		{
		GXDisposeShape(pData->currentSelectionShape);
		pData->currentSelectionShape = nil;
		}
		
} // ClearCurrentSelection

// --------------------------------------------------------------------------------------------------------------
static OSErr	GetCurrentPage(GXDataPtr pData, Boolean disposeOfSelection)
/*
	Disposes of previously loaded page information, and loads the
	page information for the current page number.
*/
{
	OSErr		anErr;
	LongRect	oldRect, newRect;
	
	if (pData->numberOfPages != 0)
		{
		// get rid of any previous format
		if (pData->currentPageFormat)
			{
			GXDisposeFormat(pData->currentPageFormat);
			pData->currentPageFormat = nil;
			}
			
		// get rid of any previous shape
		if (pData->currentPageShape)
			{
			GXDisposeShape(pData->currentPageShape);
			pData->currentPageShape = nil;
			}
			
		// get rid of selection, if desired
		if (disposeOfSelection)
			ClearCurrentSelection(pData);

		GXGetDocumentRect((WindowPtr)pData, (WindowDataPtr)pData, &oldRect, false);
		
		GXReadPrintFilePage(pData->thePrintFile, 
			pData->currentPage, 
			1, &pData->childViewPort, 
			&pData->currentPageFormat, &pData->currentPageShape);
		}
		
	anErr = GXGetJobError(pData->w.hPrint);
	
	if (anErr == noErr)
		{
		GXGetDocumentRect((WindowPtr)pData,(WindowDataPtr) pData, &newRect, false);
		
		if 	(
			(oldRect.left != newRect.left) ||
			(oldRect.top != newRect.top) ||
			(oldRect.right != newRect.right) ||
			(oldRect.bottom != newRect.bottom)
			)
			{
			// if the resulting page is < the current window size, we need to resize,
			long newWidth 	= newRect.right - newRect.left + kScrollBarSize;
			long newHeight 	= newRect.bottom - newRect.top + kScrollBarSize;
			long oldWidth 	= pData->w.theWindow.port.portRect.right - pData->w.theWindow.port.portRect.left;
			long oldHeight 	= pData->w.theWindow.port.portRect.bottom - pData->w.theWindow.port.portRect.top;
			
			// but don't let it get too small!
			if (newWidth < pData->w.minHSize)
				newWidth = pData->w.minHSize;
			if (newHeight < kMinGXDocSize)
				newHeight = kMinGXDocSize;
				
			if 	(
				(newWidth < oldWidth) ||
				(newHeight < oldHeight)
				)
				{
				if (newWidth > oldWidth)
					newWidth = oldWidth;
				if (newHeight > oldHeight)
					newHeight = oldHeight;
					
				SizeWindow((WindowPtr)pData, newWidth, newHeight, false);
				}
				
			// and in any case, the scroll bars should update
			AdjustScrollBars((WindowPtr)pData, true, true, nil);
			}
		}
		
	return(anErr);
	
} // GetCurrentPage

// --------------------------------------------------------------------------------------------------------------
#define charBullet	'�'

static void GetIntlTokenChar(short whichToken, short whichScript, char *bulletString)
//
// GetIntlTokenChar
//
// This routine gets a character out of the itl4 given a script and token�
//
{
	Handle	itl4H;
	long	offset, len;

	// default the value
	bulletString[0] = 1;
	bulletString[1] = charBullet;
	
	// Look up the untoken table -- bail if we can�t get it
	GetIntlResourceTable(whichScript, iuUnTokenTable, &itl4H, &offset, &len);
	if (itl4H && (offset > 0) && (len >= 0))
	{
		char *sp = (*itl4H + offset);				// Point to start of untoken table
		if (whichToken <= ((short *)sp)[1])			// Check if token has valid index
		{
			sp += ((short *)sp)[2+whichToken];		// Add the string offset and voli�!
			BlockMoveData(sp, bulletString, sp[0]+1);
		}
	}
	
} // GetIntlTokenChar

// --------------------------------------------------------------------------------------------------------------
// This code is required to change pop up menus to a different font size.  It would be
// better to use the pop up control, but it doesn't allow multiple items to be marked.

#define SysFontSize	0xBA8
#define SysFontFam	0xBA6
#define CurFMInput	0x988

static void DoUseWFont(TextState *savedInfo, WindowPtr owner,  Boolean saveIt)
/*************************************************************
	DoUseWFont		- Sets the font mgr low mem globals so
						we can have Geneva 9 popups

		savedInfo	- Fills it in if saveIt = true, else
						it sets the port to those values
		owner		- Where to get the original values
		saveIt		- true for save
**************************************************************/
{
	TextState		myState,
					*theState;
	short			aFont;

	theState = savedInfo;

	if (saveIt) 
		{
		savedInfo->theFont = GetSysFont();	// save low memory globals
		savedInfo->theSize = *((short *) SysFontSize);

		myState.theFont = GetWindowPort(owner)->txFont;
		myState.theSize = GetWindowPort(owner)->txSize;
		theState = &myState;

		// if we stuff systemFont, it will screw up Script Mgr
		if (GetWindowPort(owner)->txFont == systemFont)
			goto dosizestuff;
		}

	// if we stuff applFont, this will also screw up Script Mgr
	// instead we get the actual font
	aFont = theState->theFont;
	if (saveIt)
		if (GetWindowPort(owner)->txFont == applFont)
			aFont = GetAppFont();
	*((short *) SysFontFam) = aFont;					// set/restore low memory globals

dosizestuff:
	*((short *) SysFontSize) = theState->theSize;

	*((long *) CurFMInput) = 0xFFFFFFFF;
	
} // DoUseWFont


// --------------------------------------------------------------------------------------------------------------

static void SetZoom(WindowPtr pWindow, WindowDataPtr pData, Fixed newZoom)
/*
	Sets the new zoom factor for the window, causing an update for
	the window if required.
*/
{
	Fixed	scaleFactor;
	
	// pin to max/min zoom factors
	if (newZoom > ff(32))
		newZoom = ff(32);
	if (newZoom < 0x0800)
		newZoom = 0x0800;
	
	scaleFactor = FixedDivide(newZoom, ((GXDataPtr)pData)->zoomFactor);
	
	if (scaleFactor != ff(1))
		{
		gxPoint		centerPoint;
		GrafPtr		pPort = (GrafPtr)GetWindowPort(pWindow);
		
		// zoom about the window center
		centerPoint.x = ff(pPort->portRect.left + (RectWidth(pPort->portRect) >> 1));
		centerPoint.y = ff(pPort->portRect.top + (RectHeight(pPort->portRect) >> 1));
		
		// new zoom active
		((GXDataPtr)pData)->zoomFactor = newZoom;
		
		// force update and recalc the size of window
		InvalRect(&pPort->portRect);
		AdjustScrollBars(pWindow, true, true, nil);

 		// scale scroll values
		SetControlValue(pData->hScroll, FixedToInt( FixedDivide(centerPoint.x, scaleFactor) + FixedMultiply(ff(GetControlValue(pData->hScroll)), scaleFactor) ) );
		SetControlValue(pData->vScroll, FixedToInt( FixedDivide(centerPoint.y, scaleFactor) + FixedMultiply(ff(GetControlValue(pData->vScroll)), scaleFactor) ) );
		
		// zoom the selection
		((GXDataPtr)pData)->selectionRectangle.left 	= FixedToInt (FixedMultiply(ff(((GXDataPtr)pData)->selectionRectangle.left), scaleFactor) );
		((GXDataPtr)pData)->selectionRectangle.top 		= FixedToInt (FixedMultiply(ff(((GXDataPtr)pData)->selectionRectangle.top), scaleFactor) );
		((GXDataPtr)pData)->selectionRectangle.right 	= FixedToInt (FixedMultiply(ff(((GXDataPtr)pData)->selectionRectangle.right), scaleFactor) );
		((GXDataPtr)pData)->selectionRectangle.bottom 	= FixedToInt (FixedMultiply(ff(((GXDataPtr)pData)->selectionRectangle.bottom), scaleFactor) );
		}
	
} // SetZoom

// --------------------------------------------------------------------------------------------------------------

static void SetShapeGreyColorLevel(gxShape thisShape, unsigned long greyLevel)
{
	gxColor thisColor;

	thisColor.space = gxGraySpace;
	thisColor.profile = nil;
	thisColor.element.gray = greyLevel;
	GXSetShapeColor(thisShape, &thisColor);
	
} // SetShapeGreyColorLevel

// --------------------------------------------------------------------------------------------------------------
static void	CenterRect(Rect *source, Rect *against)
/*
	Centers "source" within or around "against".
*/
{
	// center picture if requested
	short	height, width, pheight, pwidth;
	
	height = (against->bottom - against->top) >> 1;
	width = (against->right - against->left) >> 1;
	pheight = (source->bottom - source->top) >> 1;
	pwidth = (source->right - source->left) >> 1;
	
	source->top = against->top + height - pheight;
	source->bottom = against->bottom - height + pheight;
	source->left = against->left + width - pwidth;
	source->right = against->right - width + pwidth;

} // CenterRect

// --------------------------------------------------------------------------------------------------------------

static gxShape FindNestedIndexedLayout(gxShape shape, 
					long searchIndex, long * pIndex, gxMapping *pConcatMapping)
/*
	Returns the shape represented by "searchIndex" shapes into the picture,
	sequentially, including all nestings of pictures.  Uses pIndex as
	work storage, which must be initialized to zero before the call.
	
	Returns shape found, or NIL if the searchIndex is larger than
	the number of shapes in the picture.  If NIL is returned,
	then the contents of pIndex will contain the number of
	shapes in the picture.
*/
{
	gxShape		returnShape = nil;
	gxShapeType shapeType = GXGetShapeType(shape);

	// bail on negative index
	if (searchIndex < 0)
		return(nil);
		
	if (shapeType == gxPictureType)
		{	
		long		index, count;
		gxShape		contentShape;
		gxTransform	contentTransform;
		gxMapping	contentMapping;
		
		count = GXGetPicture(shape, nil, nil, nil, nil);
		for (index = 0; index < count; index++)
			{
			GXGetPictureParts(shape, index+1, 1, &contentShape, nil, nil, &contentTransform);
			
			returnShape = FindNestedIndexedLayout(contentShape, searchIndex, pIndex, pConcatMapping ? &contentMapping : nil);
			if (returnShape)
				{
				if (pConcatMapping)
					{
					if (!contentTransform)
						contentTransform = GXGetShapeTransform(contentShape);
					GXGetTransformMapping(contentTransform, &contentMapping);
					MapMapping(&contentMapping, pConcatMapping);
					*pConcatMapping = contentMapping;
					}
				break;
				}
			}
		}
	else 
		{
		if ( (shapeType == gxLayoutType) || (shapeType == gxGlyphType) || (shapeType == gxTextType) )
			{
			(*pIndex)++;
			if (searchIndex == *pIndex)
				returnShape = shape;
			}
		}
		
	return(returnShape);
	
} // FindNestedIndexedLayout

// --------------------------------------------------------------------------------------------------------------
static Boolean PerformNextFind(WindowPtr pWindow, WindowDataPtr pData,
				Str255 findString,
				Boolean caseSensitive,
				Boolean backwards,
				Boolean wraparound)
{
	Boolean	foundSomething = false;
	long	searchIndex, workIndex;
	gxShape	aShape;
	long	direction;
	long	oldPageNumber = ((GXDataPtr)pData)->currentPage;
	long	endPageNumber;
	Boolean	firstTime = true;
	gxMapping	concatMapping;
	
	// initialize direction of the walk
	if (backwards)
		direction = -1;
	else
		direction = 1;
	
	// start searching where we last left off	
	searchIndex = ((GXDataPtr)pData)->currentShapeIndex;
	if (searchIndex != 0)
		searchIndex -= direction;
	
	if (((GXDataPtr)pData)->numberOfPages == 1)
		wraparound = false;

	// end searching on a particular page
	if (backwards)
		endPageNumber = 0;
	else
		endPageNumber = ((GXDataPtr)pData)->numberOfPages + 1;
	
	// can't search on qd shapes, so we get rid of them
	GetRidOfAnyQDShapeTags(((GXDataPtr)pData)->currentPageShape);

	do
		{
		// search for the next shape or prev shape
		searchIndex += direction;
		
		// initialize the working index so that we know traversal level
		workIndex = 0;
		
		// initialize the mapping to identity
		ResetMapping(&concatMapping);
		GXGetShapeMapping(((GXDataPtr)pData)->currentPageShape, &concatMapping);
		
		// find the next layout in the page
		aShape = FindNestedIndexedLayout(((GXDataPtr)pData)->currentPageShape, searchIndex, &workIndex, &concatMapping);
		if (aShape)
			{
			gxShapeType shapeType = GXGetShapeType(aShape);
			long  		size;
			Handle		aHandle;
			
			// determine size and allocate storage for layout contents
			switch (shapeType)
				{
				case gxTextType:
					size = GXGetText(aShape, nil, nil, nil);
					break;
				case gxGlyphType:
					size = GXGetGlyphs(aShape, nil, nil, nil,
									nil, nil, nil, nil, nil);
					break;
				case gxLayoutType:
					size = GXGetLayout(aShape, nil,
									nil, nil, nil, 	// styles
									nil, nil, nil, 	// levels
									nil, nil);		// options/position
					break;
				}
			aHandle = NewHandle(size);
			
			if (aHandle)
				{
				long newStart, newEnd;
				
				// grab the contents of the layout into the temp storage
				HLock(aHandle);
				switch (shapeType)
					{
					case gxTextType:
						GXGetText(aShape, nil, (unsigned char*)*aHandle, nil);
						break;
					case gxGlyphType:
						GXGetGlyphs(aShape, nil,  (unsigned char*)*aHandle, nil,
										nil, nil, nil, nil, nil);
						break;
					case gxLayoutType:
						GXGetLayout(aShape,  (unsigned char*)*aHandle,
									nil, nil, nil, 	// styles
									nil, nil, nil, 	// levels
									nil, nil);		// options/position
						break;
					}
				HUnlock(aHandle);
				
				// search the handle for the string we're looking for,
				// but don't wraparound because we handle that over layout
				// ranges and pages ourselves
				{
				long offset;
				
				if ((firstTime) && (((GXDataPtr)pData)->currentSelectionShape))
					{
					// for shape that we have found something in before
					// start at end of last point if forwards, start of last point
					// if backwards
					firstTime = false;
					offset = backwards ? ((GXDataPtr)pData)->currentShapeStart : ((GXDataPtr)pData)->currentShapeEnd;
					}
				else
					{
					// for "new" shape we haven't hit before, start at
					// begining for forwards, end for backwards
					offset = backwards ? size : 0;
					}
					
				foundSomething = PerformSearch(aHandle, offset, findString, 
							caseSensitive, backwards, false,
							&newStart, &newEnd);
				}
							
				// done with our temp storage
				DisposeHandle(aHandle);
				
				// got it?  then mark it and bail out
				if (foundSomething)
					{
					// remember where we are in the page
					((GXDataPtr)pData)->currentShapeIndex = searchIndex;
					
					// what offsets the selection is
					((GXDataPtr)pData)->currentShapeStart = newStart;
					((GXDataPtr)pData)->currentShapeEnd = newEnd;
					
					// and the shape containing the selection
					if (((GXDataPtr)pData)->currentSelectionShape)
						GXDisposeShape(((GXDataPtr)pData)->currentSelectionShape);
					((GXDataPtr)pData)->currentSelectionShape = GXCloneShape(aShape);
					((GXDataPtr)pData)->currentSelectionMapping = concatMapping;
					break;
					} // found the string
					
				} // allocated the handle
				
			} // found a shape
		else
			{
			OSErr	anErr = noErr;
			
			// didn't find it on this page, move on
			((GXDataPtr)pData)->currentPage += direction;
			
				
			// clamp to the ends of the range
			if (backwards)
				{
				if (((GXDataPtr)pData)->currentPage <= endPageNumber)
					{
					if (wraparound)
						{
						((GXDataPtr)pData)->currentPage = ((GXDataPtr)pData)->numberOfPages;
						endPageNumber = oldPageNumber;
						wraparound = false;
						}
					else
						anErr = paramErr;
					}
				}
			else
				{
				if (((GXDataPtr)pData)->currentPage >= endPageNumber)
					{
					if (wraparound)
						{
						((GXDataPtr)pData)->currentPage = 1;
						endPageNumber = oldPageNumber;
						wraparound = false;
						}
					else
						anErr = paramErr;
					}
				}
				
			// fetch contents
			if (anErr == noErr)
				anErr = GetCurrentPage((GXDataPtr) pData, false);
				
			// anything wrong?  then all done searching
			if (anErr != noErr)
				{
				break;
				}
			else
				{
				GetRidOfAnyQDShapeTags(((GXDataPtr)pData)->currentPageShape);
				if (backwards)
					{
					workIndex = 0;
					(void) FindNestedIndexedLayout(((GXDataPtr)pData)->currentPageShape, 0x7FFFFFF, &workIndex, nil);
					searchIndex = workIndex;
					}
				else
					searchIndex = 0;
				}
			}
		} while (!foundSomething);
	
	// if we found something, force and update.  If not, make sure
	// that the current page is restored to the page we had when 
	// coming in.
	if (foundSomething)
		{
		InvalRect(&GetWindowPort(pWindow)->portRect);
		}
	else
		{
		if (oldPageNumber != ((GXDataPtr)pData)->currentPage)
			{
			((GXDataPtr)pData)->currentPage = oldPageNumber;
			GetCurrentPage((GXDataPtr) pData, false);
			}
		}
	return(foundSomething);
	
} // PerformNextFind

// --------------------------------------------------------------------------------------------------------------
static gxShape GetCurrentSelectionHighlight(WindowDataPtr pData, Boolean mapIt)
{
	gxShape		highlight;
	
	highlight = GXGetLayoutHighlight(((GXDataPtr)pData)->currentSelectionShape, 
						((GXDataPtr)pData)->currentShapeStart, ((GXDataPtr)pData)->currentShapeEnd,
						gxHighlightAverageAngle, nil);
						
	
	if (mapIt)
		GXMapShape(highlight, &((GXDataPtr)pData)->currentSelectionMapping);
	
	// draw and dispose of the highlight
	GXSetShapeViewPorts(highlight, 1, &((GXDataPtr)pData)->childViewPort);
	GXSetShapeFill(highlight, gxClosedFrameFill);
	GXSetShapeClip(highlight, nil);

	return(highlight);
	
} // GetCurrentSelectionHighlight

// --------------------------------------------------------------------------------------------------------------
static void ScrollFoundShapeIntoView(WindowPtr pWindow, WindowDataPtr pData)
{
	gxRectangle	bounds;
	Point		scrollAmount;
	Point		controlValues;
	gxRectangle	windowBounds;
	GrafPtr		pPort = (GrafPtr)GetWindowPort(pWindow);
	
	if ( ! (((GXDataPtr)pData)->currentSelectionShape) )
		return;
		
	// cache scroll state
	controlValues.h = GetControlValue(pData->hScroll);
	controlValues.v = GetControlValue(pData->vScroll);
	
	// calculate visible bounds of window
	windowBounds.left 		= ff(pPort->portRect.left + controlValues.h);
	windowBounds.right 		= ff(pPort->portRect.right - kScrollBarSize + controlValues.h);
	windowBounds.top 		= ff(pPort->portRect.top + controlValues.v);
	windowBounds.bottom 	= ff(pPort->portRect.bottom - kScrollBarSize + controlValues.v);				

	// grab the bounds of the shape, add on the margins, scale to zoom factor
	{
	gxRectangle		pageSize, paperSize;
	gxShape			highlight = GetCurrentSelectionHighlight(pData, false);
	
	GXGetShapeBounds(highlight, 0, &bounds);
	GXDisposeShape(highlight);
	GetCurrentPageAndPaper(pData, &pageSize, &paperSize);
	bounds.left 	= FixedMultiply(bounds.left - paperSize.left, ((GXDataPtr)pData)->zoomFactor);
	bounds.right 	= FixedMultiply(bounds.right - paperSize.left, ((GXDataPtr)pData)->zoomFactor);
	bounds.top 		= FixedMultiply(bounds.top - paperSize.top, ((GXDataPtr)pData)->zoomFactor);
	bounds.bottom 	= FixedMultiply(bounds.bottom - paperSize.top, ((GXDataPtr)pData)->zoomFactor);
	}

	if 	(
		(bounds.bottom <= windowBounds.top) ||
		(bounds.top >= windowBounds.bottom) ||
		(bounds.right <= windowBounds.left) ||
		(bounds.left >= windowBounds.right) 
		)
		{
		scrollAmount.h = controlValues.h - FixedToInt(bounds.left);
		scrollAmount.v = controlValues.v - FixedToInt(bounds.top);
		
		SetControlAndClipAmount(pData->hScroll, &scrollAmount.h);
		SetControlAndClipAmount(pData->vScroll, &scrollAmount.v);
		if ((scrollAmount.h) || (scrollAmount.v))
			DoScrollContent(pWindow, pData, scrollAmount.h, scrollAmount.v);
		}
		
} // ScrollFoundShapeIntoView

// --------------------------------------------------------------------------------------------------------------
static Boolean TrackIn(Rect *pTrackRect, Point clickPoint, Rect *pDrawRect, short inID, short outID)
{
	Boolean	in = false;
	
	if (PtInRect(clickPoint, pTrackRect))
		{
		in = true;
		
		PlotIconID(pDrawRect, ttNone, ttNone, inID);
		while (StillDown())
			{
			GetMouse(&clickPoint);
			
			if (PtInRect(clickPoint, pTrackRect))
				{
				if (!in)
					{
					in = true;
					PlotIconID(pDrawRect, ttNone, ttNone, inID);
					}
				}
			else
				{
				if (in)
					{
					in = false;
					PlotIconID(pDrawRect, ttNone, ttNone, outID);
					}
				}
			}
		}
		
	return(in);
	
} // TrackIn

// --------------------------------------------------------------------------------------------------------------
static void DrawPageSliderAndThumb(WindowPtr pWindow, long currentValue, long maxValue)
{
	Rect		pageSliderRect;
	Rect		pageThumbRect;
	long		pixelValue;
	Str255		aString;
	FontInfo	theInfo;
	PicHandle	proxyHandle;
	Rect		proxyRect;
	GrafPtr		pPort = (GrafPtr)GetWindowPort(pWindow);
	
	// calculate location of the slider
	pageSliderRect.left 	= kPageSliderMargins;
	pageSliderRect.bottom 	= pPort->portRect.bottom - kPageSliderMargins;
	pageSliderRect.top 		= pageSliderRect.bottom - kPageSliderHeight;
	pageSliderRect.right 	= pPort->portRect.right - kPageSliderMargins;
		
	// then calculate the thumb within that slider
	pixelValue = (currentValue-1) 
				*
				(pageSliderRect.right - pageSliderRect.left - kPageThumbMargins*2 - kPageThumbWidth) 
				/
				(maxValue-1);
				
	pageThumbRect.left 		= pageSliderRect.left + kPageThumbMargins + pixelValue;
	pageThumbRect.right		= pageThumbRect.left + kPageThumbWidth;
	pageThumbRect.top		= pageSliderRect.top - kPageThumbEdge;
	pageThumbRect.bottom	= pageThumbRect.top + kPageThumbHeight;

	// and finally, the location to draw the proxy (if any)
	proxyRect.top			= kPageSliderMargins;
	proxyRect.bottom		= proxyRect.top + kProxyHeight;
	proxyRect.left			= pPort->portRect.left + 
								((pPort->portRect.right - pPort->portRect.left) >> 1) -
								(kProxyWidth >> 1);
	proxyRect.right			= proxyRect.left + kProxyWidth;
	if (Count1Resources(kProxyType) == 0)
		proxyRect.bottom = proxyRect.top;
		
	// draw the slider area
	FillRect(&pageSliderRect, &qd.gray);
	FrameRect(&pageSliderRect);

	// erase areas above and below the slider (old thumb erase)
	{
	Rect	sliderEraseRect = pageSliderRect;
	
	ForeColor(whiteColor);
	sliderEraseRect.top = pageThumbRect.top;
	sliderEraseRect.bottom = sliderEraseRect.top + kPageThumbEdge;
	PaintRect(&sliderEraseRect);
	sliderEraseRect.bottom = pageThumbRect.bottom;
	sliderEraseRect.top = sliderEraseRect.bottom - kPageThumbEdge;
	PaintRect(&sliderEraseRect);
	}
	
	// draw the thumb
	ForeColor(blackColor);
	FrameRect(&pageThumbRect);
	InsetRect(&pageThumbRect, 1, 1);
	ForeColor(whiteColor);
	FrameRect(&pageThumbRect);
	InsetRect(&pageThumbRect, 1, 1);
	ForeColor(blackColor);
	FrameRect(&pageThumbRect);

	// draw page string label
	TextFace(bold);
	TextFont(applFont);
	TextSize(9);
	TextMode(srcCopy);
	GetFontInfo(&theInfo);
	
	MoveTo(pageSliderRect.left, pageThumbRect.top - kPageThumbEdge - theInfo.descent);
	GetIndString(aString, kPageControlStrings, iGoToPageString);
	DrawString(aString);
	NumToString(currentValue, aString);
	DrawString(aString);
	
	// erase any trailing digits (pretty cheezy, but seems to work)
	DrawString("\p       ");
	
	// draw the proxy, or erase the proxy area if no picture to draw
	proxyHandle = (PicHandle) GetResource(kProxyType, kProxyBaseID + currentValue - 1);
	if (proxyHandle)
		{
		Rect	drawRect;
		Fixed	scaleFactor;
		
		drawRect = (**proxyHandle).picFrame;
		
		// compute aspect ratio preserving scale
		if (RectHeight(drawRect) > RectWidth(drawRect))
			scaleFactor = FixRatio(RectHeight(proxyRect), RectHeight(drawRect));
		else
			scaleFactor = FixRatio(RectWidth(proxyRect), RectWidth(drawRect));
		drawRect.bottom = drawRect.top +
					( FixMul( (RectHeight(drawRect) << 16), scaleFactor) >> 16);
		drawRect.right = drawRect.left +
					( FixMul( (RectWidth(drawRect) << 16), scaleFactor) >> 16);
		CenterRect(&drawRect, &proxyRect);
		
		// erase the area outside of the picture, but inside of the 
		// total proxy area, because some pictures will leave whitespace on the edge
		{
		RgnHandle	rgn1 = NewRgn();
		RgnHandle	rgn2 = NewRgn();
		
		RectRgn(rgn1, &proxyRect);
		RectRgn(rgn2, &drawRect);
		DiffRgn(rgn1, rgn2, rgn1);
		EraseRgn(rgn1);
		DisposeRgn(rgn1);
		DisposeRgn(rgn2);
		}

		// finally, we can draw and dispose of the picture
		DrawPicture(proxyHandle, &drawRect);
		ReleaseResource((Handle) proxyHandle);
		
		}
	else
		{
		EraseRect(&proxyRect);
		}
		
} // DrawPageSliderAndThumb

// --------------------------------------------------------------------------------------------------------------
static OSErr	DoDrawingClick(WindowPtr pWindow, WindowDataPtr pData, Point clickPoint, EventRecord *pEvent)
{
#pragma unused (pEvent, pWindow)

	OSErr	anErr = noErr;
	Point	lastPoint = clickPoint;
	Point	currentPoint;
	Fixed	penSize;
	gxInk	newInk = GXNewInk();
	gxStyle newStyle = GXNewStyle();
	gxShape	newShape = GXNewShape(gxPolygonType);
	long	addPoly[] = {1, 1, 0, 0};
	
	// set up the style for the shape
	GXSetStylePen(newStyle, ff(10));
	
	// and the ink for the shape
	{
	gxColor	redColor;
	gxTransferMode mode;
	
	// the color
	redColor.space		= gxRGBSpace;
	redColor.profile	= nil;
	redColor.element.rgb.red	= 0xFFFF;
	redColor.element.rgb.green	= 0x0000;
	redColor.element.rgb.blue	= 0x0000;
	GXSetInkColor(newInk, &redColor);
	
	// the transfer mode
	mode.space 		= gxHSVSpace;
	mode.set		= nil;
	mode.profile	= nil;
	InitColorMatrix(mode.sourceMatrix);
	InitColorMatrix(mode.deviceMatrix);
	InitColorMatrix(mode.resultMatrix);
	mode.flags		= 0;
	
	mode.component[0].mode	= gxCopyMode;
	mode.component[0].flags	= 0;
	mode.component[0].sourceMinimum	= 0;
	mode.component[0].sourceMaximum	= gxColorValue1;
	mode.component[0].deviceMinimum	= 0;
	mode.component[0].deviceMaximum	= gxColorValue1;
	mode.component[0].clampMinimum	= 0;
	mode.component[0].clampMaximum	= gxColorValue1;
	mode.component[0].operand		= 0;

	mode.component[1].mode	= gxCopyMode;
	mode.component[1].flags	= 0;
	mode.component[1].sourceMinimum	= 0;
	mode.component[1].sourceMaximum	= gxColorValue1;
	mode.component[1].deviceMinimum	= 0;
	mode.component[1].deviceMaximum	= gxColorValue1;
	mode.component[1].clampMinimum	= 0;
	mode.component[1].clampMaximum	= gxColorValue1;
	mode.component[1].operand		= 0;

	mode.component[2].mode	= gxNoMode;
	mode.component[2].flags	= 0;
	mode.component[2].sourceMinimum	= 0;
	mode.component[2].sourceMaximum	= gxColorValue1;
	mode.component[2].deviceMinimum	= 0;
	mode.component[2].deviceMaximum	= gxColorValue1;
	mode.component[2].clampMinimum	= 0;
	mode.component[2].clampMaximum	= gxColorValue1;
	mode.component[2].operand		= 0;
	GXSetInkTransfer(newInk, &mode);
	}
	
	// set the style and ink of the shape
	GXSetShapeStyle(newShape, newStyle);
	GXSetShapeInk(newShape, newInk);
	GXSetShapeFill(newShape, gxOpenFrameFill);
	
	// initialize the first point in the shape
	addPoly[2] = ff(lastPoint.h);
	addPoly[3] = ff(lastPoint.v);
	GXSetPolygonParts(newShape, gxSelectToEnd, 1, (gxPolygons*)addPoly, gxRemoveDuplicatePointsEdit);

	// determine the amount we require the mouse to move before adding a new point
	penSize = FixedDivide(GXGetStylePen(newStyle), ff(2));
	if (penSize < ff(1))
		penSize = ff(1);

	do
		{
		GetMouse(&currentPoint);
		if 	(
			(ff(ABS(currentPoint.h - lastPoint.h)) > penSize) ||
			(ff(ABS(currentPoint.v - lastPoint.v)) > penSize)
			)
			{
			// add the new point to the new shape
			lastPoint = currentPoint;
			addPoly[2] = ff(lastPoint.h);
			addPoly[3] = ff(lastPoint.v);
			GXSetPolygonParts(newShape, gxSelectToEnd, 1, (gxPolygons*)addPoly, gxRemoveDuplicatePointsEdit);
			GXDrawShape(newShape);
			}
		} while (StillDown());
	
	{
	Fixed			zoomFactor = ((GXDataPtr)pData)->zoomFactor;
	Fixed			shapeScale = FixedDivide(ff(1), zoomFactor);
	gxRectangle		pageSize, paperSize;
		
	// offset the shape by the scroll bars & margins
	GetCurrentPageAndPaper(pData, &pageSize, &paperSize);
	GXMoveShape(newShape, 
				ff(GetControlValue(pData->hScroll)) + FixedMultiply(paperSize.left, zoomFactor),
				ff(GetControlValue(pData->vScroll)) + FixedMultiply(paperSize.top, zoomFactor) );

	// scale the shape to the current scale factor
	GXScaleShape(newShape, shapeScale, shapeScale, 0, 0 );
	GXSetShapePen(newShape, FixedMultiply(GXGetShapePen(newShape), shapeScale) );
	}
	
	// add shape to the page
	{
	gxShape		annotationShape = (*((GXDataPtr)pData)->pageAnnotations)[((GXDataPtr)pData)->currentPage-1];
	
	if (!annotationShape)
		{
		annotationShape = GXNewShape(gxPictureType);
		(*((GXDataPtr)pData)->pageAnnotations)[((GXDataPtr)pData)->currentPage-1] = annotationShape;
		}
	if (annotationShape)
		GXSetPictureParts(annotationShape, 0, 0, 1, &newShape, nil, nil, nil);
	}

	// all done with our copies of the shape, style, and ink	
	GXDisposeShape(newShape);
	GXDisposeStyle(newStyle);
	GXDisposeInk(newInk);
	
	// we've touched the file
	pData->changed = true;
	
	return(anErr);
	
} // DoDrawingClick

// --------------------------------------------------------------------------------------------------------------

// Handle update/activate events behind Standard File
static pascal Boolean SaveDialogFilter(DialogPtr theDialog, EventRecord *theEvent,
									  short *itemHit, void *myDataPtr)
{
	#pragma unused(myDataPtr)

	if (StdFilterProc(theDialog, theEvent, itemHit))
		return true;

	// Pass updates through (Activates are tricky...was mucking with Apple menu & thereby
	// drastically changing how the system handles the menu bar during our alert)
	if (theEvent->what == updateEvt /* || theEvent->what == activateEvt */ )
		{
		HandleEvent(theEvent);
		}

	return false;

} // SaveDialogFilter

#if GENERATINGCFM
	static RoutineDescriptor gSaveDialogFilterRD = BUILD_ROUTINE_DESCRIPTOR(uppModalFilterYDProcInfo, SaveDialogFilter);
	static ModalFilterYDUPP gSaveDialogFilter = &gSaveDialogFilterRD;
#else
	static ModalFilterYDUPP gSaveDialogFilter = NewModalFilterYDProc(SaveDialogFilter);
#endif

// --------------------------------------------------------------------------------------------------------------
#define kLoadAnnotations	true
#define kSaveAnnotations	false

static OSErr	LoadOrSaveAnnotations(WindowDataPtr pData, Boolean load)
{
	OSErr				anErr = noErr;
	short				i;
	short				oldResFile = CurResFile();
	userSpool 			block;
	
	block.spool.spoolProcedure = gHandleSpoolProc;
	
	UseResFile(((GXDataPtr)pData)->printFileRefNum);
	for (i = 0; i < ((GXDataPtr)pData)->numberOfPages; ++i)
		{
		gxShape annotation = (*((GXDataPtr)pData)->pageAnnotations)[i];
		Handle	annotationHandle;
		
		block.spool.buffer = nil;
		block.spool.bufferSize = 0;
		if (load)
			{
			// load annotation, if any
			annotationHandle = Get1Resource(kAnnotationType, kAnnotationBaseID + i-1);
			if (annotationHandle)
				{
				block.data = annotationHandle;
				annotation = GXUnflattenShape(&block.spool, 0, nil);
				ReleaseResource(annotationHandle);
				}
			else
				{
				annotation = nil;
				}
			}
		else
			{
			// remove old annotation
			annotationHandle = Get1Resource(kAnnotationType, kAnnotationBaseID + i-1);
			if (annotationHandle)
				RemoveResource(annotationHandle);
				
			// add new annotation
			if (annotation)
				{
				block.spool.spoolProcedure = gHandleSpoolProc;
				block.spool.buffer = nil;
				block.spool.bufferSize = 0;
				GXFlattenShape(annotation, 0, &block.spool);
				annotationHandle = (Handle) block.data;
				if (annotationHandle)
					{
					AddResource(annotationHandle, kAnnotationType, kAnnotationBaseID + i-1, "\p");
					ReleaseResource(annotationHandle);
					}
				}
			}
			
		(*((GXDataPtr)pData)->pageAnnotations)[i] = annotation;
		}
	UpdateResFile(CurResFile());
	UseResFile(oldResFile);
	
	return(anErr);
	
} // LoadOrSaveAnnotations

// --------------------------------------------------------------------------------------------------------------
static OSErr	GXSaveAs(WindowPtr pWindow, WindowDataPtr pData)
{
	OSErr				anErr = noErr;
	Boolean				replacing;
	FSSpec				fileSpec;
	NavReplyRecord		navReply;
	
	// ask where and how to save this document
	{
	Str255	defaultName;
	Point	where = {-1, -1};
	
	// setup for the call
	GetWTitle(pWindow, defaultName);
	SetCursor(&qd.arrow);
	
	// find out where the user wants the file
	if (gMachineInfo.haveNavigationServices)
		{				
		anErr = SaveFileDialog(defaultName, pData->originalFileType, 'ttxt', MyEventProc, &fileSpec, NULL, &replacing, &navReply);
		if ( anErr == userCanceledErr)
			anErr = eUserCanceled;
		}
	else
		{
		StandardFileReply	sfReply;
	
		// find out where the user wants the file
		CustomPutFile("\p", defaultName, &sfReply, 
					sfPutDialogID, where,
					nil, gSaveDialogFilter, nil, nil, nil);
		
		// map the cancel button into a cancelling error
		if (!sfReply.sfGood)
			anErr = eUserCanceled;

		replacing	= sfReply.sfReplacing;
		fileSpec	= sfReply.sfFile;
		}
	}
		
	// can't replace over other types	
	if (replacing)
		{
		FInfo	theInfo;
		
		FSpGetFInfo(&fileSpec, &theInfo);
		
		if ( 
			(theInfo.fdType != 'sjob') && 
			(theInfo.fdType != 'tjob') && 
			(theInfo.fdType != 'rjob') && 
			(theInfo.fdType != 'qjob') 
			)
			anErr = eDocumentWrongKind;
		}
	nrequire(anErr, StandardPutFile);
		
	GXSavePrintFile(((GXDataPtr)pData)->thePrintFile, &fileSpec);
	anErr = GXGetJobError(pData->hPrint);


	// FALL THROUGH EXCEPTION HANDLING
StandardPutFile:

	// if everything went okay
	if (anErr == noErr)
		{
		// update the window title 
		SetWTitle(pWindow, fileSpec.name);
	
		// save new location
		BlockMoveData(&fileSpec, &pData->fileSpec, sizeof(FSSpec));

		// update the refNum
		((GXDataPtr)pData)->printFileRefNum = CurResFile();
		
		// and read in the current page
		anErr = GetCurrentPage((GXDataPtr) pData, true);
		}
		
// Return eUserCanceled so we can avoid closing/quitting if they cancel the SF dialog
//	// don't propagate this error
//	if (anErr == eUserCanceled)
//		anErr = noErr;

	if (gMachineInfo.haveNavigationServices)
		{
		CompleteSave(&fileSpec, &navReply);
		}

	return anErr;
	
} // GXSaveAs

// --------------------------------------------------------------------------------------------------------------
// OOP INTERFACE ROUTINES
// --------------------------------------------------------------------------------------------------------------

static OSErr	GXCloseWindow(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	if (((GXDataPtr)pData)->pageAnnotations)
		{
		short	i;
		
		for (i = 0; i < ((GXDataPtr)pData)->numberOfPages; ++i)
			{
			gxShape annotation = (*((GXDataPtr)pData)->pageAnnotations)[i];
			
			if (annotation)
				GXDisposeShape(annotation);
			}
		DisposeHandle((Handle) (((GXDataPtr)pData)->pageAnnotations) );
		((GXDataPtr)pData)->pageAnnotations = nil;
		}
	
	if (((GXDataPtr)pData)->currentSelectionShape)
		{
		GXDisposeShape(((GXDataPtr)pData)->currentSelectionShape);
		((GXDataPtr)pData)->currentSelectionShape = nil;
		}
						
	if (((GXDataPtr)pData)->currentPageShape)
		{
		GXDisposeShape(((GXDataPtr)pData)->currentPageShape);
		((GXDataPtr)pData)->currentPageShape = nil;
		}
	GXClosePrintFile( ((GXDataPtr)pData)->thePrintFile);

	GXDisposeViewPort( ((GXDataPtr)pData)->parentViewPort);
	GXDisposeViewPort( ((GXDataPtr)pData)->childViewPort);

	return(noErr);
	
} // GXCloseWindow

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXUpdateWindow(WindowPtr pWindow, WindowDataPtr pData)
{
	gxGraphicsError		anErr = noErr;	

	// draw informational area to the left of the horizontal scroll bar
	{
	FontInfo	theInfo;
	Rect		infoArea;
	RgnHandle	oldClip = NewRgn();
	Handle		theString;
	long		theStringSize;
	
	// save old clip and clip to the label area
	GetClip(oldClip);
	infoArea.left = 0;
	infoArea.right = pData->hScrollOffset-1;
	infoArea.bottom = GetWindowPort(pWindow)->portRect.bottom;
	infoArea.top = infoArea.bottom - kScrollBarSize;
	ClipRect(&infoArea);
		
	// draw the label
	TextFont(applFont);
	TextSize(9);
	GetFontInfo(&theInfo);
	theString = GetResource('LSTR', kLabelString);
	if (theString)
		{
		Handle	inString = NewHandle(sizeof(Str255));
		Str255	newString;
		Rect	labelArea = infoArea;
		
		// erase any old string we had there
		labelArea.right -= kZoomControlsWidth + kToolControlWidth;
		if (((GXDataPtr)pData)->numberOfPages > 1)
			labelArea.left += kPageControlsWidth;		
		EraseRect(&labelArea);
		
		// current page label
		NumToString(((GXDataPtr)pData)->currentPage, newString);
		SetHandleSize(inString, newString[0]);
		BlockMoveData(&newString[1], *inString, newString[0]);
		ReplaceText(theString, inString, "\p^0");

		// total page count label
		NumToString(((GXDataPtr)pData)->numberOfPages, newString);
		SetHandleSize(inString, newString[0]);
		BlockMoveData(&newString[1], *inString, newString[0]);
		ReplaceText(theString, inString, "\p^1");

		// scale factor label
		NumToString(FixedToInt( FixedMultiply(((GXDataPtr)pData)->zoomFactor, ff(100)) ), newString);
		SetHandleSize(inString, newString[0]);
		BlockMoveData(&newString[1], *inString, newString[0]);
		ReplaceText(theString, inString, "\p^2");

		// done with replace string content
		DisposeHandle(inString);
		
		// draw the label
		HLock(theString);
		theStringSize = GetHandleSize(theString);
		MoveTo(labelArea.left + ((labelArea.right - labelArea.left) >> 1) - (TextWidth(*theString, 0, theStringSize) >> 1), 
				labelArea.top + ((labelArea.bottom - labelArea.top)>>1) + ((theInfo.ascent + theInfo.descent) >> 1) - theInfo.descent);
		DrawText(*theString, 0, theStringSize);
		ReleaseResource(theString);
		}
		
	// draw the current tool
		{
		Rect		toolArea = infoArea;
		CIconHandle	icon;
		
		toolArea.left = toolArea.right - kToolControlWidth;
		toolArea.right = toolArea.left + 32;
		toolArea.bottom = toolArea.top + 32;
		EraseRect(&toolArea);
		OffsetRect(&toolArea, -8, -8);
		
		icon = GetCIcon(kIconBase + ((GXDataPtr)pData)->contentClickMode);
		if (icon)
			{
			PlotCIconHandle(&toolArea, kAlignAbsoluteCenter, ttNone, icon);
			DisposeCIcon(icon);
			}
		}

	// draw the zoom controls
		{
		Rect	zoomArea = infoArea;
		zoomArea.left = zoomArea.right - kZoomControlsWidth - kToolControlWidth;
		zoomArea.right = zoomArea.left + 32;
		zoomArea.bottom = zoomArea.top + 32;
		
		PlotIconID(&zoomArea, ttNone, ttNone, kZoomControlPlain);
		}
				
	// draw the left/right/page arrows
	if (((GXDataPtr)pData)->numberOfPages > 1)
		{
		Rect	arrowsRect;
		
		// erase any old arrow bits, including around the edges of the icon
		// needed when the window resizes
		arrowsRect = infoArea;
		arrowsRect.bottom = arrowsRect.top + 32;
		arrowsRect.right = arrowsRect.left + 34;
		EraseRect(&arrowsRect);
		
		// then draw the new arrows
		arrowsRect.left = infoArea.left + 2;
		arrowsRect.top = infoArea.top + 2;
		arrowsRect.right = arrowsRect.left + kPageControlsWidth;
		arrowsRect.bottom = arrowsRect.top + 32;
		PlotIconID(&arrowsRect, ttNone, ttNone, kPageControlPlain);
		}
	
	// frame the area
	MoveTo(infoArea.left, infoArea.top);
	LineTo(infoArea.right, infoArea.top);

	// restore old clip value
	SetClip(oldClip);
	DisposeRgn(oldClip);
	}


	// then draw the page shape and things around it
	{
	gxRectangle			pageSize, paperSize;	
	gxShape				tempShape, pageShape;
	gxMapping			thisMapping;

	// clip to the content area
	paperSize.left 		= ff(pData->contentRect.left);
	paperSize.top 		= ff(pData->contentRect.top);
	paperSize.right 	= ff(pData->contentRect.right);
	paperSize.bottom 	= ff(pData->contentRect.bottom);
	tempShape = GXNewRectangle(&paperSize);
	GXSetViewPortClip(((GXDataPtr)pData)->childViewPort, tempShape);
	GXDisposeShape(tempShape);
	
	// get the paper sizes, account for zoom factor
	GetCurrentPageAndPaper(pData, &pageSize, &paperSize);
	pageSize.left 		= FixedMultiply(pageSize.left, ((GXDataPtr)pData)->zoomFactor);
	pageSize.right 		= FixedMultiply(pageSize.right, ((GXDataPtr)pData)->zoomFactor);
	pageSize.top 		= FixedMultiply(pageSize.top, ((GXDataPtr)pData)->zoomFactor);
	pageSize.bottom 	= FixedMultiply(pageSize.bottom, ((GXDataPtr)pData)->zoomFactor);
	paperSize.left 		= FixedMultiply(paperSize.left, ((GXDataPtr)pData)->zoomFactor);
	paperSize.right 	= FixedMultiply(paperSize.right, ((GXDataPtr)pData)->zoomFactor);
	paperSize.top 		= FixedMultiply(paperSize.top, ((GXDataPtr)pData)->zoomFactor);
	paperSize.bottom 	= FixedMultiply(paperSize.bottom, ((GXDataPtr)pData)->zoomFactor);
	
	// offset by the scrolling amount	
	ResetMapping(&thisMapping);
	MoveMapping(&thisMapping, Long2Fix(-GetControlValue(pData->hScroll)) - paperSize.left ,
				Long2Fix(-GetControlValue(pData->vScroll)) - paperSize.top );

	// make the paper shape
	tempShape = GXNewShape(gxFullType);
	GXSetTransformViewPorts(GXGetShapeTransform(tempShape), 1, & ((GXDataPtr)pData)->childViewPort);
	GXSetShapeFill(tempShape, gxEvenOddFill);
	GXSetShapeMapping(tempShape, &thisMapping);
	
	// make the page shape
	pageShape = GXNewRectangle(&pageSize);
	GXSetTransformViewPorts(GXGetShapeTransform(pageShape), 1, & ((GXDataPtr)pData)->childViewPort);
	GXSetShapeFill(pageShape, gxEvenOddFill);
	GXSetShapeMapping(pageShape, &thisMapping);
	
	// remove the page shape from the paper shape
	GXDifferenceShape(tempShape, pageShape);

	// draw the paper shape, dispose of it
	SetShapeGreyColorLevel(tempShape, 0xD000);	/* Set up light gray background */
	GXDrawShape(tempShape);
	GXDisposeShape(tempShape);
	
	// draw white on the page shape
	SetShapeGreyColorLevel(pageShape, 0xFFFF);	/* Set up white page */
	GXSetShapeFill(pageShape, gxEvenOddFill);
	GXDrawShape(pageShape);
	
	// frame the page shape
	SetShapeGreyColorLevel(pageShape, 0x8000);	/* Set up medium gray frame */
	GXSetShapeFill(pageShape, gxClosedFrameFill);
	GXDrawShape(pageShape);
	
	// draw the scroll bars and grow box now to give a nice appearence
	DrawControls(pWindow);
	DrawGrowIcon(pWindow);

	// draw the page data itself, clipped to the page

	{
	gxMapping	oldMapping, flipMapping;
	
	// get the page shape's old mapping, and make a copy to work with
	GXGetShapeMapping(((GXDataPtr)pData)->currentPageShape, &oldMapping);

	// run the clip through the inverse of the shape mapping to scale it properly
	GXInsetShape(pageShape, ff(1));
	ScaleMapping(&oldMapping, ((GXDataPtr)pData)->zoomFactor, ((GXDataPtr)pData)->zoomFactor, 0, 0);
	InvertMapping(&flipMapping, &oldMapping);
	GXMapShape(pageShape, &flipMapping);
	GXSetShapeFill(pageShape, gxEvenOddFill);
	GXSetShapeClip( ((GXDataPtr)pData)->currentPageShape, pageShape);
	
	// move the shape into position by offseting the viewPort
	GXGetViewPortMapping(((GXDataPtr)pData)->childViewPort, &thisMapping);	
	GXGetViewPortMapping(((GXDataPtr)pData)->childViewPort, &oldMapping);	
	ScaleMapping(&thisMapping, ((GXDataPtr)pData)->zoomFactor, ((GXDataPtr)pData)->zoomFactor, 0, 0);
	MoveMapping(&thisMapping, Long2Fix(-GetControlValue(pData->hScroll)) - paperSize.left ,
				Long2Fix(-GetControlValue(pData->vScroll)) - paperSize.top );
	GXSetViewPortMapping(((GXDataPtr)pData)->childViewPort, &thisMapping);	

	/*
	 *	Bracket the call to DrawShape with UseResFile, so that we put the
	 *	document's resfile on top, allowing the translator (for QDShapes)
	 *	to see our embedded fonts (if any) first.
	 */
	{	short oldResFile = CurResFile();
	
		UseResFile(((GXDataPtr)pData)->printFileRefNum);
		GXDrawShape( ((GXDataPtr)pData)->currentPageShape);
		UseResFile(oldResFile);
	}

	
	// Draw the selection, if any
	{
	gxShape	selectionShape =  ((GXDataPtr)pData)->currentSelectionShape;
	gxShape	highlight;
	
	if (selectionShape)
		{
		
		// better be a layout shape to get hilights
		GXSetShapeType(selectionShape, gxLayoutType);
		
		// get the highlight
		highlight = GetCurrentSelectionHighlight(pData, true);
		GXDrawShape(highlight);
		GXDisposeShape(highlight);						
		}
	}

	// Draw the overlay, if any
	{
	gxShape	annotationShape =  (*((GXDataPtr)pData)->pageAnnotations)[((GXDataPtr)pData)->currentPage-1];
	
	if (annotationShape)
		{
		GXSetShapeViewPorts(annotationShape, 1, &((GXDataPtr)pData)->childViewPort);
		GXDrawShape(annotationShape);
		}	
	}

	// restore viewPort's mapping (so we don't use it again next time)
	GXSetViewPortMapping(((GXDataPtr)pData)->childViewPort, &oldMapping);	

	
	}
	
	// done with the page shape
	GXDisposeShape(pageShape);

	DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, false);

	GXGetGraphicsError(&anErr);
	}
	
	return(anErr);
	
} // GXUpdateWindow

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXContentClick(WindowPtr pWindow, WindowDataPtr pData, EventRecord *pEvent)
{
	OSErr			anErr = noErr;
	Point			clickPoint = pEvent->where;
	Rect			infoArea, labelArea, toolArea;
	Rect			zoomOutRect, zoomInRect, zoomsRect;
	RgnHandle		oldClip = NewRgn();
	Boolean			somethingHit = false;
	
	// convert to local space, calculate clickable areas	
	GlobalToLocal(&clickPoint);
	infoArea.left = 0;
	infoArea.right = pData->hScrollOffset-1;
	infoArea.bottom = GetWindowPort(pWindow)->portRect.bottom;
	infoArea.top = infoArea.bottom - kScrollBarSize;

	// clip to the info area
	GetClip(oldClip);
	ClipRect(&infoArea);

	// label area
	labelArea = infoArea;
	labelArea.right -= kZoomControlsWidth + kToolControlWidth;
	if (((GXDataPtr)pData)->numberOfPages > 1)
		labelArea.left += kPageControlsWidth;		

	// the tool pop up
	toolArea = infoArea;
	toolArea.left = toolArea.right - kToolControlWidth;
	
	// calculate the zoom in/out rects
	zoomInRect = infoArea;
	zoomInRect.right -= kToolControlWidth;
	zoomInRect.left = zoomInRect.right - 13;
	zoomOutRect = zoomInRect;
	OffsetRect(&zoomOutRect, -13, 0);
	zoomsRect = zoomOutRect;
	zoomsRect.bottom = zoomsRect.top + 32;
	zoomsRect.right = zoomsRect.left + 32;

	// deal with zoom in/out clicks
	if (TrackIn(&zoomInRect, clickPoint, &zoomsRect, kZoomControlRight, kZoomControlPlain))
		{
		somethingHit = true;
		SetZoom(pWindow, pData, FixedMultiply(((GXDataPtr)pData)->zoomFactor, ff(2)) );
		PlotIconID(&zoomsRect, ttNone, ttNone, kZoomControlPlain);
		}

	if (TrackIn(&zoomOutRect, clickPoint, &zoomsRect, kZoomControlLeft, kZoomControlPlain))
		{
		somethingHit = true;
		SetZoom(pWindow, pData, FixedDivide(((GXDataPtr)pData)->zoomFactor, ff(2)) );
		PlotIconID(&zoomsRect, ttNone, ttNone, kZoomControlPlain);
		}

	// deal with the options pop up
	if (PtInRect(clickPoint, &labelArea))
		{
		MenuHandle	popupMenu = GetMenu(kGXPopUpMenu);
		short		selectedItem;
		TextState	textState;
		Point		popPoint;
		char		bulletString[3];
		
		// figure out where to display the pop up
		popPoint.v = labelArea.top;
		popPoint.h = labelArea.left;
		LocalToGlobal(&popPoint);

		somethingHit = true;
		GetIntlTokenChar(tokenCenterDot, FontToScript(applFont), bulletString);
		
		// set up menu to be small sized
		TextFont(applFont);
		TextSize(9);
		DoUseWFont(&textState, pWindow, true);
		
		// set up the menu for selected items
		SetItemMark(popupMenu, iDontShowMargins, (((GXDataPtr)pData)->dontShowMargins) ? bulletString[1] : noMark);
		{
		ZoomTableEntry *pEntry = &gZoomTable[0];
		
		while (pEntry->menuItem != 0)
			{
			SetItemMark(popupMenu, pEntry->menuItem, (((GXDataPtr)pData)->zoomFactor == pEntry->zoomFactor) ? bulletString[1] : noMark);
			pEntry++;
			}
		}
		
		// conduct the menu
		InsertMenu(popupMenu, -1);
		selectedItem = PopUpMenuSelect(popupMenu, popPoint.v, popPoint.h, CountMenuItems(popupMenu)+1) & 0xFFFF;

		// restore menu sizes
		DoUseWFont(&textState, nil, false);
		DeleteMenu(kGXPopUpMenu);
		
		switch (selectedItem)
			{				
			// toggle show/hide margins
			case iDontShowMargins:
				// flip the boolean
				((GXDataPtr)pData)->dontShowMargins = 1 - ((GXDataPtr)pData)->dontShowMargins;
				
				// force update and recalc the size of window
				InvalRect(&GetWindowPort(pWindow)->portRect);
				AdjustScrollBars(pWindow, true, true, nil);
				break;

			// scale graphics to fit the window
			case iScaleToFit:
				{
				gxRectangle		pageSize, paperSize;
				Fixed			horizScale, vertScale;
				GrafPtr			pPort = (GrafPtr)GetWindowPort(pWindow);
				
				GetCurrentPageAndPaper(pData, &pageSize, &paperSize);
				
				pageSize.left = pageSize.top = 0;
				pageSize.right = ff(pPort->portRect.right - kScrollBarSize);
				pageSize.bottom = ff(pPort->portRect.bottom - kScrollBarSize);
				
				horizScale = FixedDivide(pageSize.right - pageSize.left, paperSize.right - paperSize.left);
				vertScale = FixedDivide(pageSize.bottom - pageSize.top, paperSize.bottom - paperSize.top);
				if (horizScale > vertScale)
					SetZoom(pWindow, pData, vertScale);
				else
					SetZoom(pWindow, pData, horizScale);
				}
				break;
				
			// absolute set scale cases
			default:
				{
				ZoomTableEntry *pEntry = &gZoomTable[0];
				
				while (pEntry->menuItem != 0)
					{
					if (selectedItem == pEntry->menuItem)
						SetZoom(pWindow, pData, pEntry->zoomFactor);
					pEntry++;
					}
				}
				break;
			}
			
		}
		
	// deal with the tool pop up
	if (PtInRect(clickPoint, &toolArea))
		{
		MenuHandle	popupMenu = GetMenu(kGXToolMenu);
		short		selectedItem;
		Point		popPoint;
		short		i, numItems;
		
		// figure out where to display the pop up
		popPoint.v = toolArea.top;
		popPoint.h = toolArea.left;
		LocalToGlobal(&popPoint);

		// we've processed the mouse click
		somethingHit = true;
		
		// select our current item in the menu
		numItems = CountMenuItems(popupMenu);
		for (i = 1; i <= numItems; ++i)
			MacCheckMenuItem(popupMenu, i, (i == ((GXDataPtr)pData)->contentClickMode) );
		
		// conduct the menu
		InsertMenu(popupMenu, -1);
		selectedItem = PopUpMenuSelect(popupMenu, popPoint.v, popPoint.h, numItems+1) & 0xFFFF;
		DeleteMenu(kGXPopUpMenu);

		// remember the item selected
		if (selectedItem != 0)
			((GXDataPtr)pData)->contentClickMode = selectedItem;
			
		// invalidation the tool picture
		InvalRect(&toolArea);
		
		if (selectedItem != kSelectionTool)
			{
			// erase the old selection
			DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, false);
	
			// clear the selection
			((GXDataPtr)pData)->selectionRectangle.top 		= 0;
			((GXDataPtr)pData)->selectionRectangle.left		= 0;
			((GXDataPtr)pData)->selectionRectangle.bottom	= 0;
			((GXDataPtr)pData)->selectionRectangle.right	= 0;
	
			// draw the new selection
			DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, true);
			}
		}
		
	// deal with clicks in page controls
	if (((GXDataPtr)pData)->numberOfPages > 1)
		{
		Rect	leftArrowRect, rightArrowRect, arrowsRect;
		Rect	gotoPageRect;
		
		// calculate the minus one page arrow		
		leftArrowRect.top 		= infoArea.top + 2;
		leftArrowRect.bottom 	= leftArrowRect.top + 32;
		leftArrowRect.left 		= infoArea.left + 2;
		leftArrowRect.right 	= leftArrowRect.left + 11;
				
		// calculate the go to a particular page rect		
		gotoPageRect = leftArrowRect;
		OffsetRect(&gotoPageRect, 11, 0);
		gotoPageRect.right --;

		// calculate the plus one page arrow		
		rightArrowRect = gotoPageRect;
		OffsetRect(&rightArrowRect, 10, 0);
		
		// calculate sum of all areas
		arrowsRect = leftArrowRect;
		arrowsRect.left 	= infoArea.left + 2;
		arrowsRect.right 	= arrowsRect.left + kPageControlsWidth;
					
		if (TrackIn(&leftArrowRect, clickPoint, &arrowsRect, kPageControlLeft, kPageControlPlain))
			{
			somethingHit = true;
			if (((GXDataPtr)pData)->currentPage > 1) 
				anErr = GXCommand(pWindow, pData, cPreviousPage, 0);
			else
				PlotIconID(&arrowsRect, ttNone, ttNone, kPageControlPlain);
			}

		if (TrackIn(&rightArrowRect, clickPoint, &arrowsRect, kPageControlRight, kPageControlPlain))
			{
			somethingHit = true;
			if (((GXDataPtr)pData)->currentPage < ((GXDataPtr)pData)->numberOfPages)
				anErr = GXCommand(pWindow, pData, cNextPage, 0);
			else
				PlotIconID(&arrowsRect, ttNone, ttNone, kPageControlPlain);
			}
			
		if (PtInRect(clickPoint, &gotoPageRect))
			{
			unsigned long	actualTicks;
			
			somethingHit = true;
			// pause, then check if the mouse is still down
			Delay(20, &actualTicks);
			
			// if still down, track preview window
			if (StillDown())
				{
				WindowPtr	popWindow;
				Rect		windowRect;
				short		oldResFile = CurResFile();
				
				// put the print file on top
				UseResFile(((GXDataPtr)pData)->printFileRefNum);
				
				// figure out where to place the pop up window, and then make it
				windowRect.bottom 	= arrowsRect.top - 2;
				windowRect.left		= 2;
				if (Count1Resources(kProxyType) > 0)
					windowRect.top = windowRect.bottom - kPopUpWindowHeightLarge;
				else
					windowRect.top = windowRect.bottom - kPopUpWindowHeightSmall;
				windowRect.right 	= windowRect.left + kScrollAreaWidth + kPageControlsWidth + kZoomControlsWidth + kToolControlWidth - 4;
					
				LocalToGlobal(&TopLeft(windowRect));				
				LocalToGlobal(&BotRight(windowRect));				
				
				popWindow = NewCWindow(nil, &windowRect, "\p", true, plainDBox, (WindowPtr)-1, false, 0);
				if (popWindow)
					{			
					long	oldValue = ((GXDataPtr)pData)->currentPage;
					long	newValue = oldValue;
					GrafPtr	popPort = (GrafPtr)GetWindowPort(popWindow);
					
					// draw initial location of the value
					SetPort(popPort);
					DrawPageSliderAndThumb(popWindow, oldValue, ((GXDataPtr)pData)->numberOfPages);
										
					// track the mouse, updating the value as we go
					while (StillDown())
						{
						GetMouse(&clickPoint);
						if (PtInRect(clickPoint, &popPort->portRect))
							{
							newValue = 	clickPoint.h
											*
										((GXDataPtr)pData)->numberOfPages
											/
										(popPort->portRect.right - popPort->portRect.left) 
											+ 1;
										
							if (oldValue != newValue)
								{
								oldValue = newValue;
								DrawPageSliderAndThumb(popWindow, oldValue, ((GXDataPtr)pData)->numberOfPages);
								}
							}
						}
						
					// and done with the pop up window, return to main window
					DisposeWindow(popWindow);
					SetPort((GrafPtr)GetWindowPort(pWindow));
					
					// if we changed the value, make it so
					if (newValue != ((GXDataPtr)pData)->currentPage)
						{
						((GXDataPtr)pData)->currentPage = newValue;
						anErr = GetCurrentPage((GXDataPtr) pData, true);
						InvalRect(&GetWindowPort(pWindow)->portRect);
						}
					}
					
				// restore resource chain
				UseResFile(oldResFile);
				}
			else
				{
				// otherwise, do the goto dialog box
				anErr = GXCommand(pWindow, pData, cGotoPage, 0);
				}
				
			} // if (click in goto page area)
			
		} // if (> 1 page)

	// restore clip
	SetClip(oldClip);
	DisposeRgn(oldClip);
	
	// nothing matched so far, deal with selecting the contents
	if (!somethingHit)
		{
		Rect	selectionRect = ((GXDataPtr)pData)->selectionRectangle;
		
		
		OffsetRect(&selectionRect, -GetControlValue(pData->hScroll), -GetControlValue(pData->vScroll));
		if ( (gMachineInfo.haveDragMgr) && (PtInRect(clickPoint, &selectionRect)) )
			{
			((GXDataPtr)pData)->tempDragShape = nil;
			DragAndDropArea(pWindow, pData, pEvent, 
								&selectionRect);
				
								
			if ( ((GXDataPtr)pData)->tempDragShape )
				GXDisposeShape( ((GXDataPtr)pData)->tempDragShape );
			}
		else
			{
			LongRect		docRect;
			Rect			contentRect;
			
			contentRect = GetWindowPort(pWindow)->portRect;
			contentRect.right -= kScrollBarSize;
			contentRect.bottom -= kScrollBarSize;
			
			if (PtInRect(clickPoint, &contentRect))
				{
				switch (((GXDataPtr)pData)->contentClickMode)
					{
					case kSelectionTool:
						GXGetDocumentRect(pWindow, pData, &docRect, false);
						contentRect.top 	= docRect.top;
						contentRect.left 	= docRect.left;
						contentRect.bottom 	= docRect.bottom;
						contentRect.right 	= docRect.right;
						
						anErr = SelectContents(pWindow, pData, pEvent, 
											&((GXDataPtr)pData)->selectionRectangle, &contentRect, 
											&((GXDataPtr)pData)->patternPhase);
				
						// existing text selection? clear the highlight
						if (((GXDataPtr)pData)->currentSelectionShape)
							{
							InvalRect(&GetWindowPort(pWindow)->portRect);
							
							ClearCurrentSelection((GXDataPtr)pData);
							}
						break;

					case kRedMarkerTool:
						DoDrawingClick(pWindow, pData, clickPoint, pEvent);
						break;
						
					} // switch (mode)
					
				} // click in content rect
			}
		}
		
	return(anErr);
		
} // GXContentClick

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXAdjustMenus(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	OSErr anErr = noErr;
	
	EnableCommand(cSaveAs);

	if (((GXDataPtr)pData)->numberOfPages > 1)
		{
		if (((GXDataPtr)pData)->currentPage < ((GXDataPtr)pData)->numberOfPages)
			EnableCommand(cNextPage);
			
		if (((GXDataPtr)pData)->currentPage > 1)
			EnableCommand(cPreviousPage);
		
		EnableCommand(cGotoPage);
		}
		
	if (!EmptyRect( &((GXDataPtr)pData)->selectionRectangle) )
		EnableCommand(cCopy);

	{
	LongRect		docRect;
	Rect			shortDocRect;
	
	// find out the size of the document			
	GXGetDocumentRect(pWindow, pData, &docRect, false);
	LongRectToRect(&docRect, &shortDocRect);
	if 	(EqualRect(&shortDocRect, &((GXDataPtr)pData)->selectionRectangle))
		ChangeCommandName(cSelectAll, kMiscStrings, iSelectNoneCommand);
	else
		ChangeCommandName(cSelectAll, kMiscStrings, iSelectAllCommand);
	}
	EnableCommand(cSelectAll);
	
	EnableCommand(cFind);
	if (gFindString[0] != 0)
		EnableCommand(cFindAgain);
	
	return(anErr);
	
} // GXAdjustMenus

// --------------------------------------------------------------------------------------------------------------

OSErr	GXCommand(WindowPtr pWindow, WindowDataPtr pData, short commandID, long menuResult)
{
#pragma unused (menuResult)

	OSErr	anErr = noErr;
		
	switch (commandID)
		{
		case cSave:
			GXSavePrintFile(((GXDataPtr)pData)->thePrintFile, nil);
			anErr = GXGetJobError(pData->hPrint);
			if (anErr == noErr)
				anErr = LoadOrSaveAnnotations(pData, kSaveAnnotations);
				
			// if everything went okay, then clear the changed bit
			if (anErr == noErr)
				pData->changed = false;
			break;
			
		case cSaveAs:
			anErr = GXSaveAs(pWindow, pData);
			if (anErr == noErr)
				anErr = LoadOrSaveAnnotations(pData, kSaveAnnotations);

			// if everything went okay, then clear the changed bit
			if (anErr == noErr)
				pData->changed = false;
			break;
			
		case cFind:
			if (ConductFindOrReplaceDialog(kFindWindowID) == cancel)	
				break;
			
			// start search at top of page
			((GXDataPtr)pData)->currentShapeIndex = 0;
			
		// fall through from find
		case cFindAgain:
			{
			Boolean	isBackwards = ((gEvent.modifiers & shiftKey) != 0);
			
			SetWatchCursor();

			if (!PerformNextFind(pWindow, pData, gFindString, gCaseSensitive, isBackwards, gWrapAround))
				SysBeep(1);
			else
				ScrollFoundShapeIntoView(pWindow, pData);
				
			SetCursor(&qd.arrow);
			}
			break;

		case cCopy:
			{
			gxShape			cullShape = GetSelectedShape(pData);

			if (cullShape)
				{
				// done with the shape now
				ShapeToScrap(cullShape);
				GXDisposeShape(cullShape);
				}
			}
			break;
		
		case cSelectAll:
			{
			LongRect		docRect;
			Rect			shortDocRect;
			
			// find out the size of the document			
			GXGetDocumentRect(pWindow, pData, &docRect, false);

			// erase the old selection
			DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, false);
			
			LongRectToRect(&docRect, &shortDocRect);
			if 	(EqualRect(&shortDocRect, &((GXDataPtr)pData)->selectionRectangle))
				{
				((GXDataPtr)pData)->selectionRectangle.top 		= 0;
				((GXDataPtr)pData)->selectionRectangle.left		= 0;
				((GXDataPtr)pData)->selectionRectangle.bottom	= 0;
				((GXDataPtr)pData)->selectionRectangle.right	= 0;
				}
			else
				{
				((GXDataPtr)pData)->selectionRectangle.top 		= docRect.top;
				((GXDataPtr)pData)->selectionRectangle.left		= docRect.left;
				((GXDataPtr)pData)->selectionRectangle.bottom	= docRect.bottom;
				((GXDataPtr)pData)->selectionRectangle.right	= docRect.right;
				}

			// draw the new selection
			DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, true);
			
			DoAdjustCursor(pWindow, nil);

			}
			break;

		case cPageSetup:
			DoPageSetup(pWindow);
			anErr = GetCurrentPage((GXDataPtr) pData, false);
			InvalRect(&GetWindowPort(pWindow)->portRect);
			anErr = eActionAlreadyHandled;
			break;
			
		case cNextPage:
			((GXDataPtr)pData)->currentPage++;
			anErr = GetCurrentPage((GXDataPtr) pData, true);
			InvalRect(&GetWindowPort(pWindow)->portRect);
			if (anErr == noErr)
				anErr = eActionAlreadyHandled;
			break;
		
		case cPreviousPage:
			((GXDataPtr)pData)->currentPage--;
			anErr = GetCurrentPage((GXDataPtr) pData, true);
			InvalRect(&GetWindowPort(pWindow)->portRect);
			if (anErr == noErr)
				anErr = eActionAlreadyHandled;
			break;
		
		case cGotoPage:
			switch (menuResult)
				{
				case cGotoFirst:
					((GXDataPtr)pData)->currentPage = 1;
					anErr = GetCurrentPage((GXDataPtr) pData, true);
					InvalRect(&GetWindowPort(pWindow)->portRect);
					break;

				case cGotoLast:
					((GXDataPtr)pData)->currentPage = ((GXDataPtr)pData)->numberOfPages;
					anErr = GetCurrentPage((GXDataPtr) pData, true);
					InvalRect(&GetWindowPort(pWindow)->portRect);
					break;
					
				default:
					{
					DialogPtr	dPtr;
					short		hit;
					
					dPtr = GetNewDialog(kGotoPageDialogID, nil, (WindowPtr)-1);
					if (dPtr)
						{
						short	theType;
						Handle	theHandle;
						Rect	theRect;
						Str255	theString;
						
						GetDialogItem(dPtr, 4, &theType, &theHandle, &theRect);
						NumToString(((GXDataPtr)pData)->currentPage, theString);
						SetDialogItemText(theHandle, theString);
						SelectDialogItemText(dPtr, 4, 0, 32767);

						NumToString(((GXDataPtr)pData)->numberOfPages, theString);
						ParamText(theString, "\p", "\p", "\p");
						
						SetDialogDefaultItem(dPtr, ok);
						SetDialogCancelItem(dPtr, cancel);
						BeginMovableModal();
						
						do
							{
							MovableModalDialog(nil, &hit);
							
							if (hit == ok)
								{
								long	tempLong;
								
								// convert to a page number, find and report errors
								GetDialogItemText(theHandle, theString);
								StringToNum(theString, &tempLong);
								if (tempLong < 1) 
									{
									SysBeep(1);
									tempLong = 1;
									hit = 0;
									}
								if (tempLong > ((GXDataPtr)pData)->numberOfPages)
									{
									tempLong = ((GXDataPtr)pData)->numberOfPages;
									hit = 0;
									}
									
								// if we have an error, we try again, otherwise we go to the page
								if (hit == 0)
									{
									SysBeep(1);
									NumToString(tempLong, theString);
									SetDialogItemText(theHandle, theString);
									SelectDialogItemText(dPtr, 4, 0, 32767);
									}
								else
									{
									((GXDataPtr)pData)->currentPage = tempLong;
									anErr = GetCurrentPage((GXDataPtr) pData, true);
									InvalRect(&GetWindowPort(pWindow)->portRect);
									}
								}
							} while ((hit != ok) && (hit != cancel));
							
						DisposeDialog(dPtr);
						EndMovableModal();
						}
						
					}
					break;
					
				}
			if (anErr == noErr)
				anErr = eActionAlreadyHandled;
			break;
		
		}
	
	return(anErr);
	
} // GXCommand

// --------------------------------------------------------------------------------------------------------------
static OSErr	GXFilePrintPage(WindowPtr pWindow, WindowDataPtr pData,
					Rect * pageRect, long *pageNum)
{
#pragma unused (pWindow, pageRect)

	OSErr		anErr = noErr;
	gxShape 	thisShape;
	gxFormat 	thisFormat;
	
	GXReadPrintFilePage(((GXDataPtr)pData)->thePrintFile, *pageNum, 0, nil, &thisFormat, &thisShape);
	anErr = GXGetJobError(pData->hPrint);
	nrequire(anErr, ReadPrintFilePage);

	GXPrintPage(pData->hPrint, *pageNum, thisFormat, thisShape);
	anErr = GXGetJobError(pData->hPrint);
	nrequire(anErr, PrintPage);

	GXDisposeFormat(thisFormat);
	GXDisposeShape(thisShape);

// FALL THROUGH EXCEPTION HANDLING
PrintPage:
ReadPrintFilePage:
	// tell it to stop printing when we reach the end
	if (*pageNum >= ((GXDataPtr)pData)->numberOfPages)
		*pageNum = -1;
	
	return(anErr);
	
} // GXFilePrintPage

// --------------------------------------------------------------------------------------------------------------

OSErr	GXGetDocumentRect(WindowPtr pWindow, WindowDataPtr pData, 
			LongRect * documentRectangle, Boolean forGrow)
{
#pragma unused (pWindow, forGrow)

	gxRectangle		pageSize, paperSize;
	
	GetCurrentPageAndPaper(pData, &pageSize, &paperSize);

	documentRectangle->left = 0;
	documentRectangle->top = 0;
	documentRectangle->bottom = FixedMultiply(paperSize.bottom - paperSize.top, ((GXDataPtr)pData)->zoomFactor) >> 16;
	documentRectangle->right = FixedMultiply(paperSize.right - paperSize.left, ((GXDataPtr)pData)->zoomFactor) >> 16;
		
	return(noErr);
	
} // GXGetDocumentRect

// --------------------------------------------------------------------------------------------------------------

static long GXCalculateIdleTime(WindowPtr pWindow, WindowDataPtr pData)
{
#pragma unused (pWindow)

	if (!EmptyRect( &((GXDataPtr)pData)->selectionRectangle))
		return(0);
	else
		return(kMaxWaitTime);
		
} // GXCalculateIdleTime

// --------------------------------------------------------------------------------------------------------------

static Boolean	GXFilterEvent(WindowPtr pWindow, WindowDataPtr pData, EventRecord *pEvent)
{
	if 	(
		(!gMachineInfo.amInBackground) &&
		(pEvent->what == nullEvent) &&
		(pWindow == FrontNonFloatingWindow()) &&
		(EmptyRgn( ((WindowPeek)pWindow)->updateRgn)) &&
		(MOVESELECTION(pEvent->when) )
		)
		{
		// erase the old
		DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, false);
		
		// draw the new, moving onto the next pattern
		DrawSelection(pData, &((GXDataPtr)pData)->selectionRectangle, &((GXDataPtr)pData)->patternPhase, true);
		}
		
	return(false);
	
} // GXFilterEvent

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXDragAddFlavors(WindowPtr pWindow, WindowDataPtr pData, DragReference theDragRef)
{
#pragma unused (pWindow)

	OSErr	anErr = noErr;
	
	SetDragSendProc(theDragRef, gGXSendDataProc, pData);
	AddDragItemFlavor(theDragRef, 1, 'qdgx', nil, 0, 0);
	AddDragItemFlavor(theDragRef, 1, 'PICT', nil, 0, 0);
	
	return(anErr);
	
} // GXDragAddFlavors

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXAdjustCursor(WindowPtr pWindow, WindowDataPtr pData, Point * localMouse, RgnHandle globalRgn)
{
#pragma unused (pWindow)

	OSErr		anErr = noErr;
	Handle		theCursor;
	Rect		selectionRect = ((GXDataPtr)pData)->selectionRectangle;
	Rect		globalSelection;
	
	OffsetRect(&selectionRect, -GetControlValue(pData->hScroll), -GetControlValue(pData->vScroll));
	
	globalSelection = selectionRect;
	LocalToGlobal(&TopLeft(globalSelection));
	LocalToGlobal(&BotRight(globalSelection));
	
	if (!PtInRect(*localMouse, &selectionRect) )
		{
		short	cursorID;
		Boolean	colorCursor;
		
		cursorID = ((GXDataPtr)pData)->contentClickMode;
		if (cursorID == kSelectionTool)
			{
			colorCursor = false;
			cursorID = crossCursor;
			}
		else
			{
			colorCursor = true;
			cursorID += kIconBase;
			}
			
		if (colorCursor)
			theCursor = (Handle)GetCCursor(cursorID);
		else
			theCursor = (Handle)GetCursor(cursorID);
		if (theCursor)
			{
			if (colorCursor)
				{
				SetCCursor((CCrsrHandle)theCursor);
				DisposeCCursor((CCrsrHandle)theCursor);
				}
			else
				{
				char	oldState;
				
				oldState = HGetState(theCursor);
				HLock((Handle) theCursor);
				SetCursor(*(CursHandle)theCursor);
				HSetState(theCursor, oldState);
				}
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
	
} // GXAdjustCursor

// --------------------------------------------------------------------------------------------------------------

static OSErr	GXMakeWindow(WindowPtr pWindow, WindowDataPtr pData)
{
	OSErr				anErr = noErr;
	
	pData->pCloseWindow 		= (CloseWindowProc)			GXCloseWindow;
	pData->pAdjustMenus 		= (AdjustMenusProc)			GXAdjustMenus;
	pData->pCommand				= (CommandProc)				GXCommand;
	pData->pUpdateWindow 		= (UpdateWindowProc)		GXUpdateWindow;
	pData->pContentClick 		= (ContentClickProc)		GXContentClick;
	pData->pGetDocumentRect 	= (GetDocumentRectProc)		GXGetDocumentRect;
	pData->pPrintPage		 	= (PrintPageProc)			GXFilePrintPage;
	pData->pFilterEvent		 	= (FilterEventProc)			GXFilterEvent;
	pData->pCalculateIdleTime	= (CalculateIdleTimeProc)	GXCalculateIdleTime;
	pData->pAdjustCursor		= (AdjustCursorProc)		GXAdjustCursor;
	pData->pDragAddFlavors		= (DragAddFlavorsProc)		GXDragAddFlavors;
	
	pData->documentOutputsGX	= true;
	pData->hasGrow				= true;
	pData->minHSize				= kMinGXDocSize + kScrollAreaWidth + kZoomControlsWidth + kToolControlWidth;
	pData->hScrollAmount		= 10;
	pData->vScrollAmount		= 10;
	pData->hScrollOffset		= kScrollAreaWidth + kZoomControlsWidth + kToolControlWidth;
	
	// default the job info, which we need to have
	anErr = DoDefault(pData);
	nrequire(anErr, DoDefault);

	((GXDataPtr)pData)->thePrintFile = GXOpenPrintFile(pData->hPrint, &pData->fileSpec, fsRdWrPerm);
	anErr = GXGetJobError(pData->hPrint);
	nrequire(anErr, OpenPrintFile);

	/*
	 *	The assumption here is that the GXOpenPrintFile leaves the resfile on top.
	 *	We need this refnum so that any resource handles the translator gets will
	 *	match those that GXOpenPrintFile used for calls to GXNewFont.  See the call
	 *	to GXDrawShape in GXUpdateWindow
	 */
	((GXDataPtr)pData)->printFileRefNum = CurResFile();

	// close down other paths to the file -- because we don't need them
	if (pData->resRefNum != -1)
		{
		CloseResFile(pData->resRefNum);
		pData->resRefNum = -1;
		}
	if (pData->dataRefNum != -1)
		{
		FSClose(pData->dataRefNum);
		pData->dataRefNum = -1;
		}
		
	// default to normal printing -- so if the user prints, we go to normal mode at first
	{
	Collection	jobCollection = GXGetJobCollection(pData->hPrint);
	gxJobInfo	theInfo;
	long		theSize = sizeof(theInfo);
	
	if (GetCollectionItem(jobCollection, gxJobTag, gxPrintingTagID, &theSize, &theInfo) == noErr)
		{
		theInfo.priority = gxPrintJobASAP;
		AddCollectionItem(jobCollection, gxJobTag, gxPrintingTagID, theSize, &theInfo);
		}
	
	}
	
	// by default we have no pages
	((GXDataPtr)pData)->numberOfPages		= GXCountPrintFilePages(((GXDataPtr)pData)->thePrintFile);
	((GXDataPtr)pData)->currentPage			= 1;
	((GXDataPtr)pData)->zoomFactor			= ff(1);
	((GXDataPtr)pData)->contentClickMode	= kSelectionTool;
	anErr = GXGetJobError(pData->hPrint);
	if ( (anErr == noErr) && (((GXDataPtr)pData)->numberOfPages == 0) )
		anErr = eDocumentContainsNoPages;
	nrequire(anErr, GXCountPrintFilePages);
	
	if (((GXDataPtr)pData)->numberOfPages > 1)
		pData->hScrollOffset += kPageControlsWidth;
		
	((GXDataPtr)pData)->pageAnnotations = (gxShape**) NewHandleClear( (((GXDataPtr)pData)->numberOfPages * sizeof(gxShape)) );
	anErr = MemError();
	if (anErr == noErr)
		anErr = LoadOrSaveAnnotations(pData, kLoadAnnotations);
	nrequire(anErr, FailedToAllocateAnnotation);
	
	// set up so we can draw inside of this window
	((GXDataPtr)pData)->parentViewPort = GXNewWindowViewPort((WindowPtr)pWindow);
	((GXDataPtr)pData)->childViewPort = GXNewViewPort(gxScreenViewDevices);
	GXSetViewPortParent(((GXDataPtr)pData)->childViewPort, ((GXDataPtr)pData)->parentViewPort);
	GXSetViewPortAttributes(((GXDataPtr)pData)->childViewPort, gxAlwaysGridPort);
	GXSetViewPortDither(((GXDataPtr)pData)->childViewPort, 4);
	
	// fetch the current page
	anErr = GetCurrentPage((GXDataPtr) pData, true);
	nrequire(anErr, GetCurrentPage);
	
	// if this document contains PostScript do a warning
	{
	gxTranslatedDocumentInfo theInfo;
	long theSize = sizeof(theInfo);
	
	if 	(
		(GetCollectionItem(GXGetJobCollection(pData->hPrint),
							gxTranslatedDocumentTag,
							gxPrintingTagID,
							&theSize,
							&theInfo
							) == noErr) &&
		(theInfo.translatorInfo & gxContainsPostScript)
		)
		ConductErrorDialog(eDocumentContainsPS, cOpen, ok);
	}
	
	return(anErr);
	
// EXCEPTION HANDLING
GetCurrentPage:
FailedToAllocateAnnotation:
GXCountPrintFilePages:
	GXClosePrintFile( ((GXDataPtr)pData)->thePrintFile);
	
OpenPrintFile:
	GXDisposeJob(pData->hPrint);
		
DoDefault:
	return(anErr);
	
} // GXMakeWindow


// --------------------------------------------------------------------------------------------------------------

OSErr	GXPreflightWindow(PreflightPtr pPreflightData)
{	
	pPreflightData->continueWithOpen 	= true;
	pPreflightData->wantVScroll			= true;
	pPreflightData->wantHScroll			= true;
	pPreflightData->doZoom				= true;
	pPreflightData->makeProcPtr 		= GXMakeWindow;
	pPreflightData->storageSize 		= sizeof(GXDataRecord);
	
	return(noErr);
	
} // GXPreflightWindow

// --------------------------------------------------------------------------------------------------------------

void GXGetFileTypes(OSType * pFileTypes, OSType * pDocumentTypes, short * numTypes)
{
	if (gMachineInfo.haveGXPrinting)
		{
		pFileTypes[*numTypes] 		= 'sjob';
		pDocumentTypes[*numTypes] 	= kGXWindow;
		(*numTypes)++;

		pFileTypes[*numTypes] 		= 'tjob';
		pDocumentTypes[*numTypes] 	= kGXWindow;
		(*numTypes)++;

		pFileTypes[*numTypes] 		= 'rjob';
		pDocumentTypes[*numTypes] 	= kGXWindow;
		(*numTypes)++;

		pFileTypes[*numTypes] 		= 'qjob';
		pDocumentTypes[*numTypes] 	= kGXWindow;
		(*numTypes)++;
		}
		
} // GXGetFileTypes
