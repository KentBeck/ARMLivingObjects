"SmallInteger — tagged immediate integer"

Object subclass: #SmallInteger instanceVariableNames: ''

!SmallInteger methodsFor: 'arithmetic'!
+ other
    <primitive: 1>
    self error: '+ primitive failed'
!

- other
    <primitive: 2>
    self error: '- primitive failed'
!

* other
    <primitive: 5>
    self error: '* primitive failed'
!

!SmallInteger methodsFor: 'comparing'!
< other
    <primitive: 3>
    self error: '< primitive failed'
!

= other
    <primitive: 4>
    self error: '= primitive failed'
!

> other
    ^ other < self
!

<= other
    ^ (self > other)
        ifTrue: [false]
        ifFalse: [true]
!

>= other
    ^ (self < other)
        ifTrue: [false]
        ifFalse: [true]
!

!SmallInteger methodsFor: 'converting'!
asCharacter
    <primitive: 20>
    ^ self
!

!SmallInteger methodsFor: 'examples'!
factorial
    ^ (self = 1)
        ifTrue: [1]
        ifFalse: [self * (self - 1) factorial]
!
