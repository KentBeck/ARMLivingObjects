"Character — immediate value, encoding: (codePoint << 4) | 0x0F"

value
    <primitive: 19>

= aCharacter
    ^ self == aCharacter

printChar
    <primitive: 15>

isLetter
    <primitive: 21>

isDigit
    <primitive: 22>

isAlphanumeric
    ^ self isLetter or: [self isDigit]

asUppercase
    <primitive: 23>

asLowercase
    <primitive: 24>

