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
        return str(self._stmt) + ';\n'

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

class Context:
    def __init__(self):
        self._module = []
        self._stack = []

    def module_create(self, ast, delta):
        if delta > 0:
            self._stack.append(Module())
        else:
            self._module.append(self._stack.pop())

    def module_set_name(self, ast, delta):
        self._stack[-1].set_name(LOP.LOP_symbol_value(ast).decode())

    def module_add_port(self, ast, delta):
        if delta > 0:
            self._stack.append(Port())
        else:
            i = self._stack.pop()
            self._stack[-1].add_port(i)

    def module_add_stmt(self, ast, delta):
        if delta < 0:
            i = Statement()
            i.set_stmt(self._stack.pop())
            self._stack[-1].add_stmt(i)

    def port_set_name(self, ast, delta):
        self._stack[-1].set_name(LOP.LOP_symbol_value(ast).decode())

    def port_set_dir(self, ast, delta):
        if delta > 0:
            self._stack[-1].set_dir(LOP.LOP_symbol_value(ast).decode())

    def port_set_width(self, ast, delta):
        self._stack[-1].set_width(int(LOP.LOP_symbol_value(ast).decode()))

    def assign_create(self, ast, delta):
        if delta > 0:
            self._stack.append(Assign())

    def assign_set_lhs(self, ast, delta):
        if delta < 0:
            i = self._stack.pop()
            self._stack[-1].set_lhs(i)

    def assign_set_rhs(self, ast, delta):
        if delta < 0:
            i = self._stack.pop()
            self._stack[-1].set_rhs(i)

    def bus_create(self, ast, delta):
        if delta > 0:
            self._stack.append(Bus())

    def bus_add(self, ast, delta):
        if delta < 0:
            i = self._stack.pop()
            self._stack[-1].add(i)

    def identifier(self, ast, delta):
        self._stack.append(LOP.LOP_symbol_value(ast).decode())

    def binary(self, ast, delta):
        if delta > 0:
            self._stack.append(Binary())

    def binary_set_op(self, ast, delta):
        self._stack[-1].set_op(LOP.LOP_symbol_value(ast).decode())

    def binary_set_op1(self, ast, delta):
        if delta < 0:
            i = self._stack.pop()
            self._stack[-1].set_op1(i)

    def binary_set_op2(self, ast, delta):
        if delta < 0:
            i = self._stack.pop()
            self._stack[-1].set_op2(i)

    def __str__(self):
        ret = ''
        for i in self._module:
            ret += str(i)
        return ret

schema = LOP.LOP_Schema()
schema.filename = LOP.String('schema.lop'.encode())

schema_str = LOP.map_file('schema.lop')
rc = LOP.LOP_schema_init(schema, schema_str.data, schema_str.len)
LOP.unmap_file(schema_str)

if rc == 0:
    lop = LOP.LOP()
    lop.schema = ctypes.pointer(schema)
    lop.top_rule_name = LOP.String('top'.encode())
    lop.filename = LOP.String('example.lop'.encode())

    src = LOP.map_file('example.lop')
    LOP.LOP_init(lop, src.data, src.len)
    LOP.unmap_file(src)

    ctx = Context()

    for i in range(lop.hl.count):
        func = getattr(ctx, str(lop.hl.handler[i].key))
        func(lop.hl.handler[i].n, lop.hl.handler[i].delta)

    print(ctx)

    LOP.LOP_deinit(lop)

LOP.LOP_schema_deinit(schema)
