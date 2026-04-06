"SmallInteger — tagged immediate integer"

!SmallInteger methodsFor: 'arithmetic'!
+ other
    <primitive: 1>
!

- other
    <primitive: 2>
!

* other
    <primitive: 5>
!

!SmallInteger methodsFor: 'comparing'!
< other
    <primitive: 3>
!

= other
    <primitive: 4>
!

!SmallInteger methodsFor: 'converting'!
asCharacter
    <primitive: 20>
!

!SmallInteger methodsFor: 'examples'!
factorial
    ^ (self = 1)
        ifTrue: [1]
        ifFalse: [self * (self - 1) factorial]
!
