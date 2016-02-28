USER_TYPES = set()
BASIC_TYPES = set(['bool', 'int', 'float', 'string'])
BINARY_NAMESPACE = None
FRIENDLY_NAMESPACE = None


def snake_to_title(str):
    s = str.split('_')
    return ''.join([x.title() for x in s])


def snake_to_camel(str):
    s = str.split('_')
    return ''.join([x.title() if i > 0 else x for i, x in enumerate(s)])


def make_full_name(prefix, outer=None):
    if not outer:
        return prefix
    return outer.full_name + '::' + prefix


def valid_type(prefix, scope):
    # first do a lookup using the scope, then try without
    full_type = make_full_name(prefix, scope)
    return (
        prefix in BASIC_TYPES or
        full_type in USER_TYPES or
        prefix in USER_TYPES)


def namespace_wrapper(n):
    def fn_wrapper(fn):
        def fn_wrapper_inner(*args, **kwargs):
            res = 'namespace %s\n{\n' % n
            res += fn(*args, **kwargs)
            res += '\n}\n'
            return res
        return fn_wrapper_inner
    return fn_wrapper
