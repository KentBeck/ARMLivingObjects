"Token — lexical token with type, text, and optional value"
"Instance variables: type, text, value"

Object subclass: #Token instanceVariableNames: 'type text value'

!Token class methodsFor: 'instance creation'!
type: aSymbol text: aString
    ^ self new type: aSymbol text: aString value: nil
!

type: aSymbol text: aString value: anObject
    ^ self new type: aSymbol text: aString value: anObject
!

eof
    ^ self type: #eof text: '' value: nil
!

!Token methodsFor: 'initialization'!
type: aSymbol text: aString value: anObject
    type := aSymbol.
    text := aString.
    value := anObject.
    ^ self
!

!Token methodsFor: 'accessing'!
type
    ^ type
!

text
    ^ text
!

value
    ^ value
!

!Token methodsFor: 'testing'!
isEof
    ^ type = #eof
!

isIdentifier
    ^ type = #identifier
!

isKeyword
    ^ type = #keyword
!

isInteger
    ^ type = #integer
!

isCharacter
    ^ type = #character
!

isString
    ^ type = #string
!

isSymbol
    ^ type = #symbol
!

isSpecial
    ^ type = #special
!

isBinarySelector
    ^ type = #binary
!

isSpecialText: aString
    ^ (type = #special) & (text = aString)
!

!Token methodsFor: 'comparing'!
= anObject
    ^ (anObject isKindOf: Token)
        ifTrue: [
            (type = anObject type) & (text = anObject text)
        ]
        ifFalse: [false]
!
