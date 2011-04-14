//
//  WCFile.h
//  WabbitStudio
//
//  Created by William Towe on 3/17/11.
//  Copyright 2011 Revolution Software. All rights reserved.
//

#import "WCTreeNode.h"


extern NSString* const kWCFileAssemblyUTI;
extern NSString* const kWCFileIncludeUTI;
extern NSString* const kWCFilePanicCodaImportedUTI;

extern NSString* const kWCFileHasUnsavedChangesNotification;

extern NSString* const kWCFileNumberOfErrorMessagesChangedNotification;
extern NSString* const kWCFileNumberOfWarningMessagesChangedNotification;

extern NSString *const kWCFileNameDidChangeNotification;

@class WCTextStorage,WCSymbolScanner,WCProject,WCBuildMessage,WCAlias;

@interface WCFile : WCTreeNode <NSCoding,NSCopying,NSTextViewDelegate> {
@private
	WCAlias *_alias; // tracks the file we represent
	NSString *_UUID; // for storing in project settings plist
	
	WCTextStorage *_textStorage; // text storage that stores the text of the file we represent
	NSUndoManager *_undoManager; // undo manager for any text views that hook up to our text storage
	NSInteger _changeCount; // so we know when we have unsaved changes
	NSTextView *_textViewForFindInProjectReplace;
	NSStringEncoding _encoding; // encoding we should use when reading and writing the file contents
	
	WCSymbolScanner *_symbolScanner; // parses the text of our file and gathers information about labels, equates, etc
	
	__weak WCProject *_project; // back pointer to our project, if we have one
	
	NSMutableDictionary *_lineNumbersToErrorMessages; // NSNumber instances pointing to WCBuildMessages
	NSMutableDictionary *_lineNumbersToWarningMessages;
}
@property (readonly,nonatomic) NSString *UUID;
@property (copy,nonatomic) NSURL *URL;
@property (readonly,nonatomic) BOOL isDirectory;
@property (readonly,nonatomic) NSString *absolutePath;
@property (readonly,nonatomic) NSURL *directoryURL;
@property (readonly,nonatomic) WCTextStorage *textStorage;
@property (readonly,nonatomic) NSUndoManager *undoManager;
@property (readonly,nonatomic) BOOL hasUnsavedChanges;
@property (readonly,nonatomic) NSString *UTI;
@property (readonly,nonatomic) WCSymbolScanner *symbolScanner;
@property (assign,nonatomic) WCProject *project;
@property (readonly,nonatomic) BOOL canEditName;
@property (readonly,nonatomic) WCAlias *alias;
@property (readonly,nonatomic) NSTextView *textViewForFindInProjectReplace;
@property (assign,nonatomic) NSStringEncoding encoding;
@property (assign,nonatomic) NSInteger changeCount;

+ (id)fileWithURL:(NSURL *)url;
- (id)initWithURL:(NSURL *)url;

+ (id)fileWithURL:(NSURL *)url name:(NSString *)name;
- (id)initWithURL:(NSURL *)url name:(NSString *)name;

- (BOOL)saveFile:(NSError **)outError;
- (BOOL)resetFile:(NSError **)outError;

- (void)addErrorMessage:(WCBuildMessage *)error;
- (void)addWarningMessage:(WCBuildMessage *)warning;
- (WCBuildMessage *)errorMessageAtLineNumber:(NSUInteger)lineNumber;
- (WCBuildMessage *)warningMessageAtLineNumber:(NSUInteger)lineNumber;
- (void)removeAllErrorMessages;
- (void)removeAllWarningMessages;
- (void)removeAllBuildMessages;
- (NSArray *)allErrorMessages;
- (NSArray *)allWarningMessages;
- (NSUInteger)lineStartForBuildMessage:(WCBuildMessage *)message;
@end