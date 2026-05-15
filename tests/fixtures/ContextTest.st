TestCase subclass: #ContextTest instanceVariableNames: ''

!ContextTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runContextTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 13.
    selectors at: 1 put: #testThisContextReceiverIsSelf.
    selectors at: 2 put: #testRepeatedThisContextIsIdentical.
    selectors at: 3 put: #testSenderReceiverIsSelf.
    selectors at: 4 put: #testBlockContextReceiverIsSelf.
    selectors at: 5 put: #testBlockContextHomeReceiverIsSelf.
    selectors at: 6 put: #testBlockContextHasClosure.
    selectors at: 7 put: #testStringConcatenationProducesExpectedContents.
    selectors at: 8 put: #testArrayAtPutRoundTripsValue.
    selectors at: 9 put: #testStringAtPutRoundTripsByteValue.
    selectors at: 10 put: #testSymbolEqualityDistinguishesSameAndDifferentSymbols.
    selectors at: 11 put: #testStringEqualityDistinguishesSameAndDifferentContents.
    selectors at: 12 put: #testStringAsSymbolInternsContents.
    selectors at: 13 put: #testStringHashIsStableAndMatchesEqualContents.
    ^ selectors
!

runContextTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runContextTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!ContextTest methodsFor: 'helpers'!
captureCurrentContext
    ^ thisContext
!

captureCallerContext
    ^ thisContext sender
!

captureBlockContext
    ^ [thisContext] value
!

!ContextTest methodsFor: 'tests'!
testThisContextReceiverIsSelf
    ^ self assert: thisContext receiver == self
!

testRepeatedThisContextIsIdentical
    | first second |
    first := thisContext.
    second := thisContext.
    ^ self assert: first == second
!

testSenderReceiverIsSelf
    | caller |
    caller := self captureCallerContext.
    ^ self assert: caller receiver == self
!

testBlockContextReceiverIsSelf
    | blockContext |
    blockContext := self captureBlockContext.
    ^ self assert: blockContext receiver == self
!

testBlockContextHomeReceiverIsSelf
    | blockContext homeContext |
    blockContext := self captureBlockContext.
    homeContext := blockContext home.
    ^ self assert:
        ((homeContext == nil)
            ifTrue: [false]
            ifFalse: [homeContext receiver == self])
!

testBlockContextHasClosure
    | blockContext |
    blockContext := self captureBlockContext.
    ^ self assert: (blockContext closure == nil) == false
!

testStringConcatenationProducesExpectedContents
    ^ self assert: ('ab' , 'cd') equals: 'abcd'
!

testArrayAtPutRoundTripsValue
    | arr |
    arr := Array new: 3.
    arr at: 2 put: 77.
    self assert: arr size equals: 3.
    ^ self assert: (arr at: 2) equals: 77
!

testStringAtPutRoundTripsByteValue
    | string |
    string := String new: 3.
    string at: 2 put: 65.
    self assert: string size equals: 3.
    ^ self assert: (string at: 2) equals: 65
!

testSymbolEqualityDistinguishesSameAndDifferentSymbols
    self assert: (#alpha = #alpha).
    self assert: ('alpha' asSymbol = #alpha).
    ^ self deny: (#alpha = #beta)
!

testStringEqualityDistinguishesSameAndDifferentContents
    self assert: ('alpha' = 'alpha').
    self assert: ('ab' , 'cd') = 'abcd'.
    ^ self deny: ('alpha' = 'beta')
!

testStringAsSymbolInternsContents
    self assert: ('alpha' asSymbol) equals: #alpha.
    ^ self assert: ('ab' , 'cd') asSymbol equals: #abcd
!

testStringHashIsStableAndMatchesEqualContents
    | value |
    value := 'alpha' hash.
    self assert: value equals: 'alpha' hash.
    ^ self assert: value equals: ('al' , 'pha') hash
!
