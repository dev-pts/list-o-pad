import sys
import LOP
import ctypes
import re

class Error(Exception):
	def __init__(self, message, ast):
		super().__init__(message)
		self.ast = ast

class OutOfBounds(Exception):
	def __init__(self, message, ast):
		super().__init__(message)
		self.ast = ast

def wrap(f):
	def wrap(self, *args, **kwargs):
		try:
			return f(self, *args, **kwargs)
		except OutOfBounds as e:
			raise e
		except Error as e:
			raise e
		except Exception as e:
			raise Error(f'{f.__name__}: {str(e)}', self.ast.contents)
	return wrap

def for_all_methods(decorator):
	def decorate(cls):
		for i in cls.__dict__:
			func = getattr(cls, i)
			if callable(func):
				setattr(cls, i, decorator(func))
		return cls
	return decorate

class Formatter:
	page = {}

	def __init__(self):
		self._cmd = []

	def tab(self):
		self._cmd.append(+1)

	def untab(self):
		self._cmd.append(-1)

	def __iadd__(self, other):
		if type(other) == Formatter:
			self._cmd += other._cmd
		else:
			self._cmd.append(other)
		return self

	def __add__(self, other):
		if type(other) == Formatter:
			self._cmd += other._cmd
		else:
			self._cmd.append(other)
		return self

	def indent(self, s):
		return '\t' * self._level + s

	def __str__(self):
		level = 0
		ret = ''
		newline = 1

		for i in self._cmd:
			if type(i) == int:
				level += i
				continue

			while True:
				idx = i.find('\n')

				if idx == 0:
					newline = 1
				elif idx < 0:
					if len(i):
						if newline:
							ret += '\t' * level
							newline = 0
						ret += i
					break
				elif idx > 0:
					if newline:
						ret += '\t' * level
					ret += i[:idx]
					newline = 1

				ret += '\n'
				i = i[idx + 1:]

		return ret

def fatal(e, src):
	loc = e.ast.loc
	s = ctypes.c_char_p(src.data).value.decode()
	s = s[loc.line_offset:]
	s = s[:s.find('\n')]
	print(f'Parsing ERROR at {loc.lineno}:{loc.charno}:')
	print(s)
	raise e

class Scope:
	def __init__(self):
		self.scope = {}

	def lookup(self, name):
		if name in self.scope:
			return self.scope[name]
		raise Exception(name)

	def lookup_try(self, name, default=None):
		if name in self.scope:
			return self.scope[name]
		return default

	def add(self, value):
		name = value.name
		if name in self.scope:
			raise Error(f'Variable "{name}" already defined at', self.scope[name].ast.contents)
		self.scope[name] = value

	def remove(self, name):
		if name in self.scope:
			del scope[name]
			return
		raise Exception(name)

class GlobalScope:
	def __init__(self):
		self.stack = []

	def push(self, scope):
		self.stack.append(scope)

	def pop(self):
		self.stack.pop()

	def lookup(self, name):
		for scope in reversed(self.stack):
			symbol = scope.lookup_try(name)
			if symbol != None:
				return symbol
		raise Exception(name)

	def lookup_try(self, name, default=None):
		for scope in reversed(self.stack):
			symbol = scope.lookup_try(name)
			if symbol != None:
				return symbol
		return default

	def add(self, value):
		self.stack[-1].add(value)

SCOPE = GlobalScope()

@for_all_methods(wrap)
class External:
	def __init__(self):
		self.scope = Scope()

		# Convenient filtered symbols
		self.param = []
		self.port = []

	def add_param(self, arg):
		self.param.append(arg)
		self.scope.add(arg)

	def add_port(self, arg):
		self.port.append(arg)
		self.scope.add(arg)

	def compile(self, param=Scope()):
		ret = External()

		for i in self.param:
			ret.add_param(i.clone(param.lookup_try(i.name, i).value))
		for i in self.port:
			ret.add_port(i.clone())

		SCOPE.push(ret.scope)

		for i in ret.param:
			i.compile_value()
		for i in ret.port:
			i.compile_value()

		SCOPE.pop()
		return ret

	def hier(self, src, field):
		return None

@for_all_methods(wrap)
class InterfacePortDesc:
	def __init__(self, ast):
		self.ast = ast
		self._class = []
		self.width = None
		self.interface = None

	def add_class(self, arg):
		if arg in self._class:
			raise Exception()
		self._class.append(arg)

	def set_width(self, arg):
		self.width = arg

	def set_interface(self, arg):
		self.interface = arg

	def compile(self, idx):
		if self.interface:
			ret = InterfaceInstance(self.ast)
			ret.set_namespace(self.interface)
			ret.set_field(self._class[idx])
			return ret.compile()

		ret2 = Port(self.ast)
		ret2.set_dir(self._class[idx] + 'put')

		ret = Array(self.ast)
		ret.set_value(ret2)
		if self.width:
			ret.set_width(self.width)
		return ret.compile()

@for_all_methods(wrap)
class Interface:
	def __init__(self, ast):
		self.ast = ast
		self.scope = Scope()
		self._class = []

		# Convenient filtered symbols
		self.param = []
		self.port = []

	def add_param(self, arg):
		self.param.append(arg)
		self.scope.add(arg)

	def add_port(self, arg):
		self.port.append(arg)
		self.scope.add(arg)

	def add_class(self, arg):
		if arg in self._class:
			raise Exception()
		self._class.append(arg)

	def compile(self, _class, param=Scope()):
		if _class not in self._class:
			raise Exception()

		ret = Interface(self.ast)

		ret._class = self._class

		for i in self.param:
			ret.add_param(i.clone(param.lookup_try(i.name, i).value))
		for i in self.port:
			ret.add_port(i.clone())

		SCOPE.push(ret.scope)

		for i in ret.param:
			i.compile_value()

		idx = self._class.index(_class)
		for i in ret.port:
			i.compile_value(idx)

		SCOPE.pop()
		return ret

	def hier(self, src, field):
		return None

	def get_ports(self, filter_dir, src):
		ret = []
		for i in self.port:
			ret.extend(i.resolve().get_ports(filter_dir, Hier(src.ast).set_namespace(src).set_field(Identifier(src.ast, i.name))))
		return ret

	def to_verilog(self, name):
		def _hier_name(field):
			return f'{name}__{field}'

		ret = Formatter()

		if self.port:
			ret += ',\n'.join([i.value.to_verilog(_hier_name(i.name)) for i in self.port])

		return str(ret)

