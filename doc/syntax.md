# Syntax


## Lexical

This section describes how Kenaf source code is broken into individual tokens.


### Whitespace and Comments

Whitespace is not syntactically significant, and can be inserted freely
inbetween tokens.

Whitespace consists of space `' '` or tab `'\t'` characters, newlines, and
comments.

A newline is a carriage return `'\r'`, a line feed `'\n'`, or the two-character
CRLF sequence `'\r\n'` are newlines.

A line comment is introduced with `'--'` and is terminated at the next newline.

A block comment is intoduced with `'/*'` and is terminated by `'*/'`.  Block
comments must be closed before the end of the script.  Comments do not nest.


### Names

A name - also called an identifier - is a character sequence matching the
regular expression:

```regexp
[A-Za-z_][A-Za-z0-9_]*
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




### Strings



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

## Grammar

Kenaf's grammar is presented in extended BNF.

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
