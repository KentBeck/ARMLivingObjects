"TestCase — minimal xUnit test case"
"Instance variables: selector, result"

Object subclass: #TestCase instanceVariableNames: 'selector result'

!TestCase class methodsFor: 'instance creation'!
selector: aSelector
    ^ (self new selector: aSelector) yourself
!

!TestCase class methodsFor: 'self testing'!
selfTest
    | result |
    result := TestResult new.
    ^ self runSelfTestOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    ^ Array new: 0
!

runSelfTestOn: aResult selectors: selectors startingAt: index
    | currentIndex |
    currentIndex := index.
    [currentIndex < (selectors size + 1)] whileTrue: [
        (self selector: (selectors at: currentIndex)) runOn: aResult.
        currentIndex := currentIndex + 1
    ].
    ^ aResult
!

!TestCase methodsFor: 'accessing'!
selector
    ^ selector
!

selector: aSelector
    selector := aSelector.
    ^ aSelector
!

result
    ^ result
!

!TestCase methodsFor: 'running'!
setUp
    ^ self
!

tearDown
    ^ self
!

runOn: aResult
    result := aResult.
    ^ [
        self setUp.
        self perform: selector.
        self tearDown.
        aResult recordPass: self selector: selector
    ] on: TestFailure do: [:ex |
        self tearDown.
        aResult recordFailure: self selector: selector reason: ex description
    ]
!

!TestCase methodsFor: 'asserting'!
assert: aBoolean
    ^ self assert: aBoolean description: #assertionFailed
!

assert: aBoolean description: aSymbol
    aBoolean
        ifTrue: [^ true]
        ifFalse: [^ self failWith: aSymbol]
!

failWith: aSymbol
    ^ TestFailure signal: aSymbol
!

assert: actual equals: expected
    ^ self assert: (actual = expected) description: #equalityFailed
!

deny: aBoolean
    ^ self deny: aBoolean description: #denialFailed
!

deny: aBoolean description: aSymbol
    aBoolean
        ifTrue: [^ self failWith: aSymbol]
        ifFalse: [^ true]
!
