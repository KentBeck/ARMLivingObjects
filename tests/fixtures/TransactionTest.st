TestCase subclass: #TransactionTest instanceVariableNames: ''

!TransactionTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runTransactionTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 4.
    selectors at: 1 put: #testAtomicCommitsObjectChanges.
    selectors at: 2 put: #testAtomicRollsBackOnError.
    selectors at: 3 put: #testAtomicReturnsBlockValue.
    selectors at: 4 put: #testReadOnlyEvaluatesWithoutMutationProtocol.
    ^ selectors
!

runTransactionTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runTransactionTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!TransactionTest methodsFor: 'helpers'!
newPoint
    ^ PointFile x: 1 y: 2
!

!TransactionTest methodsFor: 'tests'!
testAtomicCommitsObjectChanges
    | point |
    point := self newPoint.
    Transaction atomic: [
        point x: 7.
        point y: 9
    ].
    ^ self assert:
        ((point x = 7) and: [point y = 9])
!

testAtomicRollsBackOnError
    | point |
    point := self newPoint.
    [
        Transaction atomic: [
            point x: 7.
            Error signal: 'rollback'
        ]
    ] on: Error do: [nil].
    ^ self assert:
        ((point x = 1) and: [point y = 2])
!

testAtomicReturnsBlockValue
    | value |
    value := Transaction atomic: [41].
    ^ self assert: value equals: 41
!

testReadOnlyEvaluatesWithoutMutationProtocol
    | point value |
    point := self newPoint.
    value := Transaction readOnly: [
        point x + point y
    ].
    ^ self assert: value equals: 3
!
