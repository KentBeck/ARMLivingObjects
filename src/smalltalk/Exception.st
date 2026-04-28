Object subclass: #Exception instanceVariableNames: 'messageText signalContext signaler'

!Exception methodsFor: 'accessing'!
messageText
    ^ messageText
!

messageText: aString
    messageText := aString.
    ^ aString
!

signalContext
    ^ signalContext
!

signalContext: aContext
    signalContext := aContext.
    ^ aContext
!

signaler
    ^ signaler
!

signaler: anObject
    signaler := anObject.
    ^ anObject
!

!Exception methodsFor: 'exceptions'!
signal
    <primitive: 37>
    ^ self
!

resume: aValue
    ^ aValue
!

defaultAction
    ^ self halt
!

!Exception class methodsFor: 'exceptions'!
signal: aString
    | exception |
    exception := self new.
    exception messageText: aString.
    exception signalContext: thisContext sender.
    exception signaler: thisContext receiver.
    ^ exception signal
!
