//
//  DisplayData.m
//  GLImageSplitter
//
//  Created on 3/6/06.
//
//	Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
//	Computer, Inc. ("Apple") in consideration of your agreement to the
//	following terms, and your use, installation, modification or
//	redistribution of this Apple software constitutes acceptance of these
//	terms.  If you do not agree with these terms, please do not use,
//	install, modify or redistribute this Apple software.
//
//	In consideration of your agreement to abide by the following terms, and
//	subject to these terms, Apple grants you a personal, non-exclusive
//	license, under Apple's copyrights in this original Apple software (the
//	"Apple Software"), to use, reproduce, modify and redistribute the Apple
//	Software, with or without modifications, in source and/or binary forms;
//	provided that if you redistribute the Apple Software in its entirety and
//	without modifications, you must retain this notice and the following
//	text and disclaimers in all such redistributions of the Apple Software. 
//	Neither the name, trademarks, service marks or logos of Apple Computer,
//	Inc. may be used to endorse or promote products derived from the Apple
//	Software without specific prior written permission from Apple.  Except
//	as expressly stated in this notice, no other rights or licenses, express
//	or implied, are granted by Apple herein, including but not limited to
//	any patent rights that may be infringed by your derivative works or by
//	other works in which the Apple Software may be incorporated.
//
//	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
//	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
//	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
//	FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
//	OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
//
//	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
//	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//	INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
//	MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
//	AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
//	STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
//	POSSIBILITY OF SUCH DAMAGE.
//
//	Copyright (c) 2006 Apple Computer, Inc., All rights reserved.
//

#import "DisplayData.h"

//This class handles the data for overridding display locations/sizes.

@implementation DisplayData

- init
{
	self=[super init];
	if(self)
	{
		displays=NULL;
		[self reset];
	}
	return self;
}

- (void)dealloc
{
	free(displays);
    [super dealloc];
}

- (id)tableView:(NSTableView*)tableView objectValueForTableColumn:(NSTableColumn*)tableColumn row:(int)row
{
	id value;
	NSRect rect=displays[row];
	if([[tableColumn identifier] isEqualToString:@"Display"])
	{
		value=[NSString stringWithFormat:@"%d",row];
	}
	else if([[tableColumn identifier] isEqualToString:@"x"])
	{
		value=[NSString stringWithFormat:@"%g",rect.origin.x];
	}
	else if([[tableColumn identifier] isEqualToString:@"y"])
	{
		value=[NSString stringWithFormat:@"%g",rect.origin.y];
	}
	else if([[tableColumn identifier] isEqualToString:@"width"])
	{
		value=[NSString stringWithFormat:@"%g",rect.size.width];
	}
	else if([[tableColumn identifier] isEqualToString:@"height"])
	{
		value=[NSString stringWithFormat:@"%g",rect.size.height];
	}
	else
	{
		value=nil;
	}
    return value;
}

- (void)tableView:(NSTableView*)tableView setObjectValue:object forTableColumn:(NSTableColumn*)tableColumn row:(int)row
{
	NSRect rect=displays[row];
	if([[tableColumn identifier] isEqualToString:@"x"])
	{
		rect.origin.x=[object floatValue];
	}
	else if([[tableColumn identifier] isEqualToString:@"y"])
	{
		rect.origin.y=[object floatValue];
	}
	else if([[tableColumn identifier] isEqualToString:@"width"])
	{
		rect.size.width=[object floatValue];
	}
	else if([[tableColumn identifier] isEqualToString:@"height"])
	{
		rect.size.height=[object floatValue];
	}
	else
	{
	}
	displays[row]=rect;
}

- (int)numberOfRowsInTableView:(NSTableView*)tableView
{
    return numberOfDisplays;
}

//Remake the display data with the default settings from the display manager
- (void)reset
{
	NSArray* screens=[NSScreen screens];

	numberOfDisplays=[screens count];
	//size the array
	displays=(NSRectArray)realloc(displays,numberOfDisplays*sizeof(NSRect));

	int onDisplay;
	for(onDisplay=0;onDisplay<[screens count];onDisplay++)
	{
		NSRect frame=[[screens objectAtIndex:onDisplay] frame];
		displays[onDisplay]=frame;
	}
}

- (NSRectArray)displays
{
	return displays;
}

- (int)numberOfDisplays
{
	return numberOfDisplays;
}

@end
