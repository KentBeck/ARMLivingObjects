"LSPChunkParser — parse !Class methodsFor: chunks into method spans"
"Instance variables: source"

Object subclass: #LSPChunkParser instanceVariableNames: 'source'

!LSPChunkParser class methodsFor: 'instance creation'!
on: aString
    ^ self new initializeOn: aString
!

!LSPChunkParser methodsFor: 'initialization'!
initializeOn: aString
    source := aString.
    ^ self
!

!LSPChunkParser methodsFor: 'parsing'!
parseMethodSpans
    | spans count |
    spans := Array new: 256.
    count := self parseFrom: 1 className: nil classSide: false category: nil into: spans at: 1.
    ^ spans copyFrom: 1 to: count - 1
!

parseFrom: index className: currentClassName classSide: currentClassSide category: currentCategory into: spans at: spanIndex
    | limit segmentEnd segmentStart trimmedStart trimmedEnd parsedHeader methodEnd methodStart methodNode methodSource span |
    limit := source size.
    index > limit
        ifTrue: [^ spanIndex]
        ifFalse: [0].

    ((source at: index) asCharacter value = 33)
        ifTrue: [
            segmentStart := index + 1.
            segmentEnd := self indexOfBangFrom: segmentStart.
            segmentEnd = 0
                ifTrue: [^ spanIndex]
                ifFalse: [0].
            trimmedStart := self trimStartFrom: segmentStart to: segmentEnd - 1.
            trimmedEnd := self trimEndFrom: segmentEnd - 1 to: trimmedStart.
            parsedHeader := trimmedEnd < trimmedStart
                ifTrue: [nil]
                ifFalse: [self parseHeaderSegmentFrom: trimmedStart to: trimmedEnd].
            parsedHeader isNil
                ifTrue: [
                    ^ self parseFrom: segmentEnd + 1
                        className: currentClassName
                        classSide: currentClassSide
                        category: currentCategory
                        into: spans
                        at: spanIndex
                ]
                ifFalse: [
                    ^ self parseFrom: segmentEnd + 1
                        className: (parsedHeader at: 1)
                        classSide: (parsedHeader at: 2)
                        category: (parsedHeader at: 3)
                        into: spans
                        at: spanIndex
                ]
        ]
        ifFalse: [0].

    currentClassName isNil
        ifTrue: [
            ^ self parseFrom: index + 1
                className: currentClassName
                classSide: currentClassSide
                category: currentCategory
                into: spans
                at: spanIndex
        ]
        ifFalse: [0].

    methodStart := index.
    methodEnd := self indexOfBangFrom: index.
    methodEnd = 0
        ifTrue: [^ spanIndex]
        ifFalse: [0].
    trimmedStart := self trimStartFrom: methodStart to: methodEnd - 1.
    trimmedEnd := self trimEndFrom: methodEnd - 1 to: trimmedStart.
    trimmedEnd < trimmedStart
        ifTrue: [
            ^ self parseFrom: methodEnd
                className: currentClassName
                classSide: currentClassSide
                category: currentCategory
                into: spans
                at: spanIndex
        ]
        ifFalse: [0].

    methodSource := self copyFrom: trimmedStart to: trimmedEnd.
    methodNode := Parser on: methodSource.
    methodNode := methodNode parseMethod.
    methodNode isNil
        ifTrue: [
            ^ self parseFrom: methodEnd + 1
                className: currentClassName
                classSide: currentClassSide
                category: currentCategory
                into: spans
                at: spanIndex
        ]
        ifFalse: [0].

    span := LSPMethodSpan className: currentClassName asSymbol selector: methodNode selector asSymbol.
    span start: trimmedStart stop: trimmedEnd.
    span source: methodSource.
    spans at: spanIndex put: span.
    ^ self parseFrom: methodEnd + 1
        className: currentClassName
        classSide: currentClassSide
        category: currentCategory
        into: spans
        at: spanIndex + 1
!

!LSPChunkParser methodsFor: 'private'!
indexOfBangFrom: index
    index > source size
        ifTrue: [^ 0]
        ifFalse: [0].
    ((source at: index) asCharacter value = 33)
        ifTrue: [^ index]
        ifFalse: [^ self indexOfBangFrom: index + 1]
!

trimStartFrom: start to: stop
    start > stop
        ifTrue: [^ start]
        ifFalse: [0].
    (self isSeparatorAt: start)
        ifTrue: [^ self trimStartFrom: start + 1 to: stop]
        ifFalse: [^ start]
!

