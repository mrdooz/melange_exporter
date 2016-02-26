import sys
import logging
from collections import deque


class Struct(object):
    def __init__(self, name, parent=None):
        self.name = name
        self.parent = parent
        self.vars = []

    def __repr__(self):
        return '%s: %s' % (self.name, self.parent)


class Var(object):
    def __init__(self, var_type, name, array_len=0):
        self.var_type = var_type
        self.name = name
        self.array_len = array_len


def line_gen(f):
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
    to_surround = [';', '(', ')', '[', ']', '{', '}', ',', ':']

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

    def __iter__(self):
        return self


class Parser(object):

    def __init__(self, tokens):
        self.tokens = TokenIterator(tokens)
        self.basic_types = set(['int', 'float', 'string'])
        self.user_types = []
        self.balance = 0

    def valid_type(self, t):
        return t in self.basic_types or t in self.user_types

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

    def parse_struct(self):
        name = self.tokens.next()
        parent = None
        if self.tokens.consume_if(':'):
            parent = self.tokens.next()

        s = Struct(name, parent)

        self.user_types.append(name)
        self.expect('{')

        for token in self.tokens:
            if token == 'struct':
                struct_type = self.tokens.next()
                self.user_types.append(struct_type)
                self.expect('{')
                self.balance += 1
                self.parse(self.balance - 1)
                self.expect(';')
            elif self.valid_type(token):
                var_name = self.tokens.next()
                # check for array
                if self.tokens.peek() == '[':
                    self.tokens.next()
                    count = int(self.tokens.next())
                    self.expect(']')
                self.expect(';')
            elif token == '}':
                self.expect(';')
                return s
        return s

    def parse_var(self):
        var_name = self.tokens.next()
        # check for array
        if self.tokens.peek() == '[':
            self.tokens.next()
            count = int(self.tokens.next())
            self.expect(']')
        self.expect(';')

    def parse(self, return_at_balance=0):
        for token in self.tokens:
            if token == '}':
                self.balance -= 1
                if self.balance == return_at_balance:
                    return
            if token == 'namespace':
                namespace = self.tokens.next()
                self.expect('{')
                self.balance += 1
                self.parse(self.balance - 1)
                self.expect(';')
            elif token == 'struct':
                struct = self.parse_struct()
                print struct

tokens = create_tokens()
p = Parser(tokens)
p.parse()