@for_all_methods(wrap)
class InterfaceInstance:
	def __init__(self, ast):
		self.ast = ast
		self.namespace = None
		self.field = None
		self.inst = None

	def set_namespace(self, arg):
		self.namespace = arg

	def set_field(self, arg):
		self.field = arg

	def compile(self):
		ret = InterfaceInstance(self.ast)
		ret.set_namespace(self.namespace)
		ret.inst = SCOPE.lookup(self.namespace).value.compile(self.field)
		return ret

	def dim(self):
		return (None, None)

	def resolve(self):
		return self

	def resolve_hier(self, field):
		return self.inst.scope.lookup(field.name).value

	def connect(self, src_a, src_b):
		return self.inst.connect(src_a, src_b)

	def is_instance(self):
		return False

	def get_ports(self, filter_dir, src):
		ret = []
		for i in self.inst.port:
			ret.extend(i.resolve().get_ports(filter_dir, Hier(src.ast).set_namespace(src).set_field(Identifier(src.ast, i.name))))
		return ret

	def to_verilog_hier(self, namespace, field):
		return f'{namespace.to_verilog()}__{field.name}'

	def to_verilog_slice(self, name, dim, shape):
		ret = name
		if dim[0]:
			ret += f'_{dim[0].to_verilog()}'
		return ret

	def to_verilog(self, name, shape=(None, None)):
		count, _ = shape

		if count:
			if count.to_int() == 0:
				raise Exception()
			if count.to_int() > 1:
				for i in range(count.to_int()):
					return ',\n'.join(self.inst.to_verilog(f'{name}_{i}') for i in range(count.to_int()))

		return self.inst.to_verilog(name)

	def to_verilog_inst_port(self, name, shape=(None, None)):
		ret = Formatter()

		for i in self.inst.port:
			ret += i.to_verilog_inst_port(name) + '\n'

		return ret

	def to_verilog_bind(self, name):
		binds = {}

		for i in self.inst.port:
			binds.update(i.to_verilog_bind(name))

		return binds

@for_all_methods(wrap)
class Module:
	def __init__(self):
		self.scope = Scope()

		# Convenient filtered symbols
		self.param = []
		self.const = []
		self.port = []
		self.local = []

		self.comb = []
		self.sync = []

	def add_param(self, arg):
		self.param.append(arg)
		self.scope.add(arg)

	def add_const(self, arg):
		self.const.append(arg)
		self.scope.add(arg)

	def add_port(self, arg):
		self.port.append(arg)
		self.scope.add(arg)

	def add_local(self, arg):
		self.local.append(arg)
		self.scope.add(arg)

	def add_comb(self, arg):
		self.comb.append(arg)

	def add_sync(self, arg):
		self.sync.append(arg)

	def compile(self, param=Scope()):
		ret = Module()

		for i in self.param:
			ret.add_param(i.clone(param.lookup_try(i.name, i).value))
		for i in self.const:
			ret.add_const(i.clone())
		for i in self.port:
			ret.add_port(i.clone())
		for i in self.local:
			ret.add_local(i.clone())

		SCOPE.push(ret.scope)

		for i in ret.param:
			i.compile_value()
		for i in ret.const:
			i.compile_value()
		for i in ret.port:
			i.compile_value()
		for i in ret.local:
			i.compile_value()

		for i in self.comb:
			ret.add_comb(i.compile())
		for i in self.sync:
			ret.add_sync(i.compile())

		SCOPE.pop()
		return ret

	def hier(self, src, field):
		return None

	def to_verilog(self, name):
		ret = Formatter()

		ret += f'module {name}('
		if self.port:
			ret += '\n'
			ret.tab()
			ret += ',\n'.join([i.value.to_verilog(i.name) for i in self.port])
			ret.untab()
			ret += '\n'
		ret += ');\n'
		ret.tab()

		for i in self.param:
			ret += f'localparam {i.name} = {i.value.to_verilog()};\n'
		for i in self.const:
			ret += f'localparam {i.name} = {i.value.to_verilog()};\n'

		for i in self.local:
			ret += i.value.to_verilog(i.name)

		sens = {}
		for i in self.comb:
			sens.update(i.get_sens())

		if sens:
			ret += 'always @('
			ret += ', '.join(sens)
			ret += ') begin\n'
			ret.tab()

			for i in self.comb:
				ret += i.to_verilog()

			ret.untab()
			ret += 'end\n'

		for i in self.sync:
			ret += i.to_verilog()

		ret.untab()

		ret += 'endmodule\n'
		return ret

@for_all_methods(wrap)
class Port:
	def __init__(self, ast):
		self.ast = ast
		self.dir = None
		self.binding = None

	def set_dir(self, dir):
		self.dir = dir

	def set_binding(self, arg):
		self.binding = arg
		arg.resolve().binded = True

	def compile(self):
		ret = Port(self.ast)
		ret.set_dir(self.dir)
		return ret

	def dim(self):
		return ()

	def resolve(self):
		return self

	def operator(self, op, op2):
		return None

	def slice(self, hi, lo):
		return None

	def get_ports(self, filter_dir, src):
		if self.dir == filter_dir:
			return [src]
		return []

	def _to_verilog_one(self, prefix, name, width, idx):
		ret = prefix
		if width and width.to_int() > 1:
			ret += f'[{width.to_int() - 1}:0] '
		ret += name
		if idx != None:
			ret += f'_{idx}'
		return ret

	def to_verilog(self, name, shape=(None, None)):
		count, width = shape

		prefix = self.dir
		if self.dir == 'output':
			prefix += ' reg '
		else:
			prefix += ' wire '

		if count and count.to_int() > 1:
			ret = ', '.join([self._to_verilog_one(prefix, name, width, i) for i in range(count.to_int())])
		else:
			return self._to_verilog_one(prefix, name, width, None)

		return ret

	def to_verilog_slice(self, name, dim, shape):
		count, width = shape

		if count == None:
			wi = dim[0]
			ci = None
		else:
			ci = dim[0]
			wi = dim[1]

		ret = name
		if ci and count and count.to_int() > 1:
			ret += f'_{ci.to_verilog()}'
		if wi and width and width.to_int() > 1:
			ret += f'[{wi.to_verilog()}]'
		return ret

	def to_verilog_inst_port(self, name, shape):
		if self.binding:
			return ''

		_, width = shape

		if self.dir == 'input':
			prefix = 'reg '
		else:
			prefix = 'wire '

		return self._to_verilog_one(prefix, name, width, None) + ';'

	def to_verilog_bind(self, name):
		binds = {}

		if self.binding:
			binds[name] = self.binding.to_verilog()
		else:
			binds[name] = name

		return binds

@for_all_methods(wrap)
class Net:
	def __init__(self, ast):
		self.ast = ast
		self.value = None
		self.binded = False

	def set_value(self, arg):
		self.value = arg

	def compile(self):
		ret = Net(self.ast)
		if self.value:
			ret.set_value(self.value.compile())
		return ret

	def dim(self):
		return ()

	def slice(self, hi, lo):
		return None

	def operator(self, op, op2):
		return None

	def to_verilog(self, name, shape=(None, None)):
		count, width = shape

		if self.value != None or self.binded:
			ret = 'wire '
		else:
			ret = 'reg '
		if width and width.to_int() > 1:
			ret += f'[{width.to_int() - 1}:0] '
		ret += name
		if count and count.to_int() > 1:
			ret += f' [{count.to_int() - 1}:0]'
		if self.value != None:
			ret += f' = {self.value.to_verilog()}'
		ret += ';\n'
		return ret

	def to_verilog_slice(self, name, dim=(None, None), count=(None, None)):
		ret = name
		for i in dim:
			ret += f'[{i.to_verilog()}]'
		return ret

@for_all_methods(wrap)
class FSM:
	def __init__(self, ast):
		self.ast = ast
		self.state = {}
		self.onehot = True

	def add_state(self, arg):
		self.state[arg] = len(self.state)

	def compile(self):
		return self

	def dim(self):
		return (Number(self.ast, len(self.state)),)

	def goto(self, src):
		ret = Statement(src.ast)
		ass = Assign(src.ast)
		ass.set_lhs(src.namespace)
		ass.set_rhs(Number(src.ast, 1 << self.state[src.field.name]))
		ret.set_comb(ass)
		return ret.compile()

	def resolve_hier(self, field):
		return self

	def operator(self, op, op2):
		return None

	def to_verilog(self, name, dim=(None, None)):
		ret = 'reg '
		ret += f'[{len(self.state) - 1}:0] '
		ret += name
		if dim[0] and dim[0].to_int() > 1:
			ret += f' [{dim[0].to_int() - 1}:0]'
		ret += ';\n'
		return ret

	def to_verilog_hier(self, namespace, field):
		return f'{namespace.to_verilog()}[{self.state[field.name]}]'

	def to_verilog_slice(self, name, dim=(None, None), count=(None, None)):
		return f'{name}[{dim[0].to_verilog()}]'

