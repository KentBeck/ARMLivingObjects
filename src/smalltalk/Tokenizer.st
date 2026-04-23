"Tokenizer — lexical analyzer for Smalltalk source code"
"Instance variables: source, stream, buffered"

Object subclass: #Tokenizer instanceVariableNames: 'source stream buffered'

!Tokenizer class methodsFor: 'instance creation'!
on: aString
    ^ self new initializeOn: aString
!

!Tokenizer methodsFor: 'initialization'!
initializeOn: aString
    source := aString.
    stream := ReadStream on: source.
    buffered := nil.
    ^ self
!

setString: aString
    source := aString.
    stream := ReadStream on: source.
    buffered := nil.
    ^ self
!

!Tokenizer methodsFor: 'tokenizing'!
tokens
    | countStream result |
    countStream := ReadStream on: source.
    result := Array new: (self tokenCountFrom: countStream).
    stream := ReadStream on: source.
    ^ self fillTokens: result at: 1
!

next
    | token |
    buffered isNil
        ifTrue: [^ self nextTokenFromStream]
        ifFalse: [
            token := buffered.
            buffered := nil.
            ^ token
        ]
!

peek
    buffered isNil ifFalse: [^ buffered].
    buffered := self nextTokenFromStream.
    ^ buffered
!

unread: aToken
    buffered := aToken.
    ^ aToken
!

atEnd
    ^ self peek isEof
!

!Tokenizer methodsFor: 'private'!
tokenCountFrom: aStream
    | token tempStream |
    tempStream := stream.
    stream := aStream.
    token := self nextTokenFromStream.
    stream := tempStream.
    token isEof
        ifTrue: [^ 0]
        ifFalse: [0].
    ^ 1 + (self tokenCountFrom: aStream)
!

fillTokens: aCollection at: index
    | token |
    token := self nextTokenFromStream.
    token isEof
        ifTrue: [^ aCollection]
        ifFalse: [0].
    aCollection at: index put: token.
    ^ self fillTokens: aCollection at: index + 1
!

nextTokenFromStream
    | ch |
    ch := self skipWhitespaceAndComments.
    ch isNil
        ifTrue: [^ Token eof]
        ifFalse: [0].

    ch isDigit
        ifTrue: [^ self readIntegerStartingWith: ch]
        ifFalse: [0].

    (ch value = 36)
        ifTrue: [^ self readCharacterLiteral]
        ifFalse: [0].

    (ch value = 39)
        ifTrue: [^ self readStringLiteral]
        ifFalse: [0].

    (ch value = 35)
        ifTrue: [^ self readSymbolOrLiteralArray]
        ifFalse: [0].

    ^ self classifyIdentifierOrSpecial: ch
!

classifyIdentifierOrSpecial: ch
    ch isLetter
        ifTrue: [^ self readIdentifierOrKeywordStartingWith: ch]
        ifFalse: [0].
    (ch value = 95)
        ifTrue: [^ self readIdentifierOrKeywordStartingWith: ch]
        ifFalse: [0].

    (ch value = 58)
        ifTrue: [^ self readColonToken]
        ifFalse: [0].

    (self isBinarySelectorChar: ch value)
        ifTrue: [^ self readBinarySelectorStartingWith: ch]
        ifFalse: [0].

    ^ self readSpecialChar: ch
!

skipWhitespaceAndComments
    | ch |
    ch := stream next.
    ch isNil
        ifTrue: [^ nil]
        ifFalse: [0].
    ch := ch asCharacter.

    (self isSeparator: ch value)
        ifTrue: [^ self skipWhitespaceAndComments]
        ifFalse: [0].

    (ch value = 34)
        ifTrue: [
            self skipComment.
            ^ self skipWhitespaceAndComments
        ]
        ifFalse: [0].

    ^ ch
!

skipComment
    | ch |
    ch := stream next.
    ch isNil
        ifTrue: [^ self]
        ifFalse: [0].
    (ch asCharacter value = 34)
        ifTrue: [^ self]
        ifFalse: [^ self skipComment]
!

isSeparator: aValue
    (aValue = 32) ifTrue: [^ true].
    (aValue = 9) ifTrue: [^ true].
    (aValue = 10) ifTrue: [^ true].
    (aValue = 13) ifTrue: [^ true].
    ^ false
!

readIntegerStartingWith: firstChar
    | value text |
    value := firstChar value - 48.
    text := self collectDigitsFrom: value text: (String new: 1) at: 1 firstChar: firstChar.
    ^ Token type: #integer text: (text at: 1) value: (text at: 2)
!

collectDigitsFrom: value text: str at: index firstChar: first
    | peek newStr newValue |
    newStr := str.
    index = 1
        ifTrue: [
            newStr := String new: 16.
            newStr at: 1 put: first value
        ]
        ifFalse: [0].
    peek := stream peek.
    peek isNil
        ifTrue: [^ Array with: (self trimString: newStr to: index) with: value]
        ifFalse: [0].
    peek := peek asCharacter.
    peek isDigit
        ifTrue: [
            stream next.
            newStr at: (index + 1) put: peek value.
            newValue := (value * 10) + (peek value - 48).
            ^ self collectDigitsFrom: newValue text: newStr at: index + 1 firstChar: first
        ]
        ifFalse: [0].
    ^ Array with: (self trimString: newStr to: index) with: value
