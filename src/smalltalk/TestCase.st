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
    | selector |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            (self selector: selector) runOn: aResult.
            self runSelfTestOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
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
    | failureCountBefore |
    result := aResult.
    failureCountBefore := aResult failureCount.
    self setUp.
    self perform: selector.
    self tearDown.
    ^ (aResult failureCount = failureCountBefore)
        ifTrue: [aResult recordPass: self selector: selector]
        ifFalse: [self]
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
    result recordFailure: self selector: selector reason: aSymbol.
    ^ false
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
