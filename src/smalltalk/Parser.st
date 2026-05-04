"Parser — Recursive descent parser for Smalltalk"
"Instance variables: tokenizer"

Object subclass: #Parser instanceVariableNames: 'tokenizer'

!Parser class methodsFor: 'instance creation'!
on: aString
    ^ self new initializeOn: aString
!

!Parser methodsFor: 'initialization'!
initializeOn: aString
    tokenizer := Tokenizer on: aString.
    ^ self
!

!Parser methodsFor: 'parsing - entry points'!
parseExpression
    ^ self parseKeywordExpression
!

parseMethod
    | selector args token body argToken selectorBuffer argsBuffer |
    token := tokenizer next.

    token isIdentifier
        ifTrue: [
            selector := token text.
            args := Array new: 0.
            body := self parseMethodBody.
            ^ MethodNode selector: selector arguments: args body: body
        ]
        ifFalse: [0].

    token isBinarySelector
        ifTrue: [
            selector := token text.
            argToken := tokenizer next.
            argToken isIdentifier
                ifTrue: [0]
                ifFalse: [^ nil].
            args := Array with: argToken text.
            body := self parseMethodBody.
            ^ MethodNode selector: selector arguments: args body: body
        ]
        ifFalse: [0].

    token isKeyword
        ifTrue: [
            selectorBuffer := token text.
            argsBuffer := Array new: 8.
            ^ self parseKeywordMethodFrom: token selector: selectorBuffer args: argsBuffer argCount: 0
        ]
        ifFalse: [0].

    ^ nil
!

parseKeywordMethodFrom: firstKeyword selector: selectorSoFar args: argsBuffer argCount: count
    | argToken nextToken newSelector body finalArgs |
    argToken := tokenizer next.
    argToken isIdentifier
        ifTrue: [0]
        ifFalse: [^ nil].
    argsBuffer at: (count + 1) put: argToken text.

    nextToken := tokenizer peek.
    nextToken isKeyword
        ifTrue: [
            tokenizer next.
            newSelector := selectorSoFar , nextToken text.
            ^ self parseKeywordMethodFrom: nextToken selector: newSelector args: argsBuffer argCount: count + 1
        ]
        ifFalse: [
            finalArgs := self trimArray: argsBuffer to: count + 1.
            body := self parseMethodBody.
            ^ MethodNode selector: selectorSoFar arguments: finalArgs body: body
        ]
!

parseMethodBody
    | temps statements |
    temps := self parseTemporaries.
    statements := self parseStatements.
    ^ SequenceNode temporaries: temps statements: statements
!

!Parser methodsFor: 'parsing - statements'!
parseTemporaries
    | token temps |
    token := tokenizer peek.
    (token isSpecialText: '|')
        ifTrue: [
            tokenizer next.
            temps := Array new: 16.
            ^ self collectTemporaries: temps at: 1
        ]
        ifFalse: [0].

    token isBinarySelector
        ifTrue: [
            (token text = '|')
                ifTrue: [
                    tokenizer next.
                    temps := Array new: 16.
                    ^ self collectTemporaries: temps at: 1
                ]
                ifFalse: [0]
        ]
        ifFalse: [0].

    ^ Array new: 0
!

collectTemporaries: buffer at: index
    | token |
    token := tokenizer peek.
    token isIdentifier
        ifTrue: [
            tokenizer next.
            buffer at: index put: token text.
            ^ self collectTemporaries: buffer at: index + 1
        ]
        ifFalse: [0].
    (token isSpecialText: '|')
        ifTrue: [
            tokenizer next.
            ^ self trimArray: buffer to: index - 1
        ]
        ifFalse: [0].
    token isBinarySelector
        ifTrue: [
            (token text = '|')
                ifTrue: [
                    tokenizer next.
                    ^ self trimArray: buffer to: index - 1
                ]
                ifFalse: [0]
        ]
        ifFalse: [0].
    ^ self trimArray: buffer to: index - 1
!

