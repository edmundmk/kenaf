# Grammar

Kenaf's lexical and syntactic grammars are described here.  Grammar definitions
are expressed using [ENBF](https://en.wikipedia.org/wiki/Extended_Backus–Naur_form).
Terminal strings of the form `'a..z'` indicate any one character in the range
from `'a'` to `'z'`.


## Lexical

### Whitespace and Comments

Whitespace is not syntactically significant, and can be inserted freely
inbetween tokens.

Whitespace consists of space `' '` or tab `'\t'` characters, newlines, and
comments.

A newline is a carriage return `'\r'`, a line feed `'\n'`, or the two-character
CRLF sequence `'\r\n'`.

A line comment is introduced with `'--'` and is terminated at the next newline.

A block comment is intoduced with `'/*'` and is terminated by `'*/'`.  Block
comments must be closed before the end of the script.  Comments do not nest.


### Names

A name - also called an identifier - is a character sequence matching the
following grammar:

```EBNF
name = alphabetical {alphanumeric} ;
alphabetical = 'A..Z' | 'a..z' | '_' ;
alphanumeric = alphabetical | '0..9' ;
```

The following tokens are keywords with special syntactic meaning.  Keywords
may only be used where they are permitted by the grammar - a keyword cannot
be used as a name.

```
and         else        not         throw
break       end         null        true
continue    false       or          until
def         for         repeat      var
do          if          return      while
elif        is          then        yield
```


### Numbers

A number matches the following grammar:

```EBNF
number = decimal_number | hexadecimal_number | octal_number ;

decimal_number =
    '0' ['.' {'0..9'}] [decimal_exponent]
  | '1..9' {'0..9'} ['.' {'0..9'}] [decimal_exponent]
  | '.' '0..9' {'0..9'} [decimal_exponent]
  ;

decimal_exponent = ('e' | 'E') ['+' | '-'] '0..9' {'0..9'} ;

hexadecimal_number =
    '0x' hexdigit {hexdigit} ['.' {hexdigit}] [hexadecimal_exponent]
  | '0x' '.' hexdigit {hexdigit} [hexadecimal_exponent]
  ;

hexadecimal_exponent = ('p' | 'P') ['+' |'-'] '0..9' {'0..9'} ;

octal_number = '0o' '0..7' {'0..7'} ;

hexdigit = '0..9' | 'A..F' | 'a..f' ;
```

A decimal number cannot be prefixed with `'0'`.  This eliminates confusion with
C-style octal literals.

A number must have at least one digit in either its  fractional or integer
parts.  A lone decimal point `'.'` is the period symbol.

Octal numbers must be integers, and cannot have a fractional or exponent part.


### Strings

Lexically, a string is a sequence of characters between an opening quotation
mark `'"'` and a closing one.

Inside the quotation marks the backslash `'\'` introduces one of the following
escape sequences:

| Escape        | Result                                                      |
|---------------|-------------------------------------------------------------|
| `'\"'`        | Quotation mark `'"'`, does not close the string.            |
| `'\\'`        | Backslash `'\'`.                                            |
| `'\0'`        | Null character `U+0000` or zero byte.                       |
| `'\b'`        | Backspace `U+0008`.                                         |
| `'\f'`        | Form feed `U+000C`.                                         |
| `'\n'`        | Line feed `U+000A`.                                         |
| `'\r'`        | Carriage return `U+000D`.                                   |
| `'\t'`        | Horizontal tab `U+0009`.                                    |
| `'\v'`        | Vertical tab `U+000B`.                                      |
| `'\xXX'`      | Byte from two hexadecimal digits.                           |
| `'\uXXXX'`    | Unicode character in range `'0..FFFF'`, encoded as UTF-8.   |
| `'\U+XXXXXX'` | Unicode character in range `'0..10FFFF'`, encoded as UTF-8. |

Unescaped newlines are not valid inside a string.  The string must be closed
before the end of the script.

Byte and unicode escapes must have exactly the number of hexadecimal digits
shown.  Characters are encoded as a sequence of UTF-8 encoding units.  However,
a string may contain arbitrary byte sequences, and need not decode cleanly to
UTF-8.


### Operators and Symbols

The following character sequences are tokens.  Lexical analysis is greedy -
the longest matching token is produced.

```
-       (       //      %=      <<=     >>=
-=      )       //=     ^       <=      |
,       [       /=      ^=      =       |=
:       ]       &       +       ==      ~
!=      *       &=      +=      >       ~=
.       *=      #       <       >=      ~>>
...     /       %       <<      >>      ~>>=
```

## Syntactic

Here is the entire syntactic grammar of Kenaf:

```EBNF
script = block ;

block = {statement [';'] | last_statement ';'} [last_statement] ;

statement =
    definition
  | assignment_statement
  | call_expression
  | 'yield' 'for' call_expression
  | 'do' block 'end'
  | 'if' expression 'then' block {'elif' expression 'then' block} ['else' block] 'end'
  | 'for' name_list '=' expression ':' expression [':' expression] 'do' block 'end'
  | 'for' name_list ':' expression 'do' block 'end'
  | 'while' expression 'do' block 'end'
  | 'repeat' block 'until' expression
  | 'break'
  | 'continue'
  | 'throw' expression
  | 'return' unpack_list
  | 'yield' unpack_list
;

last_statement = 'return' ;

definition =
    'def' qualified_name ['is' expression] {object_key} 'end'
  | 'def' ['yield'] qualified_name '(' [parameter_list] ')' block 'end'
  ;

qualified_name = name ['.' name] ;

parameter_list = {name ','} name ['...'] ;

object_key = definition | name ':' expression [';'] ;

assignment_statement =
    'var' name_list ['=' rvalue_list]
  | lvalue_list '=' rvalue_list
  | lvalue_list compound_assignment rvalue_list
  ;

name_list = name {',' name} ;

lvalue_list = lvalue {',' lvalue} ;

rvalue_list =
  | unpack_list
  | 'yield' ['...'] unpack_list
  | 'yield' 'for' call_expression ['...']
  | lvalue_list '=' rvalue_list
  | lvalue_list compound_assignment rvalue_list
  ;

compound_assignment = '*=' | '/=' | '//=' | '%=' | '+=' | '-=' |
        '~=' | '<<=' | '>>=' | '~>>=' | '&=' | '^=' | '|=' ;

unpack_list = {expression ','} expression ['...'] ;

expression = constructor | array | table | arithmetic ;

constructor =
    'def' ['is' expression] {object_key} end
  | 'def' ['yield'] '(' [parameter_list] ')' block 'end'
  ;

array = '[' ']' | '[' {expression ','} expression ['...' | ','] ']' ;

table = '[' ':' ']' | '[' {key_value ','} key_value [','] ']' ;

key_value = expression ':' expression ;

arithmetic =
  | if_expression
  | arithmetic 'or' arithmetic
  | arithmetic 'and' arithmetic
  | 'not' arithmetic
  | arithmetic comparison arithmetic {comparison arithmetic}
  | arithmetic '|' arithmetic
  | arithmetic '^' arithmetic
  | arithmetic '&' arithmetic
  | arithmetic '<<' arithmetic
  | arithmetic '>>' artihmetic
  | arithemtic '~>>' arithmetic
  | arithemtic '+' arithmetic
  | arithmetic '-' arithmetic
  | arithemtic '~' arithmetic
  | arithmetic '*' arithmetic
  | arithmetic '/' arithmetic
  | arithmetic '//' arithmetic
  | arithmetic '%' arithmetic
  | '#' arithmetic
  | '-' arithmetic
  | '+' arithmetic
  | '~' arithmetic
  | prefix
  | literal
  ;

if_expression = 'if' arithmetic 'then' expression
        {'elif' expression 'then' expression} 'else' expression ;

comparison = '==' | '!=' | '<' | '<=' | '>' | '>=' | 'is' | 'is' 'not' ;

call_expression = prefix '(' [unpack_list] ')' ;

prefix = lvalue | call_expression | '(' expression ')' ;

lvalue = name | prefix '.' name | prefix '[' expression ']' ;

literal = 'null' | 'false' | 'true' | number | string ;
```
