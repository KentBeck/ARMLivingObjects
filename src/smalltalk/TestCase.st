"TestCase — minimal xUnit test case without exceptions"
"Instance variables: selector, result, failed, failureReason, failureBacktrace"

Object subclass: #TestCase instanceVariableNames: 'selector result failed failureReason failureBacktrace'

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

failureReason
    ^ failureReason
!

failureBacktrace
    ^ failureBacktrace
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
    failed := false.
    failureReason := nil.
    failureBacktrace := nil.
    self setUp.
    self perform: selector.
    self tearDown.
    failed
        ifTrue: [aResult recordFailure: self selector: selector reason: failureReason backtrace: failureBacktrace]
        ifFalse: [aResult recordPass: self selector: selector].
    ^ aResult
!

!TestCase methodsFor: 'asserting'!
assert: aBoolean
    ^ self assert: aBoolean description: #assertionFailed context: thisContext sender
!

assert: aBoolean description: aSymbol
    ^ self assert: aBoolean description: aSymbol context: thisContext sender
!

assert: aBoolean description: aSymbol context: aContext
    aBoolean
        ifTrue: [^ true]
        ifFalse: [^ self failWith: aSymbol context: aContext]
!

failWith: aSymbol context: aContext
    failed := true.
    failureReason := aSymbol.
    failureBacktrace == nil
        ifTrue: [failureBacktrace := self backtraceFrom: aContext]
        ifFalse: [failureBacktrace].
    ^ false
!

assert: actual equals: expected
    ^ self assert: (actual = expected) description: #equalityFailed context: thisContext sender
!

deny: aBoolean
    ^ self deny: aBoolean description: #denialFailed context: thisContext sender
!

deny: aBoolean description: aSymbol
    ^ self deny: aBoolean description: aSymbol context: thisContext sender
!

deny: aBoolean description: aSymbol context: aContext
    aBoolean
        ifTrue: [^ self failWith: aSymbol context: aContext]
        ifFalse: [^ true]
!

backtraceFrom: aContext
    | result size |
    size := self backtraceSizeOf: aContext.
    result := Array new: size.
    ^ self writeBacktrace: aContext into: result at: 1
!

backtraceSizeOf: aContext
    ^ (aContext == nil)
        ifTrue: [0]
        ifFalse: [1 + (self backtraceSizeOf: aContext sender)]
!

writeBacktrace: aContext into: anArray at: index
    ^ (aContext == nil)
        ifTrue: [anArray]
        ifFalse: [
            anArray at: index put: aContext.
            self writeBacktrace: aContext sender into: anArray at: index + 1
        ]
!
