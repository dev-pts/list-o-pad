import sys
import LOP
import ctypes

class Module:
    def __init__(self):
        self._name = None
        self._port = []
        self._stmt = []

    def set_name(self, name):
        self._name = name

    def add_port(self, port):
        self._port.append(port)

    def add_stmt(self, stmt):
        self._stmt.append(stmt)

    def __str__(self):
        ret = f'module {self._name}('
        ret += ', '.join([i._name for i in self._port])
        ret += ');\n'
        for i in self._port:
            ret += i._dir + 'put wire '
            if i._width > 1:
                ret += f'[{i._width - 1}:0] '
            ret += i._name + ';\n'
        for i in self._stmt:
            ret += str(i)
        ret += 'endmodule\n'
        return ret

class Port:
    def __init__(self):
        self._name = None
        self._dir = None
        self._width = 1

    def set_name(self, name):
        self._name = name

    def set_dir(self, dir):
        self._dir = dir

    def set_width(self, width):
        self._width = width

class Statement:
    def __init__(self):
        self._stmt = None

    def set_stmt(self, i):
        self._stmt = i

    def __str__(self):
        return 'assign ' + str(self._stmt) + ';\n'

class Assign:
    def __init__(self):
        self._lhs = None
        self._rhs = None

    def set_lhs(self, lhs):
        self._lhs = lhs

    def set_rhs(self, rhs):
        self._rhs = rhs

    def __str__(self):
        return str(self._lhs) + ' = ' + str(self._rhs)

class Bus:
    def __init__(self):
        self._item = []

    def add(self, item):
        self._item.append(item)

    def __str__(self):
        return '{ ' + ', '.join([str(i) for i in self._item]) + ' }'

class Binary:
    def __init__(self):
        self._op = None
        self._op1 = None
        self._op2 = None

    def set_op(self, i):
        self._op = i

    def set_op1(self, i):
        self._op1 = i

    def set_op2(self, i):
        self._op2 = i

    def __str__(self):
        return str(self._op1) + ' ' + str(self._op) + ' ' + str(self._op2)

SYNTAX = []
PARSER = []

def parser(syntax):
    def decorator(cls):
        SYNTAX.append(syntax)
        PARSER.append(cls)
        return cls
    return decorator

@parser("""
module:
	tree: @module_create
		identifier: 'module'
		identifier: @module_set_name
		tree:
			identifier: 'port'
			listof:
				$port: @module_add_port
		listof:
			$statement: @module_add_stmt
""")
class ParseModule:
    def module_create(stack, ast, delta):
        if delta > 0:
            stack.append(Module())

    def module_set_name(stack, ast, delta):
        stack[-1].set_name(LOP.LOP_symbol_value(ast).decode())

    def module_add_port(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].add_port(i)

    def module_add_stmt(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].add_stmt(i)

@parser("""
port:
	tree: @port_create
		identifier: @port_set_name
		oneof:
			$direction: @port_set_dir
			aref:
				$direction: @port_set_dir
				number: @port_set_width

direction:
	oneof:
		identifier: 'in'
		identifier: 'out'
""")
class ParsePort:
    def port_create(stack, ast, delta):
        if delta > 0:
            stack.append(Port())

    def port_set_name(stack, ast, delta):
        stack[-1].set_name(LOP.LOP_symbol_value(ast).decode())

    def port_set_dir(stack, ast, delta):
        if delta > 0:
            stack[-1].set_dir(LOP.LOP_symbol_value(ast).decode())

    def port_set_width(stack, ast, delta):
        stack[-1].set_width(int(LOP.LOP_symbol_value(ast).decode()))

@parser("""
statement:
	oneof: @stmt_create
		$stmt_assign: @stmt_set
""")
class ParseStatement:
    def stmt_create(stack, ast, delta):
        if delta > 0:
            stack.append(Statement())

    def stmt_set(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].set_stmt(i)

@parser("""
stmt_assign:
	binary: @assign_create
		operator: '='
		$lhs: @assign_set_lhs
		$expr: @assign_set_rhs

lhs:
	oneof:
		identifier: @identifier
		$bus
""")
class ParseAssign:
    def assign_create(stack, ast, delta):
        if delta > 0:
            stack.append(Assign())

    def assign_set_lhs(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].set_lhs(i)

    def assign_set_rhs(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].set_rhs(i)

@parser("""
bus:
	slist: @bus_create
		listof:
			$expr: @bus_add
""")
class ParseBus:
    def bus_create(stack, ast, delta):
        if delta > 0:
            stack.append(Bus())

    def bus_add(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].add(i)

@parser("""
expr:
	oneof:
		identifier: @identifier
		number: @number
		unary: @unary
			operator: @operator
			$expr
		binary: @binary
			operator: @binary_set_op
			$expr: @binary_set_op1
			$expr: @binary_set_op2
		$bus
		list:
			$expr
""")
class ParserExpr:
    def identifier(stack, ast, delta):
        stack.append(LOP.LOP_symbol_value(ast).decode())

    def binary(stack, ast, delta):
        if delta > 0:
            stack.append(Binary())

    def binary_set_op(stack, ast, delta):
        stack[-1].set_op(LOP.LOP_symbol_value(ast).decode())

    def binary_set_op1(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].set_op1(i)

    def binary_set_op2(stack, ast, delta):
        if delta < 0:
            i = stack.pop()
            stack[-1].set_op2(i)

@parser("""
top:
	tlist:
		listof:
			$module
""")
class Top:
    pass

schema_str = """
: #operators
	{
	}

	unary:
		'++', '--'
		'!', '~'
		'+', '-'
		'*', '/'
		'&', '|', '^'

	binary_left_to_right: '*', '/', '%'
	binary_left_to_right: '+', '-'
	binary_left_to_right: '<<', '>>'
	binary_left_to_right: '<', '>', '<=', '>='
	binary_left_to_right: '==', '!='
	binary_left_to_right: '&'
	binary_left_to_right: '^'
	binary_left_to_right: '|'
	binary_left_to_right: '&&'
	binary_left_to_right: '||'

	binary_right_to_left:
		'='
		'+=', '-='
		'*=', '/=', '%='
		'>>=', '<<='
		'~=', '&=', '|=', '^='
"""

schema_str += ''.join(SYNTAX)

schema = LOP.LOP_Schema()
schema.filename = LOP.String('schema.lop'.encode())

rc = LOP.LOP_schema_init(schema, schema_str, len(schema_str))

if rc == 0:
    lop = LOP.LOP()
    lop.schema = ctypes.pointer(schema)
    lop.top_rule_name = LOP.String('top'.encode())
    lop.filename = LOP.String('example.lop'.encode())

    src = LOP.map_file('example.lop')
    LOP.LOP_init(lop, src.data, src.len)
    LOP.unmap_file(src)

    stack = []

    for i in range(lop.hl.count):
        for j in PARSER:
            if hasattr(j, str(lop.hl.handler[i].key)):
                func = getattr(j, str(lop.hl.handler[i].key))
                func(stack, lop.hl.handler[i].n, lop.hl.handler[i].delta)
                break

    for i in stack:
        print(i)

    LOP.LOP_deinit(lop)

LOP.LOP_schema_deinit(schema)
