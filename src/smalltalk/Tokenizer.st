"Tokenizer — first Smalltalk-side tokenizer path"
"Instance variables: source"

!Tokenizer methodsFor: 'initialization'!
setString: aString
    source := aString.
    ^ self
!

!Tokenizer methodsFor: 'tokenizing'!
tokens
    | stream result |
    stream := ReadStream on: source.
    result := Array new: (self tokenCountFrom: stream).
    ^ self fillTokens: result from: (ReadStream on: source) at: 1
!

!Tokenizer methodsFor: 'private'!
tokenCountFrom: aStream
    | token |
    token := self nextTokenFrom: aStream.
    token isNil
        ifTrue: [^ 0]
        ifFalse: [0].
    ^ 1 + (self tokenCountFrom: aStream)
!

fillTokens: aCollection from: aStream at: index
    | token |
    token := self nextTokenFrom: aStream.
    token isNil
        ifTrue: [^ aCollection]
        ifFalse: [0].
    aCollection at: index put: token.
    ^ self fillTokens: aCollection from: aStream at: index + 1
!

nextTokenFrom: aStream
    | ch |
    ch := self nextNonSeparatorFrom: aStream.
    ch isNil
        ifTrue: [^ nil]
        ifFalse: [0].
    ch isDigit
        ifTrue: [^ self readIntegerFrom: aStream accumulated: ch value - 48]
        ifFalse: [0].
    (self isBinarySelector: ch value)
        ifTrue: [^ self selectorTokenFor: ch value]
        ifFalse: [0].
    ^ nil
!

nextNonSeparatorFrom: aStream
    | next |
    next := aStream next.
    next isNil
        ifTrue: [^ nil]
        ifFalse: [0].
    next := next asCharacter.
    (self isSeparator: next value)
        ifTrue: [^ self nextNonSeparatorFrom: aStream]
        ifFalse: [0].
    ^ next
!

isSeparator: aValue
    ^ (aValue = 32)
        ifTrue: [true]
        ifFalse: [
            (aValue = 9)
                ifTrue: [true]
                ifFalse: [
                    (aValue = 10)
                        ifTrue: [true]
                        ifFalse: [aValue = 13]
                ]
        ]
!

isBinarySelector: aValue
    (aValue = 43) ifTrue: [^ true].
    (aValue = 45) ifTrue: [^ true].
    (aValue = 42) ifTrue: [^ true].
    (aValue = 47) ifTrue: [^ true].
    (aValue = 60) ifTrue: [^ true].
    (aValue = 62) ifTrue: [^ true].
    (aValue = 61) ifTrue: [^ true].
    (aValue = 126) ifTrue: [^ true].
    (aValue = 38) ifTrue: [^ true].
    (aValue = 124) ifTrue: [^ true].
    (aValue = 64) ifTrue: [^ true].
    (aValue = 37) ifTrue: [^ true].
    (aValue = 44) ifTrue: [^ true].
    (aValue = 63) ifTrue: [^ true].
    ^ false
!

readIntegerFrom: aStream accumulated: value
    | peek |
    peek := aStream peek.
    peek isNil
        ifTrue: [^ value]
        ifFalse: [0].
    peek := peek asCharacter.
    peek isDigit
        ifTrue: [
            aStream next.
            ^ self readIntegerFrom: aStream accumulated: ((value * 10) + (peek value - 48))
        ]
        ifFalse: [0].
    ^ value
!

selectorTokenFor: aValue
    | one |
    one := String new: 1.
    one at: 1 put: aValue.
    ^ one asSymbol
!
