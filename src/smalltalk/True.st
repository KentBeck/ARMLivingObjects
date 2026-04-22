"True — the single true value (immediate 0x07)"

Object subclass: #True instanceVariableNames: ''

!True methodsFor: 'control flow'!
ifTrue: trueBlock ifFalse: falseBlock
    ^ trueBlock value
!

ifTrue: trueBlock
    ^ trueBlock value
!

ifFalse: falseBlock
    ^ nil
!

and: aBlock
    ^ aBlock value
!

& aBoolean
    ^ aBoolean
!

or: aBlock
    ^ true
!