class Array:
	def __init__(self, ast):
		self.ast = ast
		self.shape = (None, None)
		self.value = None

	def set_count(self, arg):
		self.shape = (arg, self.shape[1])

	def set_width(self, arg):
		self.shape = (self.shape[0], arg)

	def set_value(self, arg):
		self.value = arg

	def compile(self):
		ret = Array(self.ast)
		if self.shape[0]:
			ret.set_count(self.shape[0].compile())
		if self.shape[1]:
			ret.set_width(self.shape[1].compile())
		ret.set_value(self.value.compile())
		return ret

	def operator(self, op, op2):
		return None

	def resolve(self):
		return self.value.resolve()

	def resolve_hier(self, field):
		return self.value.resolve_hier(field)

	def slice(self, hi, lo):
		return None
		if hi != lo:
			raise Exception()
		if hi == lo:
			return self.value
		raise Exception()

	def dim(self):
		return self.shape

	def is_instance(self):
		return self.value.is_instance()

	def to_verilog(self, name):
		return self.value.to_verilog(name, self.shape)

	def to_verilog_slice(self, name, dim):
		return self.value.to_verilog_slice(name, dim, self.shape)

	def to_verilog_hier(self, namespace, field):
		return self.value.to_verilog_hier(namespace, field)

	def to_verilog_inst_port(self, name):
		return self.value.to_verilog_inst_port(name, self.shape)

	def to_verilog_bind(self, name):
		return self.value.to_verilog_bind(name)

@for_all_methods(wrap)
class Symbol:
	def __init__(self, ast):
		self.ast = ast
		self.name = None
		self.value = None
		self.compiled = False

	def set_name(self, name):
		self.name = name

	def set_value(self, arg):
		self.value = arg

	def resolve(self):
		return self.value.resolve()

	def dim(self):
		return self.value.dim()

	def clone(self, value=None):
		ret = Symbol(self.ast)
		ret.set_name(self.name)
		if value == None:
			value = self.value
		ret.set_value(value)
		return ret

	def compile_value(self, *args, **kwargs):
		if not self.compiled:
			self.value = self.value.compile(*args, **kwargs)
			self.compiled = True
		return self.value

	def to_verilog_inst_port(self, name):
		count, _ = self.dim()

		ret = Formatter()

		if count and count.to_int() > 1:
			for k in range(count.to_int()):
				pname = self.to_verilog_slice((Number(None, k), None))

				ret += self.value.to_verilog_inst_port(f'{name}__{pname}')
		else:
			ret += self.value.to_verilog_inst_port(f'{name}__{self.name}')

		return ret

	def to_verilog_slice(self, dim):
		return self.value.to_verilog_slice(self.name, dim)

	def to_verilog_bind(self, name):
		binds = {}

		count, _ = self.dim()

		if count and count.to_int() > 1:
			for k in range(count.to_int()):
				pname = self.to_verilog_slice((Number(None, k), None))

				binds.update(self.value.to_verilog_bind(f'{name}__{pname}'))
		else:
			binds.update(self.value.to_verilog_bind(f'{name}__{self.name}'))

		return binds

system = {}

class Z:
	def __init__(self, ast):
		self.ast = ast

	def to_verilog(self):
		return "1'bz"

	def get_sens(self):
		return {}

def sh_z(ast, args):
	if len(args) != 0:
		raise Exception()
	return Z(ast)

def sh_goto(ast, args):
	if len(args) != 1:
		raise Exception()

	hier = args[0]
	fsm = hier.resolve()

	return fsm.goto(hier)

def sh_bind(ast, args):
	hier = args[0]
	port = hier.resolve().resolve()
	port.set_binding(args[1])
	return Empty()

def sh_connect(ast, args):
	src_a = args[0]
	src_b = args[1]

	inst_a = src_a.is_instance()
	inst_b = src_b.is_instance()

	if inst_a:
		a_lhs = 'input'
		a_rhs = 'output'
	else:
		a_lhs = 'output'
		a_rhs = 'input'

	if inst_b:
		b_lhs = 'input'
		b_rhs = 'output'
	else:
		b_lhs = 'output'
		b_rhs = 'input'

	if_a = src_a.resolve().resolve()
	if_b = src_b.resolve().resolve()

	ret = Block(ast)

	ports_a = if_a.get_ports(a_lhs, src_a)
	ports_b = if_b.get_ports(b_rhs, src_b)

	for i in range(len(ports_a)):
		ret.add(
			Statement(ast).set_comb(
				Assign(ast)
					.set_lhs(ports_a[i])
					.set_rhs(ports_b[i])
			)
		)

	ports_a = if_a.get_ports(a_rhs, src_a)
	ports_b = if_b.get_ports(b_lhs, src_b)

	for i in range(len(ports_b)):
		ret.add(
			Statement(ast).set_comb(
				Assign(ast)
					.set_lhs(ports_b[i])
					.set_rhs(ports_a[i])
			)
		)

	return ret.compile()

system['z'] = sh_z
system['goto'] = sh_goto
system['bind'] = sh_bind
system['connect'] = sh_connect

class Empty:
	def get_sens(self):
		return {}

	def to_verilog(self):
		return ''

@for_all_methods(wrap)
class System:
	def __init__(self, ast):
		self.ast = ast
		self.func = None
		self.args = []

	def set_func(self, arg):
		self.func = arg

	def add_arg(self, arg):
		self.args.append(arg)

	def compile(self):
		args = []
		for i in self.args:
			args.append(i.compile())
		return self.func(self.ast, args)

@for_all_methods(wrap)
class Identifier:
	def __init__(self, ast, name):
		self.ast = ast
		self.name = name
		self.ref = None

	def compile(self):
		ref = SCOPE.lookup(self.name).compile_value()
		if type(ref) in [Number, String]:
			return ref
		ret = Identifier(self.ast, self.name)
		ret.ref = ref
		return ret

	def dim(self):
		return self.ref.dim()

	def resolve(self):
		return self.ref

	def operator(self, op, op2):
		return self.ref.operator(op, op2)

	def slice(self, hi, lo):
		return self.ref.slice(hi, lo)

	def is_instance(self):
		return self.ref.is_instance()

	def to_verilog(self):
		return self.name

	def to_verilog_hier(self, namespace, field):
		return self.ref.to_verilog_hier(namespace, field)

	def to_verilog_slice(self, dim):
		return self.ref.to_verilog_slice(self.name, dim)

	def get_sens(self):
		return { self.name: None }

