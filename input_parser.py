# Parses a format spec, and a compact header file representation (a format that
# can be read in 1 go), and a more easy-to-use header file with stl-structures
# and serialization code

# todo:
# - enums

import os
import logging
from collections import deque
import argparse
import input_parser_common
from input_parser_common import make_full_name

from input_parser_binary_hpp import gen_binary_hpp
from input_parser_friendly_hpp import (
    gen_friendly_hpp, gen_serializer)

OUTPUT_DIR = None


class Struct(object):
    def __init__(self, name, parent=None, outer=None):
        self.name = name
        self.full_name = make_full_name(name, outer)
        self.parent = parent
        self.vars = []
        self.children = []
        self.enums = []

    def __repr__(self):
        return '%s%s <%r>' % (
            self.full_name,
            ' : ' + self.parent if self.parent else '', self.vars)

    def is_fixed_size(self):
        return (
            all([v.is_fixed_size() for v in self.vars]) and
            all([c.is_fixed_size() for c in self.children])
        )


class Var(object):
    def __init__(
        self, name, var_type, category,
        default_value=None, optional=False, count=None
    ):
        self.name = name
        self.type = var_type
        # Note, -1 denotes a variable length array
        self.count = count
        self.default_value = default_value
        self.optional = optional
        self.category = category

    def __repr__(self):
        return '%s: %s%s%s' % (
            self.name, self.type, '(Optional)' if self.optional else '',
            '[' + str(self.count) + ']' if self.count is not None else '')

    def is_fixed_size(self):
        return self.count != -1


class Enum(object):
    def __init__(self, name, parent, vals=None):
        self.name = name
        self.parent = parent
        self.vals = vals if vals else []


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

    def __init__(self, filename):
        self.tokens = TokenIterator(self.create_tokens(filename))
        self.structs = []
        self.enums = set()
        self.user_types = set()
        self.enums = set()
        self.basic_types = set(['bool', 'int', 'float', 'string'])
        self.binary_namespace = None
        self.friendly_namespace = None

    def line_gen(self, f):
        # yields (line_num, line) tuples, stripping out commented and empty
        # lines
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

    def create_tokens(self, filename):
        # surround various characters with ' ', so we can use split to tokenize
        to_surround = [';', '(', ')', '[', ']', '{', '}', ',', ':', '?']

        tokens = deque()
        for line_num, line in self.line_gen(filename):
            for ch in to_surround:
                line = line.replace(ch, ' %s ' % ch)
            s = line.split()
            tokens.extend(zip([line_num] * len(s), s))
        return tokens

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

    def valid_type(self, prefix, scope):
        # first do a lookup using the scope, then try without
        full_type = make_full_name(prefix, scope)
        return (
            prefix in self.basic_types or
            full_type in self.user_types or
            prefix in self.user_types)

    def parse_enum(self, parent):
        name = self.tokens.next()
        self.expect('{')
        enum_val = 0
        vals = []
        while True:
            token = self.tokens.next()
            if token == '}':
                self.expect(';')
                break
            if self.tokens.consume_if('='):
                enum_val = int(self.tokens.next())
            vals.append((token, enum_val))
            enum_val += 1
            self.tokens.consume_if(',')

        self.user_types.add(name)
        self.enums.add(name)
        parent.enums.append(Enum(name, parent, vals))

    def parse_struct(self, outer=None):
        name = self.tokens.next()
        parent = None
        if self.tokens.consume_if(':'):
            parent = self.tokens.next()

        s = Struct(name, parent, outer)

        self.user_types.add(s.full_name)
        self.expect('{')

        for token in self.tokens:
            if token == 'struct':
                child = self.parse_struct(outer=s)
                s.children.append(child)
            elif self.valid_type(token, s):
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
                    if var_name in self.enums:
                        category = 'enum'
                    elif var_name in self.user_types:
                        category = 'user'
                    else:
                        category = 'basic'

                    token = self.tokens.next()

                    default_value = None
                    if token == '=':
                        default_value = self.tokens.next()
                        token = self.tokens.next()

                    s.vars.append(Var(
                        var_name, var_type, category,
                        default_value=default_value,
                        optional=optional, count=count))

                    if token == ',':
                        # allow multiple vars of the same type on one line
                        continue

                    if token != ';':
                        self.err('Error parsing multiple vars')
                    break
            elif token == '}':
                self.expect(';')
                return s
            elif token == 'enum':
                self.parse_enum(s)
        return s

    def parse_assignment(self):
        self.expect('=')
        res = self.tokens.next()
        self.expect(';')
        return res

    def parse(self):
        for token in self.tokens:
            if token == 'struct':
                self.structs.append(self.parse_struct())
            elif token == '}':
                self.expect(';')
                return
            elif token == 'binary_namespace':
                p = self.parse_assignment()
                input_parser_common.BINARY_NAMESPACE = p
            elif token == 'friendly_namespace':
                p = self.parse_assignment()
                input_parser_common.FRIENDLY_NAMESPACE = p


def save_output(p, filename):
    path, filename = os.path.split(filename)
    base, ext = os.path.splitext(filename)

    if not os.path.exists(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    binary_hpp = os.path.join(OUTPUT_DIR, path, base + '.binary.hpp')
    friendly_hpp = os.path.join(OUTPUT_DIR, path, base + '.friendly.hpp')
    friendly_cpp = os.path.join(OUTPUT_DIR, path, base + '.friendly.cpp')
    # serialize_hpp = os.path.join(OUTPUT_DIR, path, base + '.serialize.hpp')
    # serialize_cpp = os.path.join(OUTPUT_DIR, path, base + '.serialize.cpp')

    def format_file(filename, s):
        with open(filename, 'wt') as f:
            f.write(s)

        import subprocess
        proc = subprocess.Popen(
            ['clang-format', filename], stdout=subprocess.PIPE)

        res = '\n'.join([x.rstrip() for x in proc.stdout.readlines()])
        with open(filename, 'wt') as f:
            f.write(res)

    format_file(binary_hpp, gen_binary_hpp(p.structs, p.user_types))
    format_file(friendly_hpp, gen_friendly_hpp(p.structs))

    friendly_path, friendly_filename = os.path.split(friendly_hpp)
    rel_path = os.path.relpath('.', friendly_path)

    format_file(
        friendly_cpp,
        gen_serializer(p.structs, friendly_filename, rel_path, p.basic_types))

parser = argparse.ArgumentParser()
parser.add_argument('filename')
parser.add_argument('-o', '--out-dir', default='generated')
args = parser.parse_args()
OUTPUT_DIR = args.out_dir

filename = args.filename
p = Parser(filename)
p.parse()

save_output(p, filename)
