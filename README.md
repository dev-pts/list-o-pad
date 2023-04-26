LOP ("List-O-Pad" ... I'm bad at namings ...) looks like (from the inside and outside)
Lisp, Scheme ("Sweet-expressions"), XML, YAML, PARI/GP, Python, Ruby, and Nim,
but with its own rules, restrictions, and features.

Rules/features/restrictions include:
* tabs, commas, and newlines have meaning
* strict indentation rules
* fixed recognizable tokens: `identifier`, `number`, `operator`, `string`, `list`
* different types of lists: `()`, `[]`, `{}`, `:;`
* different types of list applications: `a()`, `a[]`, `a{}`, `a:;`
* string applications: `a""`, `a''`
* identifiers are C-friendly identifiers, no strange characters allowed
* strange characters are operators here
* numbers are identifiers but their first character is a digit `0-9`
* infix binary expressions
* prefix unary expressions
* configurable operator precedence
* schema to verify and parse

See:
* include/LOP.h
* examples/

# How does it look like?

For example (examples/fancy-lisp/), a fancy Lisp without extensive () could look like this:
```
define: hanoi(n, src, dest, spare)
	if: n == 1
		write("Move from ", src, " to ", dest, "\n")
	else:
		hanoi(n - 1, src, spare, dest)
		hanoi(1, src, dest, spare)
		hanoi(n - 1, spare, dest, src)
```

It will be translated into the proper Lisp code by the help of supporting C-code:
```
(define hanoi
    (lambda (n src dest spare)
        (if (eq? n 1)
            (write "Move from " src " to " dest "\n")
            (hanoi (- n 1) src spare dest)
            (hanoi 1 src dest spare)
            (hanoi (- n 1) spare dest src))))
```

The schema for the supporting C-code looks like this:
```
: #operators
	{
	}

	unary: '+', '-'

	binary_left_to_right: '*', '/'
	binary_left_to_right: '+', '-'
	binary_left_to_right: '=='

	binary_right_to_left: '='

top:
	tlist:
		listof: #optional
			$expr

expr:
	oneof:
		oneof: @expr
			tree: @define
				identifier: 'define'
				call:
					identifier: @symbol
					listof: @lambda
						identifier: @symbol
...
```

# Some sort of specification

* There are 4 types of symbols:
  1. `identifier` - `[[:alpha:]\_][[:alnum:]\_]+`:
     ```
     abc12
     _ef_g34h
     ```
  2. `number` - `\d<identifier>`:
     ```
     123
     0x456
     954jrewj432
     0x1234_5678
     ```
  3. `operator` - `[.~!@#$%^&*+-=<>/?|]+`:
     ```
     ++
     -
     ==
     &&=
     ```
  4. `string` - everything between `""` or `''`

  NOTE `123+abc-&3d4` can be splitted into:
    number (`123`), operator (`+`), identifier (`abc`), operator (`-&`), and number (`3d4`)
* Comma is a separator of the terms (the term is a symbol or a list (see below)):
  `a, b`
* There are 4 types of explicit lists with start and end characters like:
  1. `(...)` - this list has subtype of `list`:
     `(a, b, c)`
  2. `[...]` - `alist`:
     `[a, b, c]`
  3. `{...}` - `slist`:
     `{ a, b, c }`
  4. `:...;` - `tlist`:
     `: a, b, c;`
* There are 2 types of implicit lists:
  1. `binary`:
     ```
     a + b
     c * d
     ```
  2. `unary`:
     ```
     -a
     !b
     ```
* Explicit list after the term becomes a 'call'-type:
  1. `...(...)` - now this list has subtype of `call`:
     `a(b)`
  2. `...[...]` - `aref`:
     `a[b]`
  3. `...{...}` - `struct`:
     `a { b }`
  4. `...:...;` - `tree`:
     `a: b;`
* Strings can become `call`-type too:
  ```
  a"b"
  a'b'
  ```
* Newline is a separator of the terms:
  ```
  c
  d
  ```
  is the same as:
  `c, d`
* Tabs are essential after the newline, they set an indentation level
* 'tlist' is the only list for which closing character (`;`) can be omitted:
  ```
  a:
  	b
  	c
  d:
  	e
  ```
  These are 2 lists: tlist(a, b, c) and tlist(d, e)
* All lists require the proper indentation for their children and closing character
* The closing character of the list (or `'`, or `"`) must be on the same indentation level as the open character (or `'`, or `"`)
* Explicit lists increase required indentation for their children by 1 to the parent's required indentation:
  ```
  a: b:
  		c: d:
  				e: f
  a:
  	b:
  		c:
  			d:
  				e:
  					f
  a: b: c: d: e: f
  ```
  These are the same lists.
* The first implicit list increases required indentation by 1, but its children implicit lists do not:
  ```
  a && b ||
  	c && d ||
  	e && f
  ```
* Everything starts with the root 'tlist' which is given by default
* Comments goes after the `\``:
  ```
  `this is a comment
  ```

# Schema

Schema is a LOP-file with the entry rule and other rules to verify and parse other LOP-files.
There is a root schema which verifies the syntax of other schemas.
Please, see src/ASTSchema.c and examples/ to understand how schemas work.

Schema is like a DOCTYPE in XML, but with adopted syntax and explicit callbacks.

First, schema verifies the LOP-file. Then, the callbacks are called.
Callbacks implementation must be provided by the supporting code.
Every callback must explicitly call the children callbacks so that all callbacks are to be called.

# Schema syntax highlights

* The first entry is a table for allowed operators and their precedence:
```
: #operators
	{ `here goes a table for the namespace resolution operators, they are for proper list applications
		unary: '$'
		binary_left_to_right: '.', '->'
	}

	`every 'binary_XXX' and 'unary' entry lowers the precedence
	binary_left_to_right: '*', '/' `these have precedence 1
	binary_left_to_right: `and these - 2
		'+'
		'-'
	binary_right_to_left: '=' `this - 3
```
* Then goes the rules which looks like this:
```
<identifier>:
	...
```
* Every rule then describes the allowed structure of the LOP file by using:
  * sequential operators:
    * `oneof` - picks one of the variants:
      `oneof: a, b, c`
    * `listof` - picks one of the variants until fail:
      `listof: a, b, c`
    * `seqof` - picks the variants in the exact order:
      `seqof: a, b, c`
  * possible AST symbol node types:
    * `number` - picks any number
    * `identifier` - picks any identifier
    * `string` - picks any string
    * `operator` - picks any allowed operator

    All of them can be restricted to the single exact match:
    * `number: '123'` - picks only the number '123'
    * `identifier: 'qwe'` - picks only the identifier 'qwe'
    * `string: 'asd'` - picks only the string 'asd'
    * `operator: '+'` - picks only the operator '+'
  * possible AST list node types:
    * `tlist`, `list`, `alist`, `slist`:
      ```
      tlist: a, b, c
      list: a, b, c
      alist: a, b, c
      slist: a, b, c
      ```
      corresponds in LOP-file to:
      ```
      : <a>, <b>, <c>
      (<a>, <b>, <c>)
      [<a>, <b>, <c>]
      { <a>, <b>, <c> }
      ```
    * `tree`, `call`, `aref`, `struct`, `fstring`:
      ```
      tree: a, b, c
      call: a, b, c
      aref: a, b, c
      struct: a, b, c
      fstring: a, b
      ```
      corresponds in LOP-file to:
      ```
      <a>: <b>, <c>
      <a>(<b>, <c>)
      <a>[<b>, <c>]
      <a> { <b>, <c> }
      <a>"<b>"
      ```
    * `binary`, `unary`:
      `binary: operator, a, b`
      corresponds to in LOP-file to:
      `<a> + <b>`
      or
      `<a> * <b>`
      etc...

    NOTE Inside every such list there is an implicit `seqof`.
  * reference to other rule `$<ref>`:
    ```
    rule1:
      $rule2
    rule2:
      identifier
    ```
  * everything, except the rule, can have property:
    * `#optional` - it's ok to fail. Meaningful in seqofs only
    * `#last` - if it's picked, then this must be the last AST node in the list (next is NULL), otherwise it fails
  * everything can have a callback:
    * `@callback` - call this 'callback' during the callback phase

# Motivation

My personal need was to make a superset for the Verilog HDL,
in which I could easily extend everything to the degree I needed,
in which it is easy to write and read a complex structured HDL code.

Lisp/Scheme gives you the power of extending everything to the infinity,
but, for me, they do not have a pleasant syntax to work with (read and write).

I found that syntaxes like YAML/Ruby/Python/Nim are better for structured texts,
to write and to read. I tried Ruby and Nim (not Python) to make a Verilog HDL superset.
But, it comes to me, using a programming language for descriptions is just wrong.
It is easy to overwhelm the description with the hacks and tricks in the programming language
breaking the pureness of the description.

I started to understand, that this superset must not be a programming language
in the first place. Verilog, VHDL, Litex and other Python frameworks will fail
the same way for me.

I started experimenting and, in the end, I came to this:
```
module: TestModule(WIDTH = 4)
	port:
		clk: in
		result: out[WIDTH]
		axi: AXI.Master
	for: i
		result[i] = axi.ar.id[i]
```

So, this project is just a formalization of my expirements on how descriptions should look,
how they can be verified and how they can be parsed.
