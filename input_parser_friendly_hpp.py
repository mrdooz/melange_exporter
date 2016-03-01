import input_parser_common
from input_parser_common import snake_to_camel
from string import Template

STRUCT_TEMPLATE = Template("""struct $name
{
$inner $enums $vars
void Write(DeferredWriter& w);
};
""")

FRIENDLY_HPP_TEMPLATE = Template("""#pragma once

class DeferredWriter;

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

BASIC_TYPES = None


def gen_serializer(structs, friendly_name, rel_path, basic_types):
    global BASIC_TYPES
    BASIC_TYPES = basic_types
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

    res = ["void %s::Write(DeferredWriter& writer)\n{\n" % (s.full_name)]

    def write_basic_type(s):
        res.append("writer.Write(%s);\n" % s)

    def add_call(name, var):
        res.append("%s.Write(writer);\n" % (var))

    var_len = []

    if s.parent:
        res.append("%s::Write(writer);\n" % (s.parent))

    for var in s.vars:
        # All variable length vars are collected and written out last
        camel_name = snake_to_camel(var.name)
        if var.is_fixed_size():
            if var.category in ('basic', 'enum'):
                write_basic_type(camel_name)
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
        res.append("writer.Write((int)%s.size());\n" % camel_name)

        # write the elems
        res.append(
            "for (size_t i = 0; i < %s.size(); ++i)\n{\n" % camel_name)
        res.append("int localFixupId = writer.CreateFixup();\n")
        res.append("writer.InsertFixup(localFixupId);\n")
        res.append("%s[i]->Write(writer);\n" % (camel_name))
        res.append("}\n")

    res.append('};\n\n')

    for c in s.children:
        res.extend(gen_struct_serializer(c))

    return ''.join(res)


def gen_friendly_hpp(structs):
    res = []
    for s in structs:
        res.extend(gen_friendly_struct_hpp(s))

    return FRIENDLY_HPP_TEMPLATE.substitute({
        'namespace': input_parser_common.FRIENDLY_NAMESPACE,
        'inner': '\n'.join(res)})


def gen_friendly_struct_hpp(s):
    res = []

    inner = ''
    t = STRUCT_TEMPLATE
    if s.children:
        inner = []
        for c in s.children:
            inner.extend(gen_friendly_struct_hpp(c))
        inner = '\n'.join(inner)

    name = s.name if not s.parent else ('%s : %s' % (s.name, s.parent))

    vars = '\n'.join([gen_friendly_var(v) for v in s.vars])

    enums = []
    for enum in s.enums:
        vals = ['%s = %d' % (k, v) for k, v in enum.vals]
        enums.append('enum class %s { %s };\n' % (enum.name, ',\n'.join(vals)))
    enums = ''.join(enums)
    r = t.substitute(
        {'name': name, 'enums': enums, 'vars': vars, 'inner': inner})
    res.append(r)

    return res


def gen_friendly_var(var):
    res = []

    type_mapping = {
        'string': 'std::string',
        'bytes': 'std::vector<char>',
    }

    mapped_type = type_mapping.get(var.type)

    base_type = mapped_type or var.type
    camel_var = snake_to_camel(var.name)

    if var.count == -1:
        res.append(
            'std::vector<std::shared_ptr<%s>> %s;' % (base_type, camel_var))
    else:
        if var.count is None:
            if var.default_value is not None:
                res.append(
                    '%s %s = %s;' % (base_type, camel_var, var.default_value))
            else:
                res.append('%s %s;' % (base_type, camel_var))
        else:
            res.append('%s %s[%d];' % (base_type, camel_var, var.count))
    return '\n'.join(res)
