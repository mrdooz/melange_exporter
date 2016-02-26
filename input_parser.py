# Parses a format spec, and a compact header file representation (a format that
# can be read in 1 go), and a more easy-to-use header file with stl-structures
# and serialization code

# todo:
# - settings (namespace etc)
# - inner structs
# - enums

import sys
import logging
from collections import deque
from input_parser_templates import STRUCT_TEMPLATE

USER_TYPES = set()
BASIC_TYPES = set(['int', 'float', 'string'])


def snake_to_title(str):
    s = str.split('_')
    return ''.join([x.title() for x in s])


def snake_to_camel(str):
    s = str.split('_')
    return ''.join([x.title() if i > 0 else x for i, x in enumerate(s)])


def valid_type(t):
    return t in BASIC_TYPES or t in USER_TYPES


class Struct(object):
    def __init__(self, name, parent=None, outer=None):
        self.name = name
        self.full_name = (outer.full_name + '::' + name) if outer else name
        self.parent = parent
        self.vars = []
        self.children = []

    def __repr__(self):
        return '%s%s <%r>' % (
            self.name, ' : ' + self.parent if self.parent else '', self.vars)


class Var(object):
    def __init__(self, name, var_type, optional=False, count=None):
        self.name = name
        self.type = var_type
        # Note, -1 denotes a variable length array
        self.count = count
        self.optional = optional

    def __repr__(self):
        return '%s: %s%s%s' % (
            self.name, self.type, '(Optional)' if self.optional else '',
            '[' + str(self.count) + ']' if self.count is not None else '')


def line_gen(f):
    # yields (line_num, line) tuples, stripping out commented and empty lines
    block_comment_depth = 0
    line_num = 0

    def log_err(str):
        logging.warn('%s. Line: %s', str, line_num)

    for r in open(f, 'rt').readlines():
        line_num += 1

        start_block_comment = r.find('/*')

        if start_block_comment != -1:
            r = r[:start_block_comment]
            block_comment_depth += 1

        end_block_comment = r.find('*/')
        if end_block_comment != -1:
            r = r[end_block_comment + 2:]
            block_comment_depth -= 1

        if block_comment_depth:
            continue

        single_line_comment = r.find('//')
        if single_line_comment != -1:
            r = r[:single_line_comment]

        r = r.strip()
        if not r:
            continue

        yield line_num, r


def create_tokens():
    # surround various characters with ' ', so we can use split to tokenize
    to_surround = [';', '(', ')', '[', ']', '{', '}', ',', ':', '?']

    tokens = deque()
    for line_num, line in line_gen(sys.argv[1]):
        for ch in to_surround:
            line = line.replace(ch, ' %s ' % ch)
        s = line.split()
        tokens.extend(zip([line_num] * len(s), s))
    return tokens


class TokenIterator(object):
    def __init__(self, tokens):
        self.tokens = tokens
        self.cur_line = None

    def __iter__(self):
        return self

    def next(self):
        if len(self.tokens) == 0:
            raise StopIteration()
        self.cur_line, token = self.tokens.popleft()
        logging.debug('next: %r, %r', self.cur_line, token)
        return token

    def peek(self):
        return None if self.eof() else self.tokens[0][1]

    def consume_if(self, token):
        if self.peek() == token:
            self.next()
            return True
        return False

    def eof(self):
        return len(self.tokens) == 0

    def push_front(self, token):
        self.tokens.appendleft((self.cur_line, token))


class Parser(object):

    def __init__(self, tokens):
        self.tokens = TokenIterator(tokens)
        self.structs = []

    def err(self, str):
        raise Exception('%s, line: %s' % (str, self.tokens.cur_line))

    def expect(self, expected):
        if not isinstance(expected, list):
            expected = [expected]
        for e in expected:
            token = self.tokens.next()
            if token != e:
                self.err(
                    'Expected "%s" got "%s"' % (expected, token))

    def parse_struct(self, outer=None):
        name = self.tokens.next()
        parent = None
        if self.tokens.consume_if(':'):
            parent = self.tokens.next()

        s = Struct(name, parent, outer)

        USER_TYPES.add(s.full_name)
        self.expect('{')

        for token in self.tokens:
            if token == 'struct':
                child = self.parse_struct(outer=s)
                s.children.append(child)
            elif valid_type(token):
                optional = self.tokens.consume_if('?')
                var_type = token
                while True:
                    var_name = self.tokens.next()
                    count = None
                    # check for array
                    if self.tokens.consume_if('['):
                        if self.tokens.consume_if(']'):
                            # variable length array (denoated by count = -1)
                            count = -1
                        else:
                            count = int(self.tokens.next())
                            self.expect(']')
                    s.vars.append(Var(
                        var_name, var_type, optional=optional, count=count))

                    token = self.tokens.next()
                    if token == ',':
                        # allow multiple vars of the same type on one line
                        continue
                    if token != ';':
                        self.err('Error parsing multiple vars')
                    break
            elif token == '}':
                self.expect(';')
                return s
        return s

    def parse(self):
        for token in self.tokens:
            if token == 'namespace':
                self.namespace = self.tokens.next()
                self.expect('{')
                self.parse()
            elif token == 'struct':
                self.structs.append(self.parse_struct())
            elif token == '}':
                self.expect(';')
                return


def gen_format_hpp(structs):
    res = []
    for s in structs:
        vars = '\n'.join([format_var(v) for v in s.vars])
        r = STRUCT_TEMPLATE.substitute({'name': s.name, 'vars': vars})
        res.append(r)
    return '\n'.join(res)


def format_var(var):
    res = []

    type_mapping = {
        'string': 'const char*',
        'bytes': 'const char*',
    }

    user_type = var.type in USER_TYPES
    mapped_type = type_mapping.get(var.type)

    base_type = mapped_type or var.type
    title_var = snake_to_title(var.name)
    camel_var = snake_to_camel(var.name)

    if var.count == -1:
        # variable length type
        res.append('int num%s;' % title_var)
        res.append('%s* %s;' % (base_type, camel_var))
    else:
        # user types are saved as pointers, because we might not
        # know their size
        if user_type:
            res.append('%s* %s;' % (base_type, camel_var))
        else:
            res.append('%s %s;' % (base_type, camel_var))

    return '\n'.join(res)

tokens = create_tokens()
p = Parser(tokens)
p.parse()

print gen_format_hpp(p.structs)
# print '\n'.join(str(x) for x in p.structs)