@for_all_methods(wrap)
class Number:
	def __init__(self, ast, value):
		self.ast = ast
		self.value = value
		self.width = None
		self.svalue = str(value)
		self.base = None
		self.ibase = None

		try:
			self.value = int(self.value)
			self.width = self.value.bit_length()
			return
		except:
			try:
				self.value = float(self.value)
				self.width = 1 << (sys.float_info.mant_dig).bit_length()
				return
			except:
				pass

		self.width, self.base, self.svalue = re.findall(r'([0-9][_0-9]*)(.)([0-9a-f_]+)', self.value, re.IGNORECASE)[0]

		self.width = int(self.width)

		if self.base == 'b':
			self.ibase = 2
		elif self.base == 'o':
			self.ibase = 8
		elif self.base == 'd':
			self.ibase = 10
		elif self.base == 'x':
			self.ibase = 16
		else:
			raise Exception(f'Unknown base "{self.base}"')

	def compile(self):
		return Number(self.ast, self.value)

	def resolve(self):
		return self

	def to_int(self):
		if type(self.value) in [int, float]:
			return self.value

		return int(self.svalue, self.ibase)

	def dim(self):
		return (Number(self.ast, self.width),)

	def slice(self, hi, lo):
		value = self.to_int()

		return Number(self.ast, (value >> lo) & ((1 << (hi - lo + 1)) - 1))

	def operator(self, op, op2):
		op1 = self.to_int()

		if op2 == None:
			if op == '-':
				return Number(self.ast, -op1)
			if op == '+':
				return Number(self.ast, op1)
			if op == '~':
				return Number(self.ast, ~op1)
			if op == '!':
				return Number(self.ast, 1 if not op1 else 0)
			raise Exception(f'Unknown "{op}"')

		if type(op2) != Number:
			return None

		op2 = op2.to_int()

		if op == '+':
			return Number(self.ast, op1 + op2)
		if op == '-':
			return Number(self.ast, op1 - op2)
		if op == '<':
			return Number(self.ast, op1 < op2)
		if op == '>':
			return Number(self.ast, op1 > op2)
		if op == '*':
			return Number(self.ast, op1 * op2)
		if op == '>>':
			return Number(self.ast, op1 >> op2)
		if op == '<<':
			return Number(self.ast, op1 << op2)
		if op == '/':
			return Number(self.ast, op1 / op2)
		if op == '%':
			return Number(self.ast, op1 % op2)
		if op == '==':
			return Number(self.ast, 1 if op1 == op2 else 0)
		if op == '!=':
			return Number(self.ast, 1 if op1 != op2 else 0)
		if op == '**':
			return Number(self.ast, op1 ** op2)
		if op == '|':
			return Number(self.ast, op1 | op2)
		if op == '&':
			return Number(self.ast, op1 & op2)
		if op == '&&':
			return Number(self.ast, 1 if op1 and op2 else 0)
		if op == '||':
			return Number(self.ast, 1 if op1 or op2 else 0)
		raise Exception(f'Unknown "{op}"')

	def to_verilog(self):
		if type(self.value) in [int, float]:
			return str(self.value)

		ret = ''
		if self.width != 0:
			ret += str(self.width)
		base = self.base
		if base == 'x':
			base = 'h'
		ret += "'" + base
		ret += self.svalue
		return ret

	def get_sens(self):
		return {}

@for_all_methods(wrap)
class String:
	def __init__(self, ast, value):
		self.ast = ast
		self.value = value

	def compile(self):
		return String(self.ast, self.value)

	def operator(self, op, op2):
		if type(op2) != String:
			return None

		op1 = self.value
		op2 = op2.value

		if op == '==':
			return Number(self.ast, 1 if op1 == op2 else 0)
		if op == '!=':
			return Number(self.ast, 1 if op1 != op2 else 0)
		raise Exception(f'Unknown "{op}"')

	def to_verilog(self):
		return f'"{self.value}"'

@for_all_methods(wrap)
class Statement:
	def __init__(self, ast):
		self.ast = ast
		self.comb = None

	def set_comb(self, i):
		self.comb = i
		return self

	def compile(self):
		ret = Statement(self.ast)
		ret.set_comb(self.comb.compile())
		return ret

	def to_verilog(self):
		return self.comb.to_verilog() + ';\n'

	def get_sens(self):
		return self.comb.get_sens()

@for_all_methods(wrap)
class Block:
	def __init__(self, ast):
		self.ast = ast
		self.body = []

	def add(self, arg):
		self.body.append(arg)

	def compile(self):
		ret = Block(self.ast)
		for i in self.body:
			ret.add(i.compile())
		return ret

	def to_verilog(self):
		ret = Formatter()
		for i in self.body:
			ret += i.to_verilog()
		return ret

	def get_sens(self):
		ret = {}
		for i in self.body:
			ret.update(i.get_sens())
		return ret

@for_all_methods(wrap)
class For:
	def __init__(self, ast, inline):
		self.ast = ast
		self.it = None
		self.body = []
		self.inline = inline

	def set_it(self, arg):
		self.it = arg

	def add(self, arg):
		self.body.append(arg)

	def compile(self):
		if self.inline:
			ret = Bus(self.ast)
		else:
			ret = Block(self.ast)
		count = None

		it = Symbol(self.ast)
		it.set_value(Number(self.ast, 0))

		if type(self.it) == Identifier:
			it.set_name(self.it.name)
		elif type(self.it) == Binary:
			it.set_name(self.it.op1.name)
			count = self.it.op2.to_int()
		else:
			raise Exception()

		while True:
			if count != None:
				if it.value.value == count:
					break

			local = Scope()
			local.add(it.clone())

			SCOPE.push(local)
			try:
				for i in self.body:
					ret.add(i.compile())
			except OutOfBounds:
				break
			finally:
				SCOPE.pop()

			it.value.value += 1

		return ret

@for_all_methods(wrap)
class SyncCond:
	def __init__(self, ast, pos):
		self.ast = ast
		self.cond = None
		self.pos = pos

	def set_cond(self, i):
		self.cond = i

	def compile(self):
		ret = SyncCond(self.ast, self.pos)
		ret.set_cond(self.cond.compile())
		return ret

	def to_verilog(self):
		if self.pos:
			ret = 'posedge '
		else:
			ret = 'negedge '
		return ret + self.cond.to_verilog()

@for_all_methods(wrap)
class Sync:
	def __init__(self, ast):
		self.ast = ast
		self.cond = []
		self.body = []

	def add_cond(self, i):
		self.cond.append(i)

	def add(self, i):
		self.body.append(i)

	def compile(self):
		ret = Sync(self.ast)
		for i in self.cond:
			ret.add_cond(i.compile())
		for i in self.body:
			ret.add(i.compile())
		return ret

	def to_verilog(self):
		ret = Formatter()
		ret += 'always @('
		ret += ' or '.join([i.to_verilog() for i in self.cond])
		ret += ') begin\n'
		ret.tab()

		for i in self.body:
			ret += i.to_verilog()

		ret.untab()
		ret += 'end\n'
		return ret

@for_all_methods(wrap)
class Assign:
	def __init__(self, ast):
		self.ast = ast
		self.lhs = None
		self.rhs = None

	def set_lhs(self, lhs):
		self.lhs = lhs
		return self

	def set_rhs(self, rhs):
		self.rhs = rhs
		return self

	def compile(self):
		ret = Assign(self.ast)
		ret.set_lhs(self.lhs.compile())
		ret.set_rhs(self.rhs.compile())
		return ret

	def to_verilog(self):
		return f'{self.lhs.to_verilog()} <= {self.rhs.to_verilog()}'

	def get_sens(self):
		# Put LHS into the sens-list so that this:
		#   always @() a <= 1;
		# would evaluate into this:
		#   always @(a) a <= 1;
		# Nobody complains about this so far.
		ret = {}
		ret.update(self.lhs.get_sens())
		ret.update(self.rhs.get_sens())
		return ret