trimEndFrom: stop to: start
    stop < start
        ifTrue: [^ stop]
        ifFalse: [0].
    (self isSeparatorAt: stop)
        ifTrue: [^ self trimEndFrom: stop - 1 to: start]
        ifFalse: [^ stop]
!

isSeparatorAt: index
    | value |
    value := (source at: index) asCharacter value.
    (value = 32) ifTrue: [^ true].
    (value = 9) ifTrue: [^ true].
    (value = 10) ifTrue: [^ true].
    (value = 13) ifTrue: [^ true].
    ^ false
!

parseHeaderSegmentFrom: start to: stop
    | index className classSide category |
    index := start.
    className := self parseIdentifierFrom: index to: stop.
    className isNil
        ifTrue: [^ nil]
        ifFalse: [0].
    index := start + className size.
    index := self skipSpacesFrom: index to: stop.
    classSide := false.
    (self matches: 'class' from: index to: stop)
        ifTrue: [
            classSide := true.
            index := index + 5.
            index := self skipSpacesFrom: index to: stop
        ]
        ifFalse: [0].
    (self matches: 'methodsFor:' from: index to: stop)
        ifFalse: [^ nil]
        ifTrue: [index := index + 11].
    index := self skipSpacesFrom: index to: stop.
    index > stop
        ifTrue: [^ nil]
        ifFalse: [0].
    ((source at: index) asCharacter value = 39)
        ifFalse: [^ nil]
        ifTrue: [0].
    category := self parseQuotedFrom: index + 1 to: stop.
    category isNil
        ifTrue: [^ nil]
        ifFalse: [^ Array with: className with: classSide with: category]
!

parseIdentifierFrom: index to: stop
    | end |
    index > stop
        ifTrue: [^ nil]
        ifFalse: [0].
    end := self scanIdentifierFrom: index to: stop.
    end < index
        ifTrue: [^ nil]
        ifFalse: [^ self copyFrom: index to: end]
!

scanIdentifierFrom: index to: stop
    | value |
    index > stop
        ifTrue: [^ index - 1]
        ifFalse: [0].
    value := (source at: index) asCharacter value.
    (self isIdentifierValue: value)
        ifTrue: [^ self scanIdentifierFrom: index + 1 to: stop]
        ifFalse: [^ index - 1]
!

isIdentifierValue: value
    ((value > 64) and: [value < 91]) ifTrue: [^ true].
    ((value > 96) and: [value < 123]) ifTrue: [^ true].
    ((value > 47) and: [value < 58]) ifTrue: [^ true].
    (value = 95) ifTrue: [^ true].
    ^ false
!

skipSpacesFrom: index to: stop
    index > stop
        ifTrue: [^ index]
        ifFalse: [0].
    (self isSpaceAt: index)
        ifTrue: [^ self skipSpacesFrom: index + 1 to: stop]
        ifFalse: [^ index]
!

isSpaceAt: index
    | value |
    value := (source at: index) asCharacter value.
    (value = 32) ifTrue: [^ true].
    (value = 9) ifTrue: [^ true].
    ^ false
!

matches: aString from: index to: stop
    ^ self matches: aString from: index to: stop at: 1
!

matches: aString from: index to: stop at: stringIndex
    stringIndex > aString size
        ifTrue: [^ true]
        ifFalse: [0].
    index > stop
        ifTrue: [^ false]
        ifFalse: [0].
    ((source at: index) = (aString at: stringIndex))
        ifTrue: [^ self matches: aString from: index + 1 to: stop at: stringIndex + 1]
        ifFalse: [^ false]
!

parseQuotedFrom: index to: stop
    | quoteIndex |
    quoteIndex := self indexOfQuoteFrom: index to: stop.
    quoteIndex = 0
        ifTrue: [^ nil]
        ifFalse: [^ self copyFrom: index to: quoteIndex - 1]
!

indexOfQuoteFrom: index to: stop
    index > stop
        ifTrue: [^ 0]
        ifFalse: [0].
    ((source at: index) asCharacter value = 39)
        ifTrue: [^ index]
        ifFalse: [^ self indexOfQuoteFrom: index + 1 to: stop]
!

copyFrom: start to: stop
    | result |
    result := String new: (stop - start) + 1.
    ^ self copyFrom: start to: stop into: result at: 1
!

copyFrom: start to: stop into: aString at: stringIndex
    start > stop
        ifTrue: [^ aString]
        ifFalse: [
            aString at: stringIndex put: (source at: start).
            ^ self copyFrom: start + 1 to: stop into: aString at: stringIndex + 1
        ]
!
