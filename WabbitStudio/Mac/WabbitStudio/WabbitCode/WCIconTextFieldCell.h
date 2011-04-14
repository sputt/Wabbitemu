//
//  WCIconTextFieldCell.h
//  WabbitStudio
//
//  Created by William Towe on 3/18/11.
//  Copyright 2011 Revolution Software. All rights reserved.
//

#import "RSVerticallyCenteredTextFieldCell.h"


@interface WCIconTextFieldCell : RSVerticallyCenteredTextFieldCell <NSCopying> {
@private
    NSImage *_icon;
	NSSize _iconSize;
}
@property (retain, nonatomic) NSImage *icon;
@property (assign, nonatomic) NSSize iconSize;

@end