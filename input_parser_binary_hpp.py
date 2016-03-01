import input_parser_common
from input_parser_common import (
    snake_to_title, snake_to_camel)
from string import Template

STRUCT_TEMPLATE = Template("""struct $name
{
$inner $enums $vars
};
""")

BINARY_HPP_TEMPLATE = Template("""#pragma once
namespace $namespace
{
$inner
}""")

USER_TYPES = None


def gen_binary_hpp(structs, user_types):
    global USER_TYPES
    USER_TYPES = user_types
    res = []
    for s in structs:
        res.extend(format_struct(s))
    return BINARY_HPP_TEMPLATE.substitute({
        'namespace': input_parser_common.BINARY_NAMESPACE,
        'inner': '\n'.join(res)})


def format_struct(s):
    res = []
    inner = ''
    t = STRUCT_TEMPLATE
    if s.children:
        inner = []
        for c in s.children:
            inner.extend(format_struct(c))
        inner = '\n'.join(inner)

    name = s.name if not s.parent else ('%s : %s' % (s.name, s.parent))

    enums = []
    for enum in s.enums:
        vals = ['%s = %d' % (k, v) for k, v in enum.vals]
        enums.append('enum class %s { %s };\n' % (enum.name, ',\n'.join(vals)))
    enums = ''.join(enums)

    vars = '\n'.join([format_var(v) for v in s.vars])
    r = t.substitute(
        {'name': name, 'enums': enums, 'vars': vars, 'inner': inner})
    res.append(r)

    return res


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
        if var.category in ('basic', 'enum'):
            res.append('%s %s;' % (base_type, camel_var))
        else:
            res.append('%s* %s;' % (base_type, camel_var))

    return '\n'.join(res)
