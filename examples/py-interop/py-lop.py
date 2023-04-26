import sys
import LOP

class Context:
    def module(self, hl, n, param, cb_arg):
        sys.stdout.write('module ')
        LOP.LOP_handler_eval(hl, 1, None)
        sys.stdout.write('(')
        LOP.LOP_handler_eval(hl, 2, None)
        sys.stdout.write(');\n')
        LOP.LOP_handler_eval(hl, 3, None)
        sys.stdout.write('endmodule')
        return 0
    def str(self, hl, n, param, cb_arg):
        sys.stdout.write(LOP.LOP_symbol_value(n).decode() + ' ')
        return 0
    def port(self, hl, n, param, cb_arg):
        LOP.LOP_handler_eval(hl, 1, None)
        LOP.LOP_handler_eval(hl, 0, None)
        return 0
    def list(self, hl, n, param, cb_arg):
        for i in range(0, hl.count - 1):
            LOP.LOP_handler_eval(hl, i, None)
            sys.stdout.write(', ')
        LOP.LOP_handler_eval(hl, hl.count - 1, None)
        return 0
    def listn(self, hl, n, param, cb_arg):
        for i in range(0, hl.count):
            LOP.LOP_handler_eval(hl, i, None)
            sys.stdout.write(';\n')
        return 0
    def dir(self, hl, n, param, cb_arg):
        sys.stdout.write(LOP.LOP_symbol_value(n).decode() + 'put ')
        return 0
    def dim(self, hl, n, param, cb_arg):
        hi = int(LOP.LOP_symbol_value(n).decode())
        sys.stdout.write('[' + str(hi - 1) + ':0] ')
        return 0
    def bus(self, hl, n, param, cb_arg):
        sys.stdout.write('{ ')
        LOP.LOP_handler_eval(hl, 0, None)
        sys.stdout.write('} ')
        return 0
    def unary(self, hl, n, param, cb_arg):
        LOP.LOP_handler_eval(hl, 0, None)
        LOP.LOP_handler_eval(hl, 1, None)
        return 0
    def binary(self, hl, n, param, cb_arg):
        LOP.LOP_handler_eval(hl, 1, None)
        LOP.LOP_handler_eval(hl, 0, None)
        LOP.LOP_handler_eval(hl, 2, None)
        return 0
    def parens(self, hl, n, param, cb_arg):
        sys.stdout.write('(')
        LOP.LOP_handler_eval(hl, 0, None)
        sys.stdout.write(') ')
        return 0
    def assign(self, hl, n, param, cb_arg):
        sys.stdout.write('assign ')
        self.binary(hl, None, None, None);
        return 0

ctx = Context()

gc_protect = []

@LOP.LOP_resolve_t
def resolve(lop, key, cb):
    func = getattr(ctx, str(key))
    cb[0].func = LOP.LOP_handler_t(func)
    gc_protect.append(cb[0].func)
    return 0

lop = LOP.LOP()
lop.resolve = resolve
lop.error_cb = LOP.LOP_error_cb_t(LOP.LOP_default_error_cb)

schema = LOP.map_file('schema.lop')
rc = LOP.LOP_schema_init(lop, schema.data)
LOP.unmap_file(schema)

if rc == 0:
    src = LOP.map_file('example.lop')
    LOP.LOP_schema_parse_source(None, lop, src.data, 'top')
    LOP.unmap_file(src)

LOP.LOP_schema_deinit(lop)
