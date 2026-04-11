"Array — indexable object"

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