parseStatements
    | stmts |
    stmts := Array new: 32.
    ^ self collectStatements: stmts at: 1
!

collectStatements: buffer at: index
    | stmt token |
    token := tokenizer peek.
    token isEof
        ifTrue: [^ self trimArray: buffer to: index - 1]
        ifFalse: [0].
    (token isSpecialText: ']')
        ifTrue: [^ self trimArray: buffer to: index - 1]
        ifFalse: [0].

    stmt := self parseStatement.
    stmt == nil
        ifTrue: [^ self trimArray: buffer to: index - 1]
        ifFalse: [0].

    buffer at: index put: stmt.

    token := tokenizer peek.
    (token isSpecialText: '.')
        ifTrue: [
            tokenizer next.
            ^ self collectStatements: buffer at: index + 1
        ]
        ifFalse: [^ self trimArray: buffer to: index]
!

parseStatement
    | token |
    token := tokenizer peek.

    (token isSpecialText: '^')
        ifTrue: [
            tokenizer next.
            ^ ReturnNode expression: self parseExpression
        ]
        ifFalse: [0].

    ^ self parseExpressionOrAssignment
!

parseExpressionOrAssignment
    | token expr nextToken |
    token := tokenizer peek.

    token isIdentifier
        ifTrue: [0]
        ifFalse: [^ self parseExpression]
.

    tokenizer next.
    nextToken := tokenizer peek.
    (nextToken isSpecialText: ':=')
        ifFalse: [
            tokenizer unread: token.
            ^ self parseExpression
        ].

    tokenizer next.
    ^ AssignmentNode
        variable: (VariableNode name: token text)
        value: self parseExpression
!

!Parser methodsFor: 'parsing - expressions'!
parseKeywordExpression
    | receiver token selectorBuffer argsBuffer arg |
    receiver := self parseBinaryExpression.
    receiver == nil
        ifTrue: [^ nil]
        ifFalse: [0].

    token := tokenizer peek.
    token isKeyword
        ifTrue: [0]
        ifFalse: [^ receiver].

    receiver isBlock
        ifTrue: [0]
        ifFalse: [
            selectorBuffer := String new: 64.
            argsBuffer := Array new: 8.
            ^ self parseKeywordContinuation: receiver selector: selectorBuffer args: argsBuffer at: 1 selectorAt: 1
        ].

    tokenizer next.
    arg := self parseBinaryExpression.
    ^ MessageNode receiver: receiver selector: token text arguments: (Array with: arg)
!

parseKeywordContinuation: receiver selector: selBuf args: argsBuf at: argIndex selectorAt: selIndex
    | token arg newSelIndex nextToken selector args |
    token := tokenizer peek.
    token isKeyword
        ifTrue: [
            tokenizer next.
            newSelIndex := self appendString: token text to: selBuf at: selIndex.
            arg := self parseBinaryExpression.
            arg == nil
                ifTrue: [^ nil]
                ifFalse: [0].
            argsBuf at: argIndex put: arg.
            ^ self parseKeywordContinuation: receiver selector: selBuf args: argsBuf at: argIndex + 1 selectorAt: newSelIndex
        ]
        ifFalse: [
            selector := self trimString: selBuf to: selIndex - 1.
            args := self trimArray: argsBuf to: argIndex - 1.
            ^ MessageNode receiver: receiver selector: selector arguments: args
        ]
!

parseBinaryExpression
    | receiver |
    receiver := self parseUnaryExpression.
    receiver == nil
        ifTrue: [^ nil]
        ifFalse: [0].
    ^ self parseBinaryContinuation: receiver
!

parseBinaryContinuation: receiver
    | token arg |
    token := tokenizer peek.
    token isBinarySelector
        ifTrue: [
            tokenizer next.
            arg := self parseUnaryExpression.
            arg == nil
                ifTrue: [^ nil]
                ifFalse: [0].
            ^ self parseBinaryContinuation: (MessageNode receiver: receiver selector: token text arguments: (Array with: arg))
        ]
        ifFalse: [^ receiver]
!

