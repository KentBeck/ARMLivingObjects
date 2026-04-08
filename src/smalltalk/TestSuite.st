"TestSuite — explicit collection of TestCase instances"
"Instance variables: tests, tally"

!TestSuite class methodsFor: 'instance creation'!
withAll: aCollection
    ^ (self new tests: aCollection) yourself
!

!TestSuite methodsFor: 'accessing'!
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

!TestSuite methodsFor: 'running'!
runOn: aResult
    ^ self runOn: aResult startingAt: 1
!

runOn: aResult startingAt: index
    ^ (index < (tally + 1))
        ifTrue: [
            (tests at: index) runOn: aResult.
            self runOn: aResult startingAt: index + 1
        ]
        ifFalse: [aResult]
!
