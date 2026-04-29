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
newPair
    | pair |
    pair := Array new: 2.
    pair at: 1 put: 1.
    pair at: 2 put: 2.
    ^ pair
!

!TransactionTest methodsFor: 'tests'!
testAtomicCommitsObjectChanges
    | pair |
    pair := self newPair.
    Transaction atomic: [
        pair at: 1 put: 7.
        pair at: 2 put: 9
    ].
    ^ self assert:
        (((pair at: 1) = 7) and: [(pair at: 2) = 9])
!

testAtomicRollsBackOnError
    | pair |
    pair := self newPair.
    [
        Transaction atomic: [
            pair at: 1 put: 7.
            Error signal: 'rollback'
        ]
    ] on: Error do: [nil].
    ^ self assert:
        (((pair at: 1) = 1) and: [(pair at: 2) = 2])
!

testAtomicReturnsBlockValue
    | value |
    value := Transaction atomic: [41].
    ^ self assert: value equals: 41
!

testReadOnlyEvaluatesWithoutMutationProtocol
    | pair value |
    pair := self newPair.
    value := Transaction readOnly: [
        (pair at: 1) + (pair at: 2)
    ].
    ^ self assert: value equals: 3
!