parseUnaryExpression
    | receiver |
    receiver := self parsePrimary.
    receiver == nil
        ifTrue: [^ nil]
        ifFalse: [0].
    ^ self parseUnaryContinuation: receiver
!

parseUnaryContinuation: receiver
    | token nextToken |
    token := tokenizer peek.
    token isIdentifier
        ifTrue: [
            tokenizer next.
            nextToken := tokenizer peek.
            (nextToken isSpecialText: ':=')
                ifTrue: [
                    tokenizer unread: token.
                    ^ receiver
                ]
                ifFalse: [
                    ^ self parseUnaryContinuation: (MessageNode receiver: receiver selector: token text)
                ]
        ]
        ifFalse: [^ receiver]
!

!Parser methodsFor: 'parsing - primary'!
parsePrimary
    | token |
    token := tokenizer peek.

    (token isSpecialText: '(')
        ifTrue: [^ self parseParenthesized]
        ifFalse: [0].

    (token isSpecialText: '[')
        ifTrue: [^ self parseBlock]
        ifFalse: [0].

    (token isSpecialText: '#(')
        ifTrue: [^ self parseLiteralArray]
        ifFalse: [0].

    ^ self parsePrimaryLiteral: token
!

parsePrimaryLiteral: token
    token isInteger
        ifTrue: [
            tokenizer next.
            ^ LiteralNode value: token value token: token
        ]
        ifFalse: [0].

    token isCharacter
        ifTrue: [
            tokenizer next.
            ^ LiteralNode value: token value asCharacter token: token
        ]
        ifFalse: [0].

    token isString
        ifTrue: [
            tokenizer next.
            ^ LiteralNode value: token text token: token
        ]
        ifFalse: [0].

    token isSymbol
        ifTrue: [
            tokenizer next.
            ^ LiteralNode value: token value token: token
        ]
        ifFalse: [0].

    token isIdentifier
        ifTrue: [
            tokenizer next.
            ^ VariableNode name: token text
        ]
        ifFalse: [0].

    ^ nil
!

parseParenthesized
    | expr token |
    tokenizer next.
    expr := self parseExpression.
    token := tokenizer next.
    (token isSpecialText: ')')
        ifTrue: [^ expr]
        ifFalse: [^ nil]
!

parseBlock
    | args temps stmts token |
    tokenizer next.

    args := self parseBlockArguments.
    temps := self parseTemporaries.
    stmts := self parseStatements.

    token := tokenizer next.
    (token isSpecialText: ']')
        ifTrue: [^ BlockNode arguments: args temporaries: temps statements: stmts]
        ifFalse: [^ nil]
!

parseBlockArguments
    | token args |
    token := tokenizer peek.
    (token isSpecialText: ':')
        ifTrue: [0]
        ifFalse: [^ Array new: 0].

    args := Array new: 8.
    ^ self collectBlockArguments: args at: 1
!

collectBlockArguments: buffer at: index
    | token argToken |
    token := tokenizer peek.
    (token isSpecialText: ':')
        ifTrue: [
            tokenizer next.
            argToken := tokenizer next.
            argToken isIdentifier
                ifTrue: [
                    buffer at: index put: argToken text.
                    ^ self collectBlockArguments: buffer at: index + 1
                ]
                ifFalse: [^ self trimArray: buffer to: index - 1]
        ]
        ifFalse: [0].

    (token isSpecialText: '|')
        ifTrue: [
            tokenizer next.
            ^ self trimArray: buffer to: index - 1
        ]
        ifFalse: [0].

    token isBinarySelector
        ifTrue: [
            (token text = '|')
                ifTrue: [
                    tokenizer next.
                    ^ self trimArray: buffer to: index - 1
                ]
                ifFalse: [^ self trimArray: buffer to: index - 1]
        ]
        ifFalse: [^ self trimArray: buffer to: index - 1]
!

parseLiteralArray
    | elements token |
    tokenizer next.
    elements := Array new: 32.
    ^ self collectLiteralArrayElements: elements at: 1