@for_all_methods(wrap)
class Bus:
	def __init__(self, ast):
		self.ast = ast
		self.item = []

	def add(self, item):
		self.item.append(item)

	def compile(self):
		ret = Bus(self.ast)
		for i in self.item:
			ret.add(i.compile())
		return ret

	def operator(self, op, op2):
		return None

	def to_verilog(self):
		return '{ ' + ', '.join([i.to_verilog() for i in self.item]) + ' }'

	def get_sens(self):
		ret = {}
		for i in self.item:
			ret.update(i.get_sens())
		return ret

@for_all_methods(wrap)
class Replicate:
	def __init__(self, ast):
		self.ast = ast
		self.number = None
		self.item = []

	def set_number(self, arg):
		self.number = arg

	def add(self, item):
		self.item.append(item)

	def compile(self):
		ret = Replicate(self.ast)
		ret.set_number(self.number.compile())
		for i in self.item:
			ret.add(i.compile())
		return ret

	def operator(self, op, op2):
		return None

	def to_verilog(self):
		item = '{ ' + ', '.join([i.to_verilog() for i in self.item]) + ' }'
		return f'{{ {self.number.to_verilog()} {item} }}'

	def get_sens(self):
		ret = {}
		for i in self.item:
			ret.update(i.get_sens())
		return ret

@for_all_methods(wrap)
class Operator:
	def __init__(self, ast, value):
		self.ast = ast
		self.value = value

	def compile(self):
		return self

	def to_verilog(self):
		return self.value

@for_all_methods(wrap)
class Unary:
	def __init__(self, ast):
		self.ast = ast
		self.op = None
		self.op1 = None

	def set_op(self, i):
		self.op = i

	def set_op1(self, i):
		self.op1 = i

	def compile(self):
		op = self.op.to_verilog()
		op1 = self.op1.compile()

		ret = op1.operator(op, None)
		if ret:
			return ret

		ret = Unary(self.ast)
		ret.set_op(self.op.compile())
		ret.set_op1(op1)
		return ret

	def operator(self, op, op2):
		return None

	def to_verilog(self):
		ret = f'{self.op.to_verilog()}'
		if type(self.op1) in [Unary, Binary]:
			ret += '('
		ret += self.op1.to_verilog()
		if type(self.op1) in [Unary, Binary]:
			ret += ')'
		return ret

	def get_sens(self):
		return self.op1.get_sens()

@for_all_methods(wrap)
class Hier:
	def __init__(self, ast):
		self.ast = ast
		self.namespace = None
		self.field = None
		self.ref = None

	def set_namespace(self, arg):
		self.namespace = arg
		return self

	def set_field(self, arg):
		self.field = arg
		return self

	def compile(self):
		ret = Hier(self.ast)
		ret.set_namespace(self.namespace.compile())
		ret.set_field(self.field)
		ret.ref = ret.namespace.resolve().resolve_hier(self.field)
		return ret

	def resolve(self):
		return self.ref

	def operator(self, op, op2):
		return None

	def dim(self):
		return self.ref.dim()

	def slice(self, hi, lo):
		return self.ref.slice(hi, lo)

	def is_instance(self):
		return self.namespace.is_instance()

	def to_verilog(self):
		return self.namespace.resolve().to_verilog_hier(self.namespace, self.field)

	def to_verilog_slice(self, dim):
		return self.ref.to_verilog_slice(self.namespace.resolve().to_verilog_hier(self.namespace, self.field), dim)

	def get_sens(self):
		return { self.namespace.resolve().to_verilog_hier(self.namespace, self.field): None }

@for_all_methods(wrap)
class Binary:
	def __init__(self, ast):
		self.ast = ast
		self.op = None
		self.op1 = None
		self.op2 = None

	def set_op(self, i):
		self.op = i

	def set_op1(self, i):
		self.op1 = i

	def set_op2(self, i):
		self.op2 = i

	def compile(self):
		op = self.op.to_verilog()
		op1 = self.op1.compile()
		op2 = self.op2.compile()

		ret = op1.operator(op, op2)
		if ret:
			return ret

		ret = Binary(self.ast)
		ret.set_op(self.op.compile())
		ret.set_op1(op1)
		ret.set_op2(op2)
		return ret

	def operator(self, op, op2):
		return None

	def to_verilog(self):
		ret = ''
		if type(self.op1) in [Unary, Binary]:
			ret += '('
		ret += self.op1.to_verilog()
		if type(self.op1) in [Unary, Binary]:
			ret += ')'
		ret += f' {self.op.to_verilog()} '
		if type(self.op2) in [Unary, Binary]:
			ret += '('
		ret += self.op2.to_verilog()
		if type(self.op2) in [Unary, Binary]:
			ret += ')'
		return ret

	def get_sens(self):
		ret = {}
		ret.update(self.op1.get_sens())
		ret.update(self.op2.get_sens())
		return ret

@for_all_methods(wrap)
class Range:
	def __init__(self, ast):
		self.ast = ast
		self.hi = None
		self.lo = None

	def set_hi(self, i):
		self.hi = i

	def set_lo(self, i):
		self.lo = i

	def compile(self):
		ret = Range(self.ast)
		ret.set_hi(self.hi.compile())
		ret.set_lo(self.lo.compile())
		return ret

	def to_verilog(self):
		return f'{self.hi.to_verilog()}:{self.lo.to_verilog()}'

@for_all_methods(wrap)
class If:
	def __init__(self, ast, inline):
		self.ast = ast
		self.cond = None
		self.iftrue = None
		self.iffalse = None
		self.inline = inline

	def set_cond(self, i):
		self.cond = i

	def set_iftrue(self, arg):
		self.iftrue = arg

	def set_iffalse(self, arg):
		self.iffalse = arg

	def compile(self):
		cond = self.cond.compile()

		if type(cond) == Number:
			if cond.to_int() == 0:
				if self.iffalse:
					return self.iffalse.compile()
				return Empty()
			return self.iftrue.compile()

		ret = If(self.ast, self.inline)
		ret.cond = cond
		ret.set_iftrue(self.iftrue.compile())
		if self.iffalse:
			ret.set_iffalse(self.iffalse.compile())
		return ret

	def operator(self, op, op2):
		return None

	def to_verilog(self):
		if self.inline:
			return f'({self.cond.to_verilog()} ? ({self.iftrue.to_verilog()}) : ({self.iffalse.to_verilog()}))'

		ret = Formatter()
		ret += f'if ({self.cond.to_verilog()}) begin\n'
		ret.tab()
		ret += self.iftrue.to_verilog()
		ret.untab()
		ret += 'end'
		if self.iffalse:
			ret += ' else begin\n'
			ret.tab()
			ret += self.iffalse.to_verilog()
			ret.untab()
			ret += 'end'
		return ret + '\n'

	def get_sens(self):
		ret = {}
		ret.update(self.cond.get_sens())
		ret.update(self.iftrue.get_sens())
		if self.iffalse:
			ret.update(self.iffalse.get_sens())
		return ret

