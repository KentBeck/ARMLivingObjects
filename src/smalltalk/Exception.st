Object subclass: #Exception instanceVariableNames: 'messageText'

!Exception methodsFor: 'accessing'!
messageText
    ^ messageText
!

messageText: aString
    messageText := aString.
    ^ aString
!

!Exception methodsFor: 'exceptions'!
resume: aValue
    ^ aValue
!

!Exception class methodsFor: 'exceptions'!
signal: aString
    <primitive: 37>
    ^ nil
!