!

collectLiteralArrayElements: buffer at: index
    | token elem |
    token := tokenizer peek.

    (token isSpecialText: ')')
        ifTrue: [
            tokenizer next.
            ^ LiteralArrayNode elements: (self trimArray: buffer to: index - 1)
        ]
        ifFalse: [0].

    token isEof
        ifTrue: [^ nil]
        ifFalse: [0].

    (token isSpecialText: '#(')
        ifTrue: [
            elem := self parseLiteralArray.
            buffer at: index put: elem.
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    (token isSpecialText: '(')
        ifTrue: [
            tokenizer next.
            elem := self parseNestedLiteralArray.
            buffer at: index put: elem.
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    ^ self collectLiteralArrayToken: token into: buffer at: index
!

collectLiteralArrayToken: token into: buffer at: index
    token isInteger
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token value token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isSymbol
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token value token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isIdentifier
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text asSymbol token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isKeyword
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text asSymbol token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isString
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isCharacter
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token value asCharacter token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isBinarySelector
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text asSymbol token: token).
            ^ self collectLiteralArrayElements: buffer at: index + 1
        ]
        ifFalse: [0].

    ^ nil
!

parseNestedLiteralArray
    | elements |
    elements := Array new: 32.
    ^ self collectNestedLiteralElements: elements at: 1
!

collectNestedLiteralElements: buffer at: index
    | token elem |
    token := tokenizer peek.

    (token isSpecialText: ')')
        ifTrue: [
            tokenizer next.
            ^ LiteralArrayNode elements: (self trimArray: buffer to: index - 1)
        ]
        ifFalse: [0].

    token isEof
        ifTrue: [^ nil]
        ifFalse: [0].

    (token isSpecialText: '(')
        ifTrue: [
            tokenizer next.
            elem := self parseNestedLiteralArray.
            buffer at: index put: elem.
            ^ self collectNestedLiteralElements: buffer at: index + 1
        ]
        ifFalse: [0].

    ^ self collectNestedLiteralToken: token into: buffer at: index
!

collectNestedLiteralToken: token into: buffer at: index
    token isInteger
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token value token: token).
            ^ self collectNestedLiteralElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isIdentifier
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text asSymbol token: token).
            ^ self collectNestedLiteralElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isString
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token text token: token).
            ^ self collectNestedLiteralElements: buffer at: index + 1
        ]
        ifFalse: [0].

    token isSymbol
        ifTrue: [
            tokenizer next.
            buffer at: index put: (LiteralNode value: token value token: token).
            ^ self collectNestedLiteralElements: buffer at: index + 1
        ]
        ifFalse: [0].

    ^ nil
!

!Parser methodsFor: 'private - helpers'!
trimArray: arr to: size
    | result |
    size < 1
        ifTrue: [^ Array new: 0]
        ifFalse: [0].
    result := Array new: size.
    ^ self copyElements: arr to: result from: 1 to: size
!

copyElements: src to: dest from: index to: max
    index > max
        ifTrue: [^ dest]
        ifFalse: [
            dest at: index put: (src at: index).
            ^ self copyElements: src to: dest from: index + 1 to: max
        ]
!

trimString: str to: len
    | result |
    len < 1
        ifTrue: [^ '']
        ifFalse: [0].
    result := String new: len.
    ^ self copyChars: str to: result from: 1 to: len
!

copyChars: src to: dest from: index to: max
    index > max
        ifTrue: [^ dest]
        ifFalse: [
            dest at: index put: (src at: index).
            ^ self copyChars: src to: dest from: index + 1 to: max
        ]
!

appendString: aString to: buffer at: index
    ^ self appendChars: aString to: buffer srcIndex: 1 destIndex: index
!

appendChars: src to: dest srcIndex: si destIndex: di
    si > src size
        ifTrue: [^ di]
        ifFalse: [
            dest at: di put: (src at: si).
            ^ self appendChars: src to: dest srcIndex: si + 1 destIndex: di + 1
        ]
!
