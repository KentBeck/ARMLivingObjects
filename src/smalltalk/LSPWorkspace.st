"LSPWorkspace — open-document registry for LSP features"
"Instance variables: documents tally"

Object subclass: #LSPWorkspace instanceVariableNames: 'documents tally'

!LSPWorkspace methodsFor: 'initialization'!
initialize
    documents := Array new: 16.
    tally := 0.
    ^ self
!

!LSPWorkspace methodsFor: 'accessing'!
size
    ^ tally
!

documentAtUri: aUri
    | index |
    index := self indexOfUri: aUri startingAt: 1.
    ^ index = 0
        ifTrue: [nil]
        ifFalse: [documents at: index]
!

methodSpanAtClass: aClassName selector: aSelector
    ^ self methodSpanAtClass: aClassName selector: aSelector startingAt: 1
!

!LSPWorkspace methodsFor: 'updating'!
openDocumentUri: aUri text: aText version: aVersion
    | document index |
    index := self indexOfUri: aUri startingAt: 1.
    document := index = 0
        ifTrue: [LSPDocument uri: aUri text: aText]
        ifFalse: [(documents at: index) updateText: aText version: aVersion].
    index = 0
        ifTrue: [
            self ensureStorage.
            tally := tally + 1.
            documents at: tally put: document
        ]
        ifFalse: [documents at: index put: document].
    aVersion = 1
        ifTrue: [0]
        ifFalse: [document updateText: aText version: aVersion].
    self reindexDocument: document.
    ^ document
!

updateDocumentUri: aUri text: aText version: aVersion
    ^ self openDocumentUri: aUri text: aText version: aVersion
!

reindexDocument: aDocument
    | spans |
    spans := LSPChunkParser on: aDocument text.
    spans := spans parseMethodSpans.
    ^ self recordSpans: spans on: aDocument at: 1
!

!LSPWorkspace methodsFor: 'private'!
ensureStorage
    (tally < documents size)
        ifTrue: [^ self]
        ifFalse: [self growStorage]
!

growStorage
    | old newStorage |
    old := documents.
    newStorage := Array new: old size + old size.
    self copyDocumentsFrom: old to: newStorage startingAt: 1.
    documents := newStorage.
    ^ self
!

copyDocumentsFrom: oldStorage to: newStorage startingAt: index
    index > tally
        ifTrue: [^ self]
        ifFalse: [
            newStorage at: index put: (oldStorage at: index).
            ^ self copyDocumentsFrom: oldStorage to: newStorage startingAt: index + 1
        ]
!

indexOfUri: aUri startingAt: index
    index > tally
        ifTrue: [^ 0]
        ifFalse: [
            (((documents at: index) uri) = aUri)
                ifTrue: [^ index]
                ifFalse: [^ self indexOfUri: aUri startingAt: index + 1]
        ]
!

recordSpans: spans on: aDocument at: index
    index > spans size
        ifTrue: [^ aDocument]
        ifFalse: [
            aDocument recordMethodSpan: (spans at: index).
            ^ self recordSpans: spans on: aDocument at: index + 1
        ]
!

methodSpanAtClass: aClassName selector: aSelector startingAt: index
    | span |
    index > tally
        ifTrue: [^ nil]
        ifFalse: [0].
    span := (documents at: index) methodSpanAtClass: aClassName selector: aSelector.
    span isNil
        ifTrue: [^ self methodSpanAtClass: aClassName selector: aSelector startingAt: index + 1]
        ifFalse: [^ span]
!
