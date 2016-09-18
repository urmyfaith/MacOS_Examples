/*
	Created by: nwg
	
	Copyright: 	� Copyright 2002-2003 Apple Computer, Inc. All rights reserved.
	
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

	$Log: BrowserController.h,v $
	Revision 1.2  2003/05/27 17:46:59  firewire
	SDK16
	
	Revision 1.1  2002/11/07 00:36:18  noggin
	move to new repository
	
	Revision 1.3  2002/07/18 22:04:17  noggin
	added spiffy outline tree lines
	
	Revision 1.2  2002/07/16 22:07:12  noggin
	now have NSData view and new icon
	
	Revision 1.1  2002/07/15 16:09:08  noggin
	moved files around. made properties inspector into a drawer.
	
	Revision 1.5  2002/05/30 18:35:49  noggin
	Fixed a few bugs. First FireWire bus view implemented.
	
	Revision 1.4  2002/05/23 19:54:04  noggin
	added CVS Logging
	
*/

#import <Cocoa/Cocoa.h>

@class OutlineItem ;
@class InspectorController ;
@interface BrowserController : NSObject
{
    IBOutlet NSOutlineView*			mRegistryOutlineView ;
	IBOutlet NSTextField*			mFindTextEditText ;
	IBOutlet NSStepper*				mFindStepper ;
	IBOutlet NSDrawer*				mPropertiesDrawer ;
	IBOutlet NSPopUpButton*			mPlanePopup ;
	IBOutlet InspectorController*	mInspectorController ;
	
	char						mCurrentPlane[80] ;
	NSMutableDictionary*		mPlanes ;
}

// IBActions
-(IBAction)newFind:(id)sender ;
-(IBAction)stepFind:(id)sender ;
-(IBAction)changePlanes:(id)sender ;
-(IBAction)toggleDrawer:(id)sender ;

-(void)doFind:(NSString*)findText startingItem:(OutlineItem*)startingItem forward:(BOOL)forward ;
- (void)goToFind ;
- (void)findNext ;
- (void)findPrev ;

// NSWindow delegate methods
- (void)windowWillClose:(NSNotification *)notification;

// combo box delegate methods
- (void)comboBoxSelectionDidChange:(NSNotification *)notification;

// outline view delegate methods
- (void)outlineViewSelectionDidChange:(NSNotification *)notification ;
-(void)registryChanged:(id)sender ;

@end
