"Character — immediate value, encoding: (codePoint << 4) | 0x0F"

!Character methodsFor: 'accessing'!
value
    <primitive: 19>
!

!Character methodsFor: 'comparing'!
= aCharacter
    ^ self == aCharacter
!

!Character methodsFor: 'printing'!
printChar
    <primitive: 15>
!

!Character methodsFor: 'testing'!
isLetter
    <primitive: 21>
!

isDigit
    <primitive: 22>
!

isAlphanumeric
    ^ self isLetter or: [self isDigit]
!

!Character methodsFor: 'converting'!
asUppercase
    <primitive: 23>
!

asLowercase
    <primitive: 24>
!
