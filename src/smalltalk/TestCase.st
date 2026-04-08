"TestCase — minimal xUnit test case without exceptions"
"Instance variables: selector, result, failed, failureReason"

!TestCase class methodsFor: 'instance creation'!
selector: aSelector
    ^ (self new selector: aSelector) yourself
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
    self setUp.
    self perform: selector.
    self tearDown.
    failed
        ifTrue: [aResult recordFailure: self selector: selector reason: failureReason]
        ifFalse: [aResult recordPass: self selector: selector].
    ^ aResult
!

!TestCase methodsFor: 'asserting'!
assert: aBoolean
    ^ self assert: aBoolean description: #assertionFailed
!

assert: aBoolean description: aSymbol
    aBoolean
        ifTrue: [^ true]
        ifFalse: [
            failed := true.
            failureReason := aSymbol.
            false
        ]
!

assert: actual equals: expected
    ^ self assert: (actual = expected) description: #equalityFailed
!

deny: aBoolean
    ^ self deny: aBoolean description: #denialFailed
!

deny: aBoolean description: aSymbol
    aBoolean
        ifTrue: [
            failed := true.
            failureReason := aSymbol.
            ^ false
        ]
        ifFalse: [^ true]
!
