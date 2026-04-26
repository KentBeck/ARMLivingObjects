"LSPDocument — open source buffer with cached parse result and source index"
"Instance variables: uri text version ast sourceIndex"

Object subclass: #LSPDocument instanceVariableNames: 'uri text version ast sourceIndex'

!LSPDocument class methodsFor: 'instance creation'!
uri: aUri text: aText
    ^ self new initializeUri: aUri text: aText
!

!LSPDocument methodsFor: 'initialization'!
initializeUri: aUri text: aText
    uri := aUri.
    text := aText.
    version := 1.
    ast := nil.
    sourceIndex := LSPSourceIndex new initialize.
    ^ self
!

!LSPDocument methodsFor: 'accessing'!
uri
    ^ uri
!

text
    ^ text
!

version
    ^ version
!

ast
    ^ ast
!

sourceIndex
    ^ sourceIndex
!

methodSpanAtClass: aClassName selector: aSelector
    ^ sourceIndex methodSpanAtClass: aClassName selector: aSelector
!

!LSPDocument methodsFor: 'updating'!
updateText: aText version: aVersion
    text := aText.
    version := aVersion.
    ast := nil.
    sourceIndex := LSPSourceIndex new initialize.
    ^ self
!

recordMethodSpan: aMethodSpan
    ^ sourceIndex addMethodSpan: aMethodSpan
!

!LSPDocument methodsFor: 'parsing'!
parseMethodAst
    ast == nil
        ifTrue: [ast := (Parser on: text) parseMethod]
        ifFalse: [0].
    ^ ast
!
