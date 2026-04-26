"LSPMethodSpan — source range and text for one method definition"
"Instance variables: className selector start stop source"

Object subclass: #LSPMethodSpan instanceVariableNames: 'className selector start stop source'

!LSPMethodSpan class methodsFor: 'instance creation'!
className: aClassName selector: aSelector
    ^ self new initializeClassName: aClassName selector: aSelector
!

!LSPMethodSpan methodsFor: 'initialization'!
initializeClassName: aClassName selector: aSelector
    className := aClassName.
    selector := aSelector.
    start := nil.
    stop := nil.
    source := nil.
    ^ self
!

!LSPMethodSpan methodsFor: 'accessing'!
className
    ^ className
!

className: aClassName
    className := aClassName.
    ^ aClassName
!

selector
    ^ selector
!

selector: aSelector
    selector := aSelector.
    ^ aSelector
!

start
    ^ start
!

start: aStart
    start := aStart.
    ^ aStart
!

start: aStart stop: aStop
    start := aStart.
    stop := aStop.
    ^ self
!

stop
    ^ stop
!

stop: aStop
    stop := aStop.
    ^ aStop
!

source
    ^ source
!

source: aSource
    source := aSource.
    ^ aSource
!
