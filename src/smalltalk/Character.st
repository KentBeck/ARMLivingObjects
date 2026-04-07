"Character — immediate value, encoding: (codePoint << 4) | 0x0F"

!Character methodsFor: 'accessing'!
value
    <primitive: 19>
    ^ 0
!

!Character methodsFor: 'comparing'!
= aCharacter
    ^ self == aCharacter
!

!Character methodsFor: 'printing'!
printChar
    <primitive: 15>
    ^ self
!

!Character methodsFor: 'testing'!
isLetter
    <primitive: 21>
    ^ false
!

isDigit
    <primitive: 22>
    ^ false
!

isAlphanumeric
    ^ self isLetter or: [self isDigit]
!

!Character methodsFor: 'converting'!
asUppercase
    <primitive: 23>
    ^ self
!

asLowercase
    <primitive: 24>
    ^ self
!