@for_all_methods(wrap)
class Slice:
	def __init__(self, ast):
		self.ast = ast
		self.value = None
		self.hilo = None

	def set_value(self, arg):
		self.value = arg

	def set_hilo(self, arg):
		self.hilo = arg

	def dim(self):
		return self.value.dim()[1:]

	def compile(self):
		hilo = self.hilo.compile()
		value = self.value.compile()

		hi = lo = None
		if type(hilo) == Range:
			if type(hilo.hi) == Number and type(hilo.lo) == Number:
				hi = hilo.hi.to_int()
				lo = hilo.lo.to_int()
		else:
			if type(hilo) == Number:
				hi = lo = hilo.to_int()

		if hi != None:
			ret = value.slice(hi, lo)
			if ret:
				return ret

			dim = value.dim()
			if not dim:
				raise OutOfBounds('', self.ast.contents)
			if dim[0]:
				dim = dim[0]
			else:
				dim = dim[1]
			dim = dim.to_int()

			if lo < 0:
				lo += dim
			if hi < 0:
				hi += dim

			if lo < 0 or hi < 0 or dim <= lo or dim <= hi or hi < lo:
				raise OutOfBounds('', self.ast.contents)

			if hi == lo:
				hilo.value = hi
			else:
				hilo.hi.value = hi
				hilo.lo.value = lo

		ret = Slice(self.ast)
		ret.set_value(value)
		ret.set_hilo(hilo)
		return ret

	def operator(self, op, op2):
		return self.resolve().operator(op, op2)

	def resolve(self):
		return self.value.resolve()

	def slice(self, hi, lo):
		return None

	def is_instance(self):
		return self.value.is_instance()

	def to_verilog(self):
		return self.value.to_verilog_slice((self.hilo, None))

	def to_verilog_slice(self, dim=(None, None)):
		return self.value.to_verilog_slice((self.hilo, dim[0]))

	def to_verilog_hier(self, namespace, field):
		return self.value.to_verilog_hier(namespace, field)

	def get_sens(self):
		return { self.value.to_verilog_slice((self.hilo, None)): None }

@for_all_methods(wrap)
class Instance:
	def __init__(self, ast):
		self.ast = ast
		self.name = None
		self.param = Scope()
		self.inst = None

	def set_name(self, arg):
		self.name = arg

	def add_param(self, arg):
		self.param.add(arg)

	def compile(self):
		ret = Instance(self.ast)
		ret.set_name(self.name)
		ret.param = self.param
		ret.inst = SCOPE.lookup(self.name).value.compile(param=self.param)
		return ret

	def resolve(self):
		return self

	def resolve_hier(self, field):
		return self.inst.scope.lookup(field.name).value

	def dim(self):
		return ()

	def is_instance(self):
		return True

	def _to_verilog_one(self, mname, name, param):
		m = self.inst

		ret = Formatter()

		for i in m.port:
			ret += i.to_verilog_inst_port(name) + '\n'

		binds = {}

		for i in m.port:
			binds.update(i.to_verilog_bind(name))

		ret += f'{mname} '
		if param:
			ret += '#(\n'
			ret.tab()
			ret += ',\n'.join([f'.{i}({param[i]})' for i in param])
			ret.untab()
			ret += '\n) '
		ret += f'{name}('
		if m.port:
			ret += '\n'
			ret.tab()
			ret += ',\n'.join([f'.{i[len(name) + 2:]}({binds[i]})' for i in binds])
			ret += '\n'
			ret.untab()
		ret += f');\n'

		return ret

	def to_verilog(self, name, dim=(None, None)):
		count, _ = dim

		m = self.inst

		ret = Formatter()

		mname = self.name
		if type(m) == Module:
			for i in self.param.scope:
				mname += f'_{i}_{m.scope.lookup(i).value.to_verilog()}'
			mname = mname.translate(str.maketrans(".-", "_n"))

		param = {}
		if type(m) == External:
			for i in self.param.scope:
				param[i] = m.scope.lookup(i).value.to_verilog()

		if count:
			for i in range(count.to_int()):
				ret += self._to_verilog_one(mname, f'{name}_{i}', param)
		else:
			ret += self._to_verilog_one(mname, name, param)

		if type(m) == Module:
			Formatter.page[mname] = m

		return ret

	def to_verilog_hier(self, namespace, field):
		return f'{namespace.to_verilog()}__{field.name}'

	def to_verilog_slice(self, name, dim=(None, None), count=(None, None)):
		ret = name
		if dim[0]:
			ret += f'_{dim[0].to_verilog()}'
		return ret

SYNTAX = []
PARSER = []

def parser(syntax):
	def decorator(cls):
		SYNTAX.append(syntax)
		PARSER.append(cls)
		return cls
	return decorator

