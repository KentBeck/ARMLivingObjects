"Dictionary — array of Associations with linear search"

!Dictionary class methodsFor: 'image access'!
smalltalk
    <primitive: 33>
    ^ nil
!

!Dictionary methodsFor: 'accessing'!
size
    ^ tally
!

at: aKey
    | index |
    index := self indexOfKey: aKey startingAt: 1.
    ^ (index = 0)
        ifTrue: [nil]
        ifFalse: [(associations at: index) value]
!

at: aKey ifAbsent: aBlock
    | index |
    index := self indexOfKey: aKey startingAt: 1.
    ^ (index = 0)
        ifTrue: [aBlock value]
        ifFalse: [(associations at: index) value]
!

associationAt: aKey
    | index |
    index := self indexOfKey: aKey startingAt: 1.
    ^ (index = 0)
        ifTrue: [nil]
        ifFalse: [associations at: index]
!

includesKey: aKey
    ^ (self indexOfKey: aKey startingAt: 1) = 0
        ifTrue: [false]
        ifFalse: [true]
!

classNamed: aClassName
    ^ self at: aClassName asSymbol
!

methodSourceForClass: aClassName selector: aSelector
    <primitive: 34>
    ^ nil
!

at: aKey put: aValue
    | index assoc |
    self ensureStorage.
    index := self indexOfKey: aKey startingAt: 1.
    ^ (index = 0)
        ifTrue: [
            tally := tally + 1.
            assoc := Association new.
            assoc key: aKey value: aValue.
            associations at: tally put: assoc.
            aValue
        ]
        ifFalse: [
            (associations at: index) value: aValue.
            aValue
        ]
!

!Dictionary methodsFor: 'private'!
ensureStorage
    (associations == nil)
        ifTrue: [
            associations := Array new: 8.
            tally := 0
        ]
        ifFalse: [self].

    (tally < associations size)
        ifTrue: [^ self]
        ifFalse: [self growStorage]
!

growStorage
    | old newStorage newSize |
    old := associations.
    newSize := old size + old size.
    newStorage := Array new: newSize.
    self copyAssociationsFrom: old to: newStorage startingAt: 1.
    associations := newStorage.
    ^ self
!

copyAssociationsFrom: oldStorage to: newStorage startingAt: index
    ^ (index < (tally + 1))
        ifTrue: [
            newStorage at: index put: (oldStorage at: index).
            self copyAssociationsFrom: oldStorage to: newStorage startingAt: index + 1
        ]
        ifFalse: [self]
!

indexOfKey: aKey startingAt: index
    ^ (associations == nil)
        ifTrue: [0]
        ifFalse: [
            (index < (tally + 1))
                ifTrue: [
                    (((associations at: index) key) = aKey)
                        ifTrue: [index]
                        ifFalse: [self indexOfKey: aKey startingAt: index + 1]
                ]
                ifFalse: [0]
        ]
!
