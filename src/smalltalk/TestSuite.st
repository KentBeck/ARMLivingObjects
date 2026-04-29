"TestSuite — explicit collection of TestCase instances"
"Instance variables: tests, tally"

Object subclass: #TestSuite instanceVariableNames: 'tests tally'

!TestSuite class methodsFor: 'instance creation'!
new
    ^ self basicNew initialize
!

withAll: aCollection
    ^ (self new tests: aCollection) yourself
!

!TestSuite methodsFor: 'accessing'!
initialize
    tests := Array new: 4.
    tally := 0.
    ^ self
!

tests
    ^ tests
!

tests: aCollection
    tests := aCollection.
    tally := aCollection size.
    ^ aCollection
!

size
    ^ tally
!

add: aTest
    tests == nil
        ifTrue: [self initialize]
        ifFalse: [self].
    tally = tests size
        ifTrue: [self growStorage]
        ifFalse: [self].
    tally := tally + 1.
    tests at: tally put: aTest.
    ^ aTest
!

!TestSuite methodsFor: 'running'!
runOn: aResult
    | index |
    index := 1.
    [index < (tally + 1)] whileTrue: [
        (tests at: index) runOn: aResult.
        index := index + 1
    ].
    ^ aResult
!

runOn: aResult startingAt: index
    ^ (index < (tally + 1))
        ifTrue: [
            (tests at: index) runOn: aResult.
            self runOn: aResult startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!TestSuite methodsFor: 'private'!
growStorage
    | grown |
    grown := Array new: tests size + tests size.
    self copyTestsFrom: tests to: grown startingAt: 1.
    tests := grown.
    ^ self
!

copyTestsFrom: oldStorage to: newStorage startingAt: index
    ^ (index < (tally + 1))
        ifTrue: [
            newStorage at: index put: (oldStorage at: index).
            self copyTestsFrom: oldStorage to: newStorage startingAt: index + 1
        ]
        ifFalse: [newStorage]
!