@parser("""
#operators:
	{
		unary: '.', '$', '@'

		binary_left_to_right:
			'.', '->'
			'++', '--'
	}

	binary_left_to_right: '**'

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

	binary_right_to_left: '..', '+..', '-..'

top:
	tlist:
		listof:
			$module_def: @top_add
			$external_def: @top_add
			$interface_def: @top_add
""")
class Top:
	def top_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			SCOPE.add(i)

	def symbol_create(stack, ast, delta):
		if delta > 0:
			stack.push(Symbol(ast))

	def symbol_set_name(stack, ast, delta):
		stack.last.set_name(LOP.LOP_symbol_value(ast).decode())

	def symbol_set_value(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_value(i)

	def array_create(stack, ast, delta):
		if delta > 0:
			stack.push(Array(ast))

	def array_set_count(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_count(i)

	def array_set_width(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_width(i)

	def array_set_width2(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			t = stack.pop()
			stack.last.set_width(i)
			stack.push(t)

	def array_set_value(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_value(i)

@parser("""
external_def:
	tree: @symbol_create
		identifier: @symbol_set_name
		$external: @symbol_set_value

external:
	seqof:
		oneof: @external_create
			identifier: 'external'
			tree:
				identifier: 'external'
				listof:
					binary: @external_add_param, @symbol_create
						operator: '='
						identifier: @symbol_set_name
						$expr: @symbol_set_value
		tree: #optional
			identifier: 'port'
			listof: #optional
				$port: @external_add_port
""")
class ParseExternal:
	def external_create(stack, ast, delta):
		if delta > 0:
			stack.push(External())

	def external_add_param(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_param(i)

	def external_add_port(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_port(i)

@parser("""
interface_def:
	tree: @symbol_create
		identifier: @symbol_set_name
		$interface: @symbol_set_value

interface:
	seqof:
		oneof: @interface_create
			identifier: 'interface'
			tree:
				identifier: 'interface'
				listof:
					binary: @interface_add_param, @symbol_create
						operator: '='
						identifier: @symbol_set_name
						$expr: @symbol_set_value
		tree:
			identifier: 'port'
			$interface_ports
			listof: #optional
				$interface_port: @interface_add_port

interface_ports:
	oneof:
		identifier: @interface_add_class
		binary:
			operator: '/'
			$interface_ports
			identifier: @interface_add_class

interface_port:
	tree: @symbol_create
		identifier: @symbol_set_name
		tree: @symbol_set_value
			oneof: @interface_port_desc_create
				identifier: 'net'
				aref:
					identifier: 'net'
					$expr: @interface_port_desc_set_width
				identifier: @interface_port_desc_set_interface
			$interface_ports

interface_port_decl:
	oneof: @interface_instance_create
		$interface_hier

interface_hier:
	binary:
		operator: '.'
		identifier: @interface_instance_set_namespace
		identifier: @interface_instance_set_field
""")
class ParseInterface:
	def interface_create(stack, ast, delta):
		if delta > 0:
			stack.push(Interface(ast))

	def interface_add_param(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_param(i)

	def interface_add_port(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_port(i)

	def interface_add_class(stack, ast, delta):
		stack.last.add_class(LOP.LOP_symbol_value(ast).decode())

	def interface_port_desc_create(stack, ast, delta):
		if delta > 0:
			stack.push(InterfacePortDesc(ast))

	def interface_port_desc_set_width(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_width(i)

	def interface_port_desc_set_interface(stack, ast, delta):
		stack.last.set_interface(LOP.LOP_symbol_value(ast).decode())

	def interface_instance_create(stack, ast, delta):
		if delta > 0:
			stack.push(InterfaceInstance(ast))

	def interface_instance_set_namespace(stack, ast, delta):
		stack.last.set_namespace(LOP.LOP_symbol_value(ast).decode())

	def interface_instance_set_field(stack, ast, delta):
		stack.last.set_field(LOP.LOP_symbol_value(ast).decode())

@parser("""
module_def:
	tree: @symbol_create
		identifier: @symbol_set_name
		$module: @symbol_set_value

module:
	seqof:
		oneof: @module_create
			identifier: 'module'
			tree:
				identifier: 'module'
				listof:
					binary: @module_add_param, @symbol_create
						operator: '='
						identifier: @symbol_set_name
						$expr: @symbol_set_value
		tree: #optional
			identifier: 'const'
			listof: #optional
				binary: @module_add_const, @symbol_create
					operator: '='
					identifier: @symbol_set_name
					$expr: @symbol_set_value
		tree: #optional
			identifier: 'port'
			listof: #optional
				$port: @module_add_port
		tree: #optional
			identifier: 'local'
			listof: #optional
				$local: @module_add_local
		listof: #optional
			$statement: @module_add_comb
			$sync: @module_add_sync
""")
class ParseModule:
	def module_create(stack, ast, delta):
		if delta > 0:
			stack.push(Module())

	def module_add_param(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_param(i)

	def module_add_const(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_const(i)

	def module_add_port(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_port(i)

	def module_add_local(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_local(i)

	def module_add_comb(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_comb(i)

	def module_add_sync(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_sync(i)

@parser("""
port:
	oneof: @symbol_create
		tree: @symbol_set_value
			identifier: @symbol_set_name
			oneof: @array_create
				$inout
				$interface_port_decl: @array_set_value
		tree: @symbol_set_value
			aref:
				identifier: @symbol_set_name
				$expr: @array_create, @array_set_count
			oneof:
				$inout
				$interface_port_decl: @array_set_value

inout:
	oneof:
		$direction: @port_create, @array_set_value
		aref:
			$direction: @port_create, @array_set_value
			$expr: @array_set_width

direction:
	oneof: @port_set_dir
		identifier: 'in'
		identifier: 'out'
		identifier: 'inout'
""")
class ParsePort:
	def port_create(stack, ast, delta):
		if delta > 0:
			stack.push(Port(ast))

	def port_set_dir(stack, ast, delta):
		if delta > 0:
			dir = LOP.LOP_symbol_value(ast).decode()
			if dir != 'inout':
				dir += 'put'
			stack.last.set_dir(dir)

@parser("""
local:
	oneof: @symbol_create
		tree: @symbol_set_value
			identifier: @symbol_set_name
			oneof: @array_create
				$net_simple: @net_create, @array_set_value
				$net_with_width: @net_create, @array_set_value
				$net_with_value: @net_create, @array_set_value
				$fsm: @array_set_value
				$instance: @array_set_value
		tree: @symbol_set_value
			aref:
				identifier: @symbol_set_name
				$expr: @array_create, @array_set_count
			oneof:
				$net_simple: @net_create, @array_set_value
				$net_with_width: @net_create, @array_set_value
				$fsm: @array_set_value
				$instance: @array_set_value
""")
class ParseLocal:
	pass

@parser("""
instance:
	oneof: @instance_create
		identifier: @instance_set_name
		call:
			identifier: @instance_set_name
			listof:
				binary: @symbol_create, @instance_add_param
					operator: '='
					identifier: @symbol_set_name
					$expr: @symbol_set_value
""")
class ParseInstance:
	def instance_create(stack, ast, delta):
		if delta > 0:
			stack.push(Instance(ast))

	def instance_set_name(stack, ast, delta):
		stack.last.set_name(LOP.LOP_symbol_value(ast).decode())

	def instance_add_param(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_param(i)

@parser("""
net_simple:
	identifier: 'net'

net_with_width:
	aref:
		$net_simple
		$expr: @array_set_width2

net_with_value:
	oneof:
		binary:
			operator: '='
			$net_simple
			$expr: @net_set_value
		binary:
			operator: '='
			$net_with_width
			$expr: @net_set_value
""")
class ParseNet:
	def net_create(stack, ast, delta):
		if delta > 0:
			stack.push(Net(ast))

	def net_set_value(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_value(i)

@parser("""
fsm:
	seqof: @fsm_create
		unary:
			operator: '$'
			identifier: 'FSM'
		listof:
			identifier: @fsm_add_state
""")
class ParseFSM:
	def fsm_create(stack, ast, delta):
		if delta > 0:
			stack.push(FSM(ast))

	def fsm_add_state(stack, ast, delta):
		stack.last.add_state(LOP.LOP_symbol_value(ast).decode())

@parser("""
sync:
	tree: @sync_create
		identifier: 'ff'
		listof:
			unary: @sync_cond_create_pos
				operator: '+'
				$expr: @sync_cond_set_expr
			unary: @sync_cond_create_neg
				operator: '-'
				$expr: @sync_cond_set_expr
		listof: #optional
			$statement: @sync_add
""")
class ParseSync:
	def sync_create(stack, ast, delta):
		if delta > 0:
			stack.push(Sync(ast))

	def sync_cond_create_pos(stack, ast, delta):
		if delta > 0:
			stack.push(SyncCond(ast, True))
		else:
			i = stack.pop()
			stack.last.add_cond(i)

	def sync_cond_create_neg(stack, ast, delta):
		if delta > 0:
			stack.push(SyncCond(ast, False))
		else:
			i = stack.pop()
			stack.last.add_cond(i)

	def sync_cond_set_expr(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_cond(i)

	def sync_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add(i)

@parser("""
statement:
	oneof:
		oneof: @comb_create
			$comb_assign: @comb_set
		$comb_if
		$comb_for
		$system
""")
class ParseStatement:
	def comb_create(stack, ast, delta):
		if delta > 0:
			stack.push(Statement(ast))

	def comb_set(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_comb(i)

@parser("""
comb_if:
	seqof: @if_create
		tree:
			identifier: 'if'
			$expr: @if_set_cond
			listof: @if_set_iftrue, @block_create, #optional
				$statement: @block_add
		$else: #optional

else:
	oneof:
		$elif: @if_set_iffalse
		tree:
			identifier: 'else'
			listof: @if_set_iffalse, @block_create, #optional
				$statement: @block_add

elif:
	seqof: @if_create
		tree:
			identifier: 'elif'
			$expr: @if_set_cond
			listof: @if_set_iftrue, @block_create, #optional
				$statement: @block_add
		$else: #optional

expr_if:
	call: @if_create_inline
		identifier: 'mux'
		tree:
			$expr: @if_set_cond
			$expr: @if_set_iftrue
			$expr: @if_set_iffalse
""")
class ParseIf:
	def if_create(stack, ast, delta):
		if delta > 0:
			stack.push(If(ast, False))

	def if_create_inline(stack, ast, delta):
		if delta > 0:
			stack.push(If(ast, True))

	def if_set_cond(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_cond(i)

	def if_set_iftrue(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_iftrue(i)

	def if_set_iffalse(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_iffalse(i)

	def block_create(stack, ast, delta):
		if delta > 0:
			stack.push(Block(ast))

	def block_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add(i)

# FIXME s/expr/term/g
@parser("""
comb_assign:
	binary: @assign_create
		operator: '='
		$expr: @assign_set_lhs
		$expr: @assign_set_rhs
""")
class ParseAssign:
	def assign_create(stack, ast, delta):
		if delta > 0:
			stack.push(Assign(ast))

	def assign_set_lhs(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_lhs(i)

	def assign_set_rhs(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_rhs(i)

@parser("""
comb_for:
	tree: @for_create
		identifier: 'for'
		$expr: @for_set_it
		listof:
			$statement: @for_add

expr_for:
	call: @for_inline_create
		identifier: 'for'
		tree:
			$expr: @for_set_it
			listof:
				$expr: @for_add
""")
class ParseFor:
	def for_create(stack, ast, delta):
		if delta > 0:
			stack.push(For(ast, False))

	def for_inline_create(stack, ast, delta):
		if delta > 0:
			stack.push(For(ast, True))

	def for_set_it(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_it(i)

	def for_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add(i)

@parser("""
bus:
	slist: @bus_create
		listof:
			$expr: @bus_add
""")
class ParseBus:
	def bus_create(stack, ast, delta):
		if delta > 0:
			stack.push(Bus(ast))

	def bus_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add(i)

@parser("""
replicate:
	struct: @replicate_create
		$expr: @replicate_set_number
		listof:
			$expr: @replicate_add
""")
class ParseBus:
	def replicate_create(stack, ast, delta):
		if delta > 0:
			stack.push(Replicate(ast))

	def replicate_set_number(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_number(i)

	def replicate_add(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add(i)

@parser("""
expr:
	oneof:
		identifier: @identifier
		number: @number
		string: @string
		$system
		unary: @unary
			operator: @unary_set_op
			$expr: @unary_set_op1
		binary: @hier
			operator: '.'
			$expr: @hier_set_namespace
			identifier: @hier_set_field
		binary: @binary
			operator: @binary_set_op
			$expr: @binary_set_op1
			$expr: @binary_set_op2
		$bus
		list:
			$expr
		$slice
		$replicate
		$expr_if
		$expr_for
""")
class ParseExpr:
	def identifier(stack, ast, delta):
		stack.push(Identifier(ast, LOP.LOP_symbol_value(ast).decode()))

	def number(stack, ast, delta):
		stack.push(Number(ast, LOP.LOP_symbol_value(ast).decode()))

	def string(stack, ast, delta):
		stack.push(String(ast, LOP.LOP_symbol_value(ast).decode()))

	def unary(stack, ast, delta):
		if delta > 0:
			stack.push(Unary(ast))

	def unary_set_op(stack, ast, delta):
		stack.last.set_op(Operator(ast, LOP.LOP_symbol_value(ast).decode()))

	def unary_set_op1(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_op1(i)

	def binary(stack, ast, delta):
		if delta > 0:
			stack.push(Binary(ast))

	def binary_set_op(stack, ast, delta):
		stack.last.set_op(Operator(ast, LOP.LOP_symbol_value(ast).decode()))

	def binary_set_op1(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_op1(i)

	def binary_set_op2(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_op2(i)

	def hier(stack, ast, delta):
		if delta > 0:
			stack.push(Hier(ast))

	def hier_set_namespace(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_namespace(i)

	def hier_set_field(stack, ast, delta):
		stack.last.set_field(Identifier(ast, LOP.LOP_symbol_value(ast).decode()))

@parser("""
system:
	oneof: @system_create
		call:
			$system_func
			listof:
				$expr: @system_add_arg
		$system_func

system_func:
	unary:
		operator: '$'
		identifier: @system_set_func
""")
class ParseSystem:
	def system_create(stack, ast, delta):
		if delta > 0:
			stack.push(System(ast))

	def system_set_func(stack, ast, delta):
		stack.last.set_func(system[LOP.LOP_symbol_value(ast).decode()])

	def system_add_arg(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.add_arg(i)

@parser("""
slice:
	aref: @slice_create
		$expr: @slice_set_value
		oneof: @slice_set_hilo
			binary: @range_create
				operator: '..'
				$expr: @range_set_hi
				$expr: @range_set_lo
			$expr
""")
class ParseSlice:
	def slice_create(stack, ast, delta):
		if delta > 0:
			stack.push(Slice(ast))

	def slice_set_value(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_value(i)

	def slice_set_hilo(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_hilo(i)

	def range_create(stack, ast, delta):
		if delta > 0:
			stack.push(Range(ast))

	def range_set_hi(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_hi(i)

	def range_set_lo(stack, ast, delta):
		if delta < 0:
			i = stack.pop()
			stack.last.set_lo(i)

class Stack:
	def __init__(self):
		self.stack = []
		self.level = 0

	def push(self, item):
		#print('\t' * self.level, repr(item), sep='')
		self.level += 1

		self.stack.append(item)

	def pop(self):
		ret = self.stack.pop()
		self.level -= 1
		#print('\t' * self.level, '---', sep='')
		return ret

	@property
	def last(self):
		return self.stack[-1]

schema_str = ''.join(SYNTAX)

schema = LOP.LOP_Schema()
schema.filename = LOP.String('schema.lop'.encode())

rc = LOP.LOP_schema_init(schema, schema_str, len(schema_str))

topname = sys.argv[1]
filename = sys.argv[2]

if rc != 0:
	sys.exit(rc)

lop = LOP.LOP()
lop.schema = ctypes.pointer(schema)
lop.top_rule_name = LOP.String('top'.encode())
lop.filename = LOP.String(filename.encode())

src = LOP.map_file(filename)
rc = LOP.LOP_init(lop, src.data, src.len)

if rc != 0:
	sys.exit(rc)

# Top-most scope is template scope
tpl = Scope()
SCOPE.push(tpl)

# Runtime stack
stack = Stack()

for i in range(lop.hl.count):
	node = lop.hl.handler[i].n
	delta = lop.hl.handler[i].delta
	key = str(lop.hl.handler[i].key).split('\t')
	if delta < 0:
		key.reverse()
	for k in key:
		for j in PARSER:
			if hasattr(j, k):
				func = getattr(j, k)
				try:
					func(stack, node, delta)
				except Error as e:
					fatal(e, src)
				break
		else:
			raise Exception(f"Handler '{k}' not found")

try:
	top = SCOPE.lookup(topname).value
	# 1. Constant folding & propagation & bound checking, for-loops unrolling
	#param = Scope()
	#param.add(Param.create('RESET_ADDR', Number(None, 3)))
	top = top.compile()
	# 2. Convert to verilog top and dependant modules
	topv = top.to_verilog(topname)
	# 3. Dump them all
	for p in Formatter.page:
		print(Formatter.page[p].to_verilog(p))
	print(topv)
except Error as e:
	fatal(e, src)

LOP.unmap_file(src)
LOP.LOP_deinit(lop)

LOP.LOP_schema_deinit(schema)
