"SmallInteger — tagged immediate integer"

+ other
    <primitive: 1>

- other
    <primitive: 2>

* other
    <primitive: 5>

< other
    <primitive: 3>

= other
    <primitive: 4>

asCharacter
    <primitive: 20>

factorial
    ^ (self = 1)
        ifTrue: [1]
        ifFalse: [self * (self - 1) factorial]

