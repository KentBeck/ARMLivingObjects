TestCase subclass: #DurableTransactionTest instanceVariableNames: ''

!DurableTransactionTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runDurableTransactionTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 5.
    selectors at: 1 put: #testDurableUsesSameProgrammingModelAsAtomic.
    selectors at: 2 put: #testDurableReturnsBlockValue.
    selectors at: 3 put: #testDurableRollsBackOnError.
    selectors at: 4 put: #testDurableCommitSurvivesCheckpointAndRestart.
    selectors at: 5 put: #testDurableCommitReplaysWithoutPostCommitCheckpoint.
    ^ selectors
!

runDurableTransactionTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runDurableTransactionTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!DurableTransactionTest methodsFor: 'helpers'!
checkpointPath
    ^ '/tmp/arlo_durable_transaction.image'
!

durableTransactionKey
    ^ #DurableTransactionValue
!

newPair
    | pair |
    pair := Array new: 2.
    pair at: 1 put: 1.
    pair at: 2 put: 2.
    ^ pair
!

!DurableTransactionTest methodsFor: 'tests'!
testDurableUsesSameProgrammingModelAsAtomic
    | pair |
    pair := self newPair.
    Transaction durable: [
        pair at: 1 put: 17.
        pair at: 2 put: 19
    ].
    ^ self assert:
        (((pair at: 1) = 17) and: [(pair at: 2) = 19])
!

testDurableReturnsBlockValue
    | value |
    value := Transaction durable: [41].
    ^ self assert: value equals: 41
!

testDurableRollsBackOnError
    | pair |
    pair := self newPair.
    [
        Transaction durable: [
            pair at: 1 put: 17.
            Error signal: 'rollback'
        ]
    ] on: Error do: [nil].
    ^ self assert:
        (((pair at: 1) = 1) and: [(pair at: 2) = 2])
!

testDurableCommitSurvivesCheckpointAndRestart
    | path key |
    path := self checkpointPath.
    key := self durableTransactionKey.
    Transaction durable: [
        Dictionary smalltalk at: key put: 42
    ].
    Image checkpointTo: path.
    ^ self assert:
        (Image restartFrom: path valueOfGlobal: key) equals: 42
!

testDurableCommitReplaysWithoutPostCommitCheckpoint
    | path key |
    path := self checkpointPath.
    key := self durableTransactionKey.
    Dictionary smalltalk at: key put: 7.
    Image checkpointTo: path.
    Transaction durable: [
        Dictionary smalltalk at: key put: 42
    ].
    ^ self assert:
        (Image restartFrom: path valueOfGlobal: key) equals: 42
!
