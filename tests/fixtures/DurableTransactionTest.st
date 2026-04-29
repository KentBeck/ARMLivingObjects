TestCase subclass: #DurableTransactionTest instanceVariableNames: ''

!DurableTransactionTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runDurableTransactionTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 3.
    selectors at: 1 put: #testDurableUsesSameProgrammingModelAsAtomic.
    selectors at: 2 put: #testDurableReturnsBlockValue.
    selectors at: 3 put: #testDurableCommitSurvivesCheckpointAndRestart.
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

newPoint
    ^ PointFile x: 1 y: 2
!

restartFromCheckpoint: aPath verify: aBlock
    ^ Image restartFrom: aPath verify: aBlock
!

!DurableTransactionTest methodsFor: 'tests'!
testDurableUsesSameProgrammingModelAsAtomic
    | point |
    point := self newPoint.
    Transaction durable: [
        point x: 17.
        point y: 19
    ].
    ^ self assert:
        ((point x = 17) and: [point y = 19])
!

testDurableReturnsBlockValue
    | value |
    value := Transaction durable: [41].
    ^ self assert: value equals: 41
!

testDurableCommitSurvivesCheckpointAndRestart
    | path key |
    path := self checkpointPath.
    key := self durableTransactionKey.
    Transaction durable: [
        Smalltalk at: key put: 42
    ].
    Image checkpointTo: path.
    ^ self restartFromCheckpoint: path verify: [
        self assert: (Smalltalk at: key) equals: 42
    ]
!
