"False — the single false value (immediate 0x0B)"

Object subclass: #False instanceVariableNames: ''

!False methodsFor: 'control flow'!
ifTrue: trueBlock ifFalse: falseBlock
    ^ falseBlock value
!

ifTrue: trueBlock
    ^ nil
!

ifFalse: falseBlock
    ^ falseBlock value
!

and: aBlock
    ^ false
!

or: aBlock
    ^ aBlock value
!
