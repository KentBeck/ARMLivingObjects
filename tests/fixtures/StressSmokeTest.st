TestCase subclass: #StressSmokeTest instanceVariableNames: ''

!StressSmokeTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    self suite runOn: result.
    ^ result
!

suite
    | suite selectors index |
    suite := TestSuite new.
    selectors := self testSelectors.
    index := 1.
    [index < (selectors size + 1)] whileTrue: [
        | testCase |
        testCase := self new.
        testCase selector: (selectors at: index).
        suite add: testCase.
        index := index + 1
    ].
    ^ suite
!

testSelectors
    | selectors |
    selectors := Array new: 7.
    selectors at: 1 put: #testGarbageChurnAllocatesAndReturnsStableValue.
    selectors at: 2 put: #testLargeArraySummaryMatchesExpectedValue.
    selectors at: 3 put: #testLongChainSummaryMatchesExpectedValue.
    selectors at: 4 put: #testWideFanoutSummaryMatchesExpectedValue.
    selectors at: 5 put: #testDeepRecursionMaterializesContextsAndReturns.
    selectors at: 6 put: #testCheckpointRestartAfterMixedAllocation.
    selectors at: 7 put: #testDurableReplayAfterMixedAllocation.
    ^ selectors
!

failureInjectionSelector
    ^ #testCheckpointFailureSignalsError
!

!StressSmokeTest methodsFor: 'paths'!
checkpointPath
    ^ '/tmp/arlo_stress_smoke.image'
!

checkpointKey
    ^ #StressSmokeCheckpointValue
!

durableReplayKey
    ^ #StressSmokeDurableReplayValue
!

!StressSmokeTest methodsFor: 'helpers'!
churnGarbage: remaining
    | scratch large |
    remaining = 0 ifTrue: [^ 0].
    scratch := Array new: 16.
    self fillArray: scratch startingAt: 1 base: remaining.
    large := Array new: 96.
    large at: 1 put: remaining.
    large at: 96 put: remaining + 96.
    ^ self churnGarbage: remaining - 1
!

makeLargeArraySized: size
    | array |
    array := Array new: size.
    self fillArrayIteratively: array base: 10.
    ^ array
!

fillArray: anArray startingAt: index base: base
    index > anArray size
        ifTrue: [^ anArray]
        ifFalse: [
            anArray at: index put: base + index.
            ^ self fillArray: anArray startingAt: index + 1 base: base
        ]
!

fillArrayIteratively: anArray base: base
    | index |
    index := 1.
    [index < (anArray size + 1)] whileTrue: [
        anArray at: index put: base + index.
        index := index + 1
    ].
    ^ anArray
!

sumArray: anArray
    | index total |
    index := 1.
    total := 0.
    [index < (anArray size + 1)] whileTrue: [
        total := total + (anArray at: index).
        index := index + 1
    ].
    ^ total
!

makeChainOfLength: remaining
    | node |
    remaining = 0 ifTrue: [^ nil].
    node := Array new: 2.
    node at: 1 put: remaining.
    node at: 2 put: (self makeChainOfLength: remaining - 1).
    ^ node
!

sumChain: aNode
    aNode == nil
        ifTrue: [^ 0]
        ifFalse: [^ (aNode at: 1) + (self sumChain: (aNode at: 2))]
!

makeFanoutDepth: depth base: base
    | node |
    node := Array new: 4.
    depth = 0
        ifTrue: [
            node at: 1 put: base.
            node at: 2 put: base + 1.
            node at: 3 put: base + 2.
            node at: 4 put: base + 3.
            ^ node
        ]
        ifFalse: [
            node at: 1 put: (self makeFanoutDepth: depth - 1 base: base).
            node at: 2 put: (self makeFanoutDepth: depth - 1 base: base + 10).
            node at: 3 put: (self makeFanoutDepth: depth - 1 base: base + 20).
            node at: 4 put: (self makeFanoutDepth: depth - 1 base: base + 30).
            ^ node
        ]
!

sumFanout: aNode depth: depth
    depth = 0
        ifTrue: [
            ^ (aNode at: 1) + (aNode at: 2) + (aNode at: 3) + (aNode at: 4)
        ]
        ifFalse: [
            ^ (self sumFanout: (aNode at: 1) depth: depth - 1)
                + (self sumFanout: (aNode at: 2) depth: depth - 1)
                + (self sumFanout: (aNode at: 3) depth: depth - 1)
                + (self sumFanout: (aNode at: 4) depth: depth - 1)
        ]
!

deepContexts: depth
    | context holder |
    context := thisContext.
    holder := Array new: 2.
    holder at: 1 put: depth.
    holder at: 2 put: context sender.
    depth = 0
        ifTrue: [^ 0]
        ifFalse: [^ [1 + (self deepContexts: depth - 1)] value]
!

!StressSmokeTest methodsFor: 'tests'!
testGarbageChurnAllocatesAndReturnsStableValue
    ^ self assert: (self churnGarbage: 80) equals: 0
!

testLargeArraySummaryMatchesExpectedValue
    | large total |
    large := self makeLargeArraySized: 1500.
    total := self sumArray: large.
    ^ self assert: total equals: 1148250
!

testLongChainSummaryMatchesExpectedValue
    | chain total |
    chain := self makeChainOfLength: 120.
    total := self sumChain: chain.
    ^ self assert: total equals: 7260
!

testWideFanoutSummaryMatchesExpectedValue
    | fanout total |
    fanout := self makeFanoutDepth: 3 base: 1.
    total := self sumFanout: fanout depth: 3.
    ^ self assert: total equals: 3520
!

testDeepRecursionMaterializesContextsAndReturns
    ^ self assert: (self deepContexts: 80) equals: 80
!

testCheckpointRestartAfterMixedAllocation
    | path key large chain fanout summary |
    path := self checkpointPath.
    key := self checkpointKey.
    self churnGarbage: 30.
    large := self makeLargeArraySized: 1500.
    chain := self makeChainOfLength: 64.
    fanout := self makeFanoutDepth: 2 base: 5.
    summary := (large at: 1)
        + (large at: 1500)
        + (self sumChain: chain)
        + (self sumFanout: fanout depth: 2)
        + (self deepContexts: 24).
    Dictionary smalltalk at: key put: summary.
    Image checkpointTo: path.
    ^ self assert:
        (Image restartFrom: path valueOfGlobal: key) equals: summary
!

testDurableReplayAfterMixedAllocation
    | path key chain |
    path := self checkpointPath.
    key := self durableReplayKey.
    chain := self makeChainOfLength: 40.
    Dictionary smalltalk at: key put: (self sumChain: chain).
    Image checkpointTo: path.
    self churnGarbage: 20.
    Transaction durable: [
        Dictionary smalltalk at: key put: ((self sumChain: chain) + 99)
    ].
    ^ self assert:
        (Image restartFrom: path valueOfGlobal: key) equals: ((self sumChain: chain) + 99)
!

testCheckpointFailureSignalsError
    | handled |
    handled := [
        Image checkpointTo: self checkpointPath.
        false
    ] on: Error do: [true].
    ^ self assert: handled
!
