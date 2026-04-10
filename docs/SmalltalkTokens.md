# Smalltalk Tokens

This repository's C bootstrap compiler defines its token types in [src/c/bootstrap_compiler.h](/Users/kentb/Dropbox/Mac/Documents/GitHub/ARMLivingObjects/src/c/bootstrap_compiler.h) and scans them in [src/c/bootstrap_compiler.c](/Users/kentb/Dropbox/Mac/Documents/GitHub/ARMLivingObjects/src/c/bootstrap_compiler.c).

## Lexical Tokens

These are the token kinds produced directly by `bt_next`.

| Token Type | Source Form | Examples | Meaning |
| --- | --- | --- | --- |
| `BTOK_EOF` | end of input | none | End of token stream |
| `BTOK_IDENTIFIER` | identifier | `foo`, `index`, `Tokenizer`, `thisContext`, `nil` | Name token |
| `BTOK_KEYWORD` | identifier ending in `:` | `at:`, `put:`, `ifTrue:`, `setString:` | Keyword-selector part |
| `BTOK_INTEGER` | decimal integer | `0`, `42`, `1234` | Small integer literal text |
| `BTOK_CHARACTER` | `$` followed by one char | `$A`, `$7`, `$ ` | Character literal |
| `BTOK_STRING` | single-quoted string | `'hello'`, `'hello''s'` | String literal |
| `BTOK_SYMBOL` | `#` followed by name or binary selector | `#foo`, `#bar:`, `#+` | Symbol literal |
| `BTOK_SPECIAL` | punctuation/operator token | `(` `)` `[` `]` `.` `;` `^` `|` `:` `:=` `+` `-` `*` `/` `<` `>` `=` `~` `&` `@` `%` `,` `?` `#(` | Structural or operator token |

## Compiler-Internal Token Categories

These token kinds appear in parsed/code-generated literals, but are not emitted directly as standalone lexical tokens by `bt_next`.

| Token Type | Source Form | Examples | Meaning |
| --- | --- | --- | --- |
| `BTOK_SELECTOR` | synthesized selector literal | `+`, `at:put:` | Message selector stored in compiled literals |
| `BTOK_BLOCK_LITERAL` | block body | `[x + 1]` | Compiled block literal |
| `BTOK_LITERAL_ARRAY` | literal array | `#(1 #foo 'bar' $A)` | Whole literal array literal |
| `BTOK_CLASS_REF` | global/class reference | `Tokenizer`, `Array`, `ReadStream` | Global name resolved through `Smalltalk` |

## Smalltalk-Level Categories Recognized Today

- identifiers
- keyword selector parts
- binary selectors
- integers
- characters
- strings
- symbols
- literal arrays
- block delimiters
- parentheses
- assignment
- return
- statement separator
- cascade separator
- temp bars

## Not Yet Tokenized As Distinct Kinds

- floating-point literals
- scaled decimals
- pragma tokens
- byte-array literals
- comments as first-class tokens
- signed numbers as one token

`-3` is currently tokenized as `-` plus `3`, not as a single numeric token.
