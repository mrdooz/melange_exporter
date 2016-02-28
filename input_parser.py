# Parses a format spec, and a compact header file representation (a format that
# can be read in 1 go), and a more easy-to-use header file with stl-structures
# and serialization code

# todo:
# - settings (namespace etc)
# - inner structs
# - enums

import sys
import os
import logging
from collections import deque
import input_parser_common
from input_parser_common import (
    USER_TYPES, make_full_name, valid_type)


class Struct(object):
    def __init__(self, name, parent=None, outer=None):
        self.name = name
        self.full_name = make_full_name(name, outer)
        self.parent = parent
        self.vars = []
        self.children = []

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

    def is_fixed_size(self):
        return self.count != -1


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
            elif valid_type(token, s):
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
    # doing local imports here just because our decorators rely
    # on the namespaces being set, and that's done during parsing..
    from input_parser_binary_hpp import gen_binary_hpp
    from input_parser_friendly_hpp import (
        gen_friendly_hpp, gen_serializer, gen_serializer_decl)

    path, filename = os.path.split(filename)
    filename_base, ext = os.path.splitext(filename)

    binary_hpp = os.path.join(path, filename_base + '.binary.hpp')
    friendly_hpp = os.path.join(path, filename_base + '.friendly.hpp')
    serialize_hpp = os.path.join(path, filename_base + '.serialize.hpp')
    serialize_cpp = os.path.join(path, filename_base + '.serialize.cpp')

    def format_file(filename, s):
        with open(filename, 'wt') as f:
            f.write(s)

        import subprocess
        proc = subprocess.Popen(
            ['clang-format', filename], stdout=subprocess.PIPE)

        res = '\n'.join([x.rstrip() for x in proc.stdout.readlines()])
        with open(filename, 'wt') as f:
            f.write(res)

    format_file(binary_hpp, gen_binary_hpp(p.structs))
    format_file(friendly_hpp, gen_friendly_hpp(p.structs))
    format_file(serialize_hpp, gen_serializer_decl(p.structs))
    format_file(serialize_cpp, gen_serializer(p.structs, friendly_hpp))

filename = sys.argv[1]
p = Parser(filename)
p.parse()

save_output(p, filename)
