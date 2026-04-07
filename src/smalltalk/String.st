"String — byte-indexable sequence of characters"

!String methodsFor: 'accessing'!
size
    <primitive: 11>
    self error: 'size primitive failed'
!

at: index
    <primitive: 7>
    self error: 'at: primitive failed'
!

at: index put: value
    <primitive: 9>
    self error: 'at:put: primitive failed'
!

!String methodsFor: 'comparing'!
= aString
    <primitive: 25>
    false
!

hash
    <primitive: 26>
    self error: 'hash primitive failed'
!

!String methodsFor: 'converting'!
asSymbol
    <primitive: 27>
    ^ self
!

!String methodsFor: 'printing'!
printString
    | i |
    i := 1.
    [i < (self size + 1)] whileTrue: [
        (self at: i) asCharacter printChar.
        i := i + 1
    ].
    ^ self
!

!String methodsFor: 'concatenation'!
, aString
    | result i offset |
    result := String new: self size + aString size.

    i := 1.
    [i < (self size + 1)] whileTrue: [
        result at: i put: (self at: i).
        i := i + 1
    ].

    offset := self size.
    i := 1.
    [i < (aString size + 1)] whileTrue: [
        result at: (offset + i) put: (aString at: i).
        i := i + 1
    ].

    ^ result
!
