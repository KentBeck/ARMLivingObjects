"LSPSourceIndex — lookup for method source spans"
"Instance variables: spans"

Object subclass: #LSPSourceIndex instanceVariableNames: 'spans'

!LSPSourceIndex methodsFor: 'initialization'!
initialize
    spans := Dictionary new.
    ^ self
!

!LSPSourceIndex methodsFor: 'accessing'!
spans
    ^ spans
!

size
    ^ spans size
!

methodSpanAtClass: aClassName selector: aSelector
    ^ spans at: (self keyForClass: aClassName selector: aSelector) ifAbsent: [nil]
!

includesClass: aClassName selector: aSelector
    ^ spans includesKey: (self keyForClass: aClassName selector: aSelector)
!

!LSPSourceIndex methodsFor: 'updating'!
addMethodSpan: aMethodSpan
    spans at: (self keyForClass: aMethodSpan className selector: aMethodSpan selector) put: aMethodSpan.
    ^ aMethodSpan
!

!LSPSourceIndex methodsFor: 'private'!
keyForClass: aClassName selector: aSelector
    ^ Array with: aClassName with: aSelector
!
