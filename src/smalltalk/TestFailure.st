Object subclass: #TestFailure instanceVariableNames: 'messageText'

!TestFailure methodsFor: 'accessing'!
messageText
    ^ messageText
!

description
    ^ self messageText
!

messageText: aString
    messageText := aString.
    ^ aString
!

!TestFailure methodsFor: 'exceptions'!
signal
    <primitive: 37>
    ^ self
!

!TestFailure class methodsFor: 'exceptions'!
signal: aString
    | failure |
    failure := self new.
    failure messageText: aString.
    ^ failure signal
!
