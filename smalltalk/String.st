"String — byte-indexable sequence of characters"

size
    <primitive: 11>

at: index
    <primitive: 7>

at: index put: value
    <primitive: 9>

= aString
    <primitive: 25>

hash
    <primitive: 26>

asSymbol
    <primitive: 27>

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