!

trimString: str to: len
    | result |
    len < 1
        ifTrue: [^ '']
        ifFalse: [0].
    result := String new: len.
    ^ self copyChars: str to: result from: 1 to: len
!

copyChars: src to: dest from: index to: max
    index > max
        ifTrue: [^ dest]
        ifFalse: [
            dest at: index put: (src at: index).
            ^ self copyChars: src to: dest from: index + 1 to: max
        ]
!

readCharacterLiteral
    | ch text |
    ch := stream next.
    ch isNil
        ifTrue: [^ Token type: #character text: '$' value: nil]
        ifFalse: [0].
    ch := ch asCharacter.
    text := String new: 2.
    text at: 1 put: 36.
    text at: 2 put: ch value.
    ^ Token type: #character text: text value: ch value
!

readStringLiteral
    ^ self collectStringChars: (String new: 128) at: 1
!

collectStringChars: buffer at: index
    | ch peek |
    ch := stream next.
    ch isNil
        ifTrue: [^ Token type: #string text: (self trimString: buffer to: index - 1) value: nil]
        ifFalse: [0].
    ch := ch asCharacter.
    (ch value = 39)
        ifTrue: [
            peek := stream peek.
            peek isNil
                ifTrue: [^ Token type: #string text: (self trimString: buffer to: index - 1) value: nil]
                ifFalse: [
                    (peek asCharacter value = 39)
                        ifTrue: [
                            stream next.
                            buffer at: index put: 39.
                            ^ self collectStringChars: buffer at: index + 1
                        ]
                        ifFalse: [^ Token type: #string text: (self trimString: buffer to: index - 1) value: nil]
                ]
        ]
        ifFalse: [
            buffer at: index put: ch value.
            ^ self collectStringChars: buffer at: index + 1
        ]
!

readSymbolOrLiteralArray
    | peek |
    peek := stream peek.
    peek isNil
        ifTrue: [^ Token type: #special text: '#' value: nil]
        ifFalse: [0].
    peek := peek asCharacter.

    (peek value = 40)
        ifTrue: [
            stream next.
            ^ Token type: #special text: '#(' value: nil
        ]
        ifFalse: [0].

    (peek value = 39)
        ifTrue: [
            stream next.
            ^ self readSymbolString
        ]
        ifFalse: [0].

    peek isLetter
        ifTrue: [^ self readSymbolIdentifier]
        ifFalse: [0].
    (peek value = 95)
        ifTrue: [^ self readSymbolIdentifier]
        ifFalse: [0].

    (self isBinarySelectorChar: peek value)
        ifTrue: [^ self readBinarySymbol]
        ifFalse: [0].

    ^ Token type: #special text: '#' value: nil
!

readSymbolString
    | strToken |
    strToken := self readStringLiteral.
    ^ Token type: #symbol text: strToken text value: strToken text asSymbol
!

readSymbolIdentifier
    | text |
    text := self collectIdentifierChars: (String new: 64) at: 1 withColons: true.
    ^ Token type: #symbol text: text value: text asSymbol
!

readBinarySymbol
    | ch text peek |
    ch := stream next asCharacter.
    text := String new: 2.
    text at: 1 put: ch value.
    peek := stream peek.
    peek isNil
        ifTrue: [^ Token type: #symbol text: (self trimString: text to: 1) value: (self trimString: text to: 1) asSymbol]
        ifFalse: [
            (self isBinarySelectorChar: peek asCharacter value)
                ifTrue: [
                    text at: 2 put: stream next asCharacter value.
                    ^ Token type: #symbol text: (self trimString: text to: 2) value: (self trimString: text to: 2) asSymbol
                ]
                ifFalse: [
                    ^ Token type: #symbol text: (self trimString: text to: 1) value: (self trimString: text to: 1) asSymbol
                ]
        ]
!

readIdentifierOrKeywordStartingWith: firstChar
    | text peek |
    text := self collectIdentifierCharsStartingWith: firstChar.
    peek := stream peek.
    peek isNil
        ifTrue: [^ Token type: #identifier text: text value: nil]
        ifFalse: [0].
    (peek asCharacter value = 58)
        ifTrue: [^ self checkKeywordOrAssignment: text]
        ifFalse: [^ Token type: #identifier text: text value: nil]
!

checkKeywordOrAssignment: text
    | nextPeek |
    stream next.
    nextPeek := stream peek.
    nextPeek isNil
        ifTrue: [^ self keywordTokenFor: text]
        ifFalse: [0].
    (nextPeek asCharacter value = 61)
        ifFalse: [^ self keywordTokenFor: text].
    stream next.
    buffered := Token type: #special text: ':=' value: nil.
    ^ Token type: #identifier text: text value: nil
!

keywordTokenFor: text
    ^ Token type: #keyword text: (text , ':') value: nil
!

collectIdentifierCharsStartingWith: firstChar
    | buffer |
    buffer := String new: 64.
    buffer at: 1 put: firstChar value.
    ^ self collectIdentifierFrom: buffer at: 2
!

collectIdentifierFrom: buffer at: index
    | peek |
    peek := stream peek.
    peek isNil
        ifTrue: [^ self trimString: buffer to: index - 1]
        ifFalse: [0].
    peek := peek asCharacter.
    peek isLetter
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierFrom: buffer at: index + 1
        ]
        ifFalse: [0].
    peek isDigit
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierFrom: buffer at: index + 1
        ]
        ifFalse: [0].
    (peek value = 95)
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierFrom: buffer at: index + 1
        ]
        ifFalse: [0].
    ^ self trimString: buffer to: index - 1
!

collectIdentifierChars: buffer at: index withColons: allowColons
    | peek |
    peek := stream peek.
    peek isNil
        ifTrue: [^ self trimString: buffer to: index - 1]
        ifFalse: [0].
    peek := peek asCharacter.
    peek isLetter
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierChars: buffer at: index + 1 withColons: allowColons
        ]
        ifFalse: [0].
    peek isDigit
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierChars: buffer at: index + 1 withColons: allowColons
        ]
        ifFalse: [0].
    (peek value = 95)
        ifTrue: [
            stream next.
            buffer at: index put: peek value.
            ^ self collectIdentifierChars: buffer at: index + 1 withColons: allowColons
        ]
        ifFalse: [0].
    allowColons
        ifTrue: [
            (peek value = 58)
                ifTrue: [^ self collectKeywordContinuation: buffer at: index]
                ifFalse: [0]
        ]
        ifFalse: [0].
    ^ self trimString: buffer to: index - 1
!

collectKeywordContinuation: buffer at: index
    | peek |
    stream next.
    buffer at: index put: 58.
    peek := stream peek.
    peek isNil
        ifTrue: [^ self trimString: buffer to: index]
        ifFalse: [
            peek asCharacter isLetter
                ifTrue: [^ self collectIdentifierChars: buffer at: index + 1 withColons: true]
                ifFalse: [^ self trimString: buffer to: index]
        ]
!

readColonToken
    | peek |
    peek := stream peek.
    peek isNil
        ifTrue: [^ Token type: #special text: ':' value: nil]
        ifFalse: [
            (peek asCharacter value = 61)
                ifTrue: [
                    stream next.
                    ^ Token type: #special text: ':=' value: nil
                ]
                ifFalse: [^ Token type: #special text: ':' value: nil]
        ]
!

readBinarySelectorStartingWith: firstChar
    | text peek |
    text := String new: 2.
    text at: 1 put: firstChar value.
    peek := stream peek.
    peek isNil
        ifTrue: [^ Token type: #binary text: (self trimString: text to: 1) value: nil]
        ifFalse: [
            (self isBinarySelectorChar: peek asCharacter value)
                ifTrue: [
                    text at: 2 put: stream next asCharacter value.
                    ^ Token type: #binary text: (self trimString: text to: 2) value: nil
                ]
                ifFalse: [^ Token type: #binary text: (self trimString: text to: 1) value: nil]
        ]
!

isBinarySelectorChar: aValue
    (aValue = 43) ifTrue: [^ true].
    (aValue = 45) ifTrue: [^ true].
    (aValue = 42) ifTrue: [^ true].
    (aValue = 47) ifTrue: [^ true].
    (aValue = 60) ifTrue: [^ true].
    (aValue = 62) ifTrue: [^ true].
    (aValue = 61) ifTrue: [^ true].
    ^ self isBinarySelectorCharExtended: aValue
!

isBinarySelectorCharExtended: aValue
    (aValue = 126) ifTrue: [^ true].
    (aValue = 38) ifTrue: [^ true].
    (aValue = 124) ifTrue: [^ true].
    (aValue = 64) ifTrue: [^ true].
    (aValue = 37) ifTrue: [^ true].
    (aValue = 92) ifTrue: [^ true].
    (aValue = 63) ifTrue: [^ true].
    ^ false
!

readSpecialChar: ch
    | text |
    text := String new: 1.
    text at: 1 put: ch value.
    ^ Token type: #special text: text value: nil
!

!Tokenizer methodsFor: 'compatibility'!
nextTokenFrom: aStream
    | oldStream token |
    oldStream := stream.
    stream := aStream.
    token := self nextTokenFromStream.
    stream := oldStream.
    token isEof
        ifTrue: [^ nil]
        ifFalse: [^ token]
!

nextNonSeparatorFrom: aStream
    | oldStream ch |
    oldStream := stream.
    stream := aStream.
    ch := self skipWhitespaceAndComments.
    stream := oldStream.
    ^ ch
!
