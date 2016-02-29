from itertools import chain
import input_parser_common
from input_parser_common import (
    snake_to_camel, BASIC_TYPES)
from string import Template

STRUCT_TEMPLATE_INNER = Template("""struct $name
{
$inner
$vars
};
""")

STRUCT_TEMPLATE = Template("""struct $name
{
$vars
};
""")

FRIENDLY_HPP_TEMPLATE = Template("""#pragma once
namespace $namespace
{
$inner
}""")

SERIALIZE_TEMPLATE = Template("""#include "$rel_path/deferred_writer.hpp"
#include "$friendly_name"

namespace $namespace
{
$inner
}""")

SERIALIZE_DECL_TEMPLATE = Template("""#pragma once
namespace $namespace
{
$inner
}""")


def gen_friendly_hpp(structs):
    res = []
    for s in structs:
        res.extend(_format_struct(s))

    return FRIENDLY_HPP_TEMPLATE.substitute({
        'namespace': input_parser_common.FRIENDLY_NAMESPACE,
        'inner': '\n'.join(res)})


def gen_serializer_decl(structs):
    # return the forward declarations for the serializer code
    funcs, types = [], []
    for s in structs:
        t, f = gen_struct_serializer_decl(s)
        funcs.extend(f)
        types.extend(['struct %s;' % x for x in t])

    return SERIALIZE_DECL_TEMPLATE.substitute({
        'namespace': input_parser_common.FRIENDLY_NAMESPACE,
        'inner': '\n'.join(chain(types, funcs))})


def gen_struct_serializer_decl(s):
    # return forward declarations for types and serializer functions.
    types = [snake_to_camel(s.full_name)]
    funcs = ["void Write%s(const %s& obj, DeferredWriter& writer);" % (
        s.full_name.replace('::', ''), snake_to_camel(s.full_name))]

    for c in s.children:
        child_types, child_funcs = gen_struct_serializer_decl(c)
        types.extend(child_types)
        funcs.extend(child_funcs)

    return types, funcs


def gen_serializer(structs, friendly_name, rel_path):
    res = []
    for s in structs:
        res.extend(gen_struct_serializer(s))

    return SERIALIZE_TEMPLATE.substitute({
        'namespace': input_parser_common.FRIENDLY_NAMESPACE,
        'friendly_name': friendly_name,
        'rel_path': rel_path,
        'inner': ''.join(res)})


def gen_struct_serializer(s):
    # write a serializer for the given struct

    res = ["void Write%s(const %s& obj, DeferredWriter& writer)\n{\n" % (
        s.full_name.replace('::', ''), snake_to_camel(s.full_name))]

    def add_gen(s):
        res.append("writer.Write(obj.%s);\n" % s)

    def add_call(name, var):
        res.append("Write%s(obj.%s, writer);\n" % (name, var))

    var_len = []

    if s.parent:
        res.append("Write%s(obj, writer);\n" % (s.parent))

    for var in s.vars:
        # All variable length vars are collected and written out last
        camel_name = snake_to_camel(var.name)
        if var.is_fixed_size():
            if var.type in BASIC_TYPES:
                add_gen(camel_name)
            else:
                add_call(var.type, camel_name)
        else:
            res.append("int fixup%d = writer.CreateFixup();\n" % len(var_len))
            var_len.append(var)

    # write the variable sized vars
    for i, var in enumerate(var_len):
        # For each var, first write the count, then create a fixup for
        # each element in the array. This is basically implementing an
        # array of pointers
        res.append("\n// %s\n" % var.name)
        res.append("writer.InsertFixup(fixup%d);\n" % i)

        # write # elems
        camel_name = snake_to_camel(var.name)
        res.append("writer.Write((int)obj.%s.size());\n" % camel_name)

        # write the elems
        res.append(
            "for (size_t i = 0; i < obj.%s.size(); ++i)\n{\n" % camel_name)
        res.append("int localFixupId = writer.CreateFixup();\n")
        res.append("writer.InsertFixup(localFixupId);\n")
        res.append("Write%s(obj.%s[i], writer);\n" % (var.type, camel_name))
        res.append("}\n")

    res.append('};\n\n')

    for c in s.children:
        res.extend(gen_struct_serializer(c))

    return ''.join(res)


def _format_struct(s):
    res = []
    inner = ''
    t = STRUCT_TEMPLATE
    if s.children:
        inner = []
        for c in s.children:
            inner.extend(_format_struct(c))
        inner = '\n'.join(inner)
        t = STRUCT_TEMPLATE_INNER

    name = s.name if not s.parent else ('%s : %s' % (s.name, s.parent))

    vars = '\n'.join([_format_var(v) for v in s.vars])
    r = t.substitute(
        {'name': name, 'vars': vars, 'inner': inner})
    res.append(r)

    return res


def _format_var(var):
    res = []

    type_mapping = {
        'string': 'std::string',
        'bytes': 'std::vector<char>',
    }

    mapped_type = type_mapping.get(var.type)

    base_type = mapped_type or var.type
    camel_var = snake_to_camel(var.name)

    if var.count == -1:
        res.append('std::vector<%s> %s;' % (base_type, camel_var))
    else:
        res.append('%s %s;' % (base_type, camel_var))
    return '\n'.join(res)
