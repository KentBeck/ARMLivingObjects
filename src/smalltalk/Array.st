"Array — indexable object"

Object variableSubclass: #Array instanceVariableNames: ''

!Array class methodsFor: 'instance creation'!
with: a
    | arr |
    arr := Array new: 1.
    arr at: 1 put: a.
    ^ arr
!

with: a with: b
    | arr |
    arr := Array new: 2.
    arr at: 1 put: a.
    arr at: 2 put: b.
    ^ arr
!

with: a with: b with: c
    | arr |
    arr := Array new: 3.
    arr at: 1 put: a.
    arr at: 2 put: b.
    arr at: 3 put: c.
    ^ arr
!

!Array methodsFor: 'accessing'!
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

!Array methodsFor: 'enumerating'!
do: aBlock
    ^ self doFrom: 1 with: aBlock
!

doFrom: index with: aBlock
    index > self size
        ifTrue: [^ nil]
        ifFalse: [
            aBlock value: (self at: index).
            ^ self doFrom: index + 1 with: aBlock
        ]
!

collect: aBlock
    | result |
    result := Array new: self size.
    self collectInto: result from: 1 with: aBlock.
    ^ result
!

collectInto: result from: index with: aBlock
    index > self size
        ifTrue: [^ nil]
        ifFalse: [
            result at: index put: (aBlock value: (self at: index)).
            ^ self collectInto: result from: index + 1 with: aBlock
        ]
!

indexOf: anObject
    ^ self indexOfFrom: 1 seeking: anObject
!

indexOfFrom: index seeking: anObject
    index > self size
        ifTrue: [^ 0]
        ifFalse: [
            ((self at: index) = anObject)
                ifTrue: [^ index]
                ifFalse: [^ self indexOfFrom: index + 1 seeking: anObject]
        ]
!

with: aBlock inject: initial
    ^ self withInject: initial from: 1 with: aBlock
!

withInject: value from: index with: aBlock
    index > self size
        ifTrue: [^ value]
        ifFalse: [
            ^ self withInject: (aBlock value: value value: (self at: index)) from: index + 1 with: aBlock
        ]
!

!Array methodsFor: 'copying'!
copyFrom: start to: stop
    | newSize result |
    newSize := stop - start + 1.
    newSize < 1
        ifTrue: [^ Array new: 0]
        ifFalse: [0].
    result := Array new: newSize.
    ^ self copyElementsFrom: start to: stop into: result at: 1
!

copyElementsFrom: srcIndex to: srcStop into: dest at: destIndex
    srcIndex > srcStop
        ifTrue: [^ dest]
        ifFalse: [
            dest at: destIndex put: (self at: srcIndex).
            ^ self copyElementsFrom: srcIndex + 1 to: srcStop into: dest at: destIndex + 1
        ]
!

!Array methodsFor: 'comparing'!
= anArray
    ^ (self class == anArray class)
        ifTrue: [
            (self size = anArray size)
                ifTrue: [self elementsEqualTo: anArray startingAt: 1]
                ifFalse: [false]
        ]
        ifFalse: [false]
!

elementsEqualTo: anArray startingAt: index
    ^ (index < (self size + 1))
        ifTrue: [
            ((self at: index) = (anArray at: index))
                ifTrue: [self elementsEqualTo: anArray startingAt: index + 1]
                ifFalse: [false]
        ]
        ifFalse: [true]
!
